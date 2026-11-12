#include "ModInstallPlanner.h"
#include "ModManagerInternal.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QVector>

#include <algorithm>

namespace {

const QString kLogicRootName = QStringLiteral("LogicMods");
const QString kTildeModsName = QStringLiteral("~Mods");
const QString kModsName = QStringLiteral("Mods");

QString cleanPathInput(const QString& raw)
{
    QString path = raw.trimmed();
    if (path.isEmpty())
        return {};
    if (path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive))
        path = QUrl(path).toLocalFile();
    return QDir::cleanPath(path);
}

QString normalizedForCompare(const QString& path)
{
    QString normalized = QDir::cleanPath(path);
    normalized.replace('\\', '/');
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

bool samePath(const QString& left, const QString& right)
{
    return normalizedForCompare(left) == normalizedForCompare(right);
}

QString canonicalModRelativePath(const QString& relativePath, const QString& fromBase,
                                 const QString& toBase)
{
    if (fromBase.isEmpty() || toBase.isEmpty())
        return relativePath;

    const QFileInfo info(relativePath);
    QString fileName = info.fileName();

    QString suffix;
    if (fileName.endsWith(QLatin1String("-x"), Qt::CaseInsensitive)) {
        suffix.prepend(QLatin1String("-x"));
        fileName.chop(2);
    }

    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0)
        return relativePath;
    const QString extension = fileName.mid(dot).toLower();
    if (extension != QLatin1String(".pak") && extension != QLatin1String(".ucas")
        && extension != QLatin1String(".utoc"))
        return relativePath;

    suffix.prepend(fileName.mid(dot));
    fileName.truncate(dot);

    if (fileName.endsWith(QLatin1String("_P"), Qt::CaseInsensitive))
        fileName.chop(2);
    if (fileName.compare(fromBase, Qt::CaseInsensitive) != 0)
        return relativePath;

    QString base = toBase;
    if (base.endsWith(QLatin1String("_P"), Qt::CaseInsensitive))
        base.chop(2);

    const QString renamed = base + QStringLiteral("_P") + suffix;
    const QString dir = info.path();
    return dir == QLatin1String(".") ? renamed : dir + QLatin1Char('/') + renamed;
}

QString leafName(const QString& path)
{
    if (path.isEmpty())
        return {};
    return QFileInfo(path).fileName();
}

QString rootKindForName(const QString& name)
{
    if (name.compare(kLogicRootName, Qt::CaseInsensitive) == 0)
        return QStringLiteral("logic");
    if (name.compare(kModsName, Qt::CaseInsensitive) == 0
        || name.compare(kTildeModsName, Qt::CaseInsensitive) == 0) {
        return QStringLiteral("normal");
    }
    return {};
}

QString canonicalRootName(const QString& name)
{
    if (name.compare(kLogicRootName, Qt::CaseInsensitive) == 0)
        return QStringLiteral("logicmods");
    if (name.compare(kTildeModsName, Qt::CaseInsensitive) == 0)
        return QStringLiteral("~mods");
    if (name.compare(kModsName, Qt::CaseInsensitive) == 0)
        return QStringLiteral("mods");
    return {};
}

QString formattedSize(qint64 size)
{
    if (size < 1024)
        return QStringLiteral("%1 B").arg(size);
    if (size < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(QString::number(size / 1024.0, 'f', 1));
    if (size < 1024 * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));
    return QStringLiteral("%1 GB").arg(QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

QString displayExtension(const QFileInfo& info)
{
    QString suffix = info.suffix();
    if (suffix.endsWith(QStringLiteral("-x"), Qt::CaseInsensitive))
        suffix.chop(2);
    return suffix.toUpper();
}

QString sanitizedFolderName(const QString& raw)
{
    QString name = raw.trimmed();
    static const QRegularExpression invalidChars(QStringLiteral("[\\\\/:*?\"<>|]"));
    name.remove(invalidChars);
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    return name.trimmed();
}

QString sanitizedRelativeFolder(const QString& raw)
{
    QString path = raw.trimmed();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QStringList parts;
    for (const QString& segment : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        const QString clean = sanitizedFolderName(segment);
        if (clean.isEmpty() || clean == QLatin1String(".") || clean == QLatin1String(".."))
            return {};
        parts.append(clean);
    }
    return parts.join(QLatin1Char('/'));
}

QString findChildFolder(const QDir& parent, const QString& name)
{
    const QStringList entries = parent.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        if (entry.compare(name, Qt::CaseInsensitive) == 0)
            return parent.filePath(entry);
    }
    return {};
}

QString resolvePaksRoot(const QString& modsPath, const QString& gamePaksPath, QString* error)
{
    const QString cleanModsPath = cleanPathInput(modsPath);
    const QString cleanGamePaksPath = cleanPathInput(gamePaksPath);

    auto rootFromConfiguredPath = [](const QString& path) -> QString {
        if (path.isEmpty())
            return {};
        const QString leaf = leafName(path);
        if (leaf.compare(QStringLiteral("Paks"), Qt::CaseInsensitive) == 0)
            return QDir::cleanPath(path);
        if (!rootKindForName(leaf).isEmpty()) {
            const QString parent = QFileInfo(path).absolutePath();
            if (leafName(parent).compare(QStringLiteral("Paks"), Qt::CaseInsensitive) == 0)
                return parent;
        }
        return {};
    };

    QString paksRoot = rootFromConfiguredPath(cleanModsPath);
    if (paksRoot.isEmpty())
        paksRoot = rootFromConfiguredPath(cleanGamePaksPath);

    if (paksRoot.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not resolve the game's Paks folder. "
                                    "Set the Mods path to .../Content/Paks/~mods or "
                                    ".../Content/Paks/LogicMods.");
        }
        return {};
    }

    if (!QDir(paksRoot).exists()) {
        if (error) {
            *error = QStringLiteral("The game Paks folder does not exist: %1")
                         .arg(QDir::toNativeSeparators(paksRoot));
        }
        return {};
    }

    return QDir::cleanPath(paksRoot);
}

struct ArchiveRoot {
    QString kind;
    QString canonical;
    QString displayName;
    QString path;
};

struct ScannedRoot {
    ArchiveRoot archive;
    QString destPath;
    QVariantList coreFiles;
    QVariantList companionFiles;
};

QString destinationKey(const QString& destRoot, const QString& relativePath)
{
    if (destRoot.isEmpty())
        return {};
    return normalizedForCompare(destRoot + QLatin1Char('/') + relativePath);
}

QVariantMap makeFileEntry(const QString& filePath, const QString& archiveRootPath,
                          const QString& destRootPath, const QString& rootKind,
                          const QString& sourceRootName)
{
    const QFileInfo info(filePath);
    const QString relativePath = QDir(archiveRootPath).relativeFilePath(filePath);

    QVariantMap entry;
    entry["name"] = info.fileName();
    entry["relativePath"] = relativePath;
    entry["size"] = formattedSize(info.size());
    entry["extension"] = displayExtension(info);
    entry["selected"] = true;
    entry["sourcePath"] = info.absoluteFilePath();
    entry["archiveRootPath"] = QDir::cleanPath(archiveRootPath);
    entry["destRootPath"] = destRootPath;
    entry["rootKind"] = rootKind;
    entry["sourceRootName"] = sourceRootName;
    return entry;
}

QString rootNameForFile(const QVariantMap& file)
{
    const QString destRoot = file.value("destRootPath").toString();
    if (!destRoot.isEmpty())
        return leafName(destRoot);
    return file.value("sourceRootName").toString();
}

struct BuiltGroup {
    QString name;
    QVariantList files;
    QVariantList extraFiles;
    QVector<QVariantMap> roots;
    QString installMode = QStringLiteral("paks");
    QStringList extraWarnings;
};

int findRootIndex(const QVector<QVariantMap>& roots, const QVariantMap& file)
{
    const QString destRoot = file.value("destRootPath").toString();
    if (!destRoot.isEmpty()) {
        for (int i = 0; i < roots.size(); ++i) {
            const QString path = roots.at(i).value("rootPath").toString();
            if (!path.isEmpty() && samePath(path, destRoot))
                return i;
        }
    }

    const QString archiveRoot = file.value("archiveRootPath").toString();
    if (!archiveRoot.isEmpty()) {
        for (int i = 0; i < roots.size(); ++i) {
            if (roots.at(i).value("rootPath").toString().isEmpty()
                && roots.at(i).value("rootKind").toString() == file.value("rootKind").toString()
                && roots.at(i).value("rootName").toString()
                       == file.value("sourceRootName").toString()) {
                return i;
            }
        }
    }
    return -1;
}

void sortRoots(QVector<QVariantMap>& roots)
{
    std::stable_sort(roots.begin(), roots.end(), [](const QVariantMap& left, const QVariantMap& right) {
        const bool leftLogic = left.value("rootKind").toString() == QStringLiteral("logic");
        const bool rightLogic = right.value("rootKind").toString() == QStringLiteral("logic");
        if (leftLogic != rightLogic)
            return leftLogic;
        return false;
    });
}

bool isPathInside(const QString& rootPath, const QString& candidate)
{
    QString root = normalizedForCompare(rootPath);
    const QString path = normalizedForCompare(candidate);
    if (root.isEmpty() || path.isEmpty())
        return false;
    while (root.endsWith(QLatin1Char('/')))
        root.chop(1);
    return path == root || path.startsWith(root + QLatin1Char('/'));
}

const QString kLuaRootName = QStringLiteral("Lua Mods");

QString luaEntryScript(const QDir& modDir)
{
    if (modDir.exists(QStringLiteral("scripts/main.lua")))
        return QStringLiteral("scripts/main.lua");
    if (modDir.exists(QStringLiteral("main.lua")))
        return QStringLiteral("main.lua");
    return {};
}

bool folderHasExtension(const QString& path, const QString& suffix)
{
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (QFileInfo(it.next()).suffix().compare(suffix, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool folderHasUnrealModFiles(const QString& path)
{
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (mmdetail::isUnrealModExtension(QFileInfo(it.next()).suffix()))
            return true;
    }
    return false;
}

bool fileIsInsideAny(const QString& filePath, const QSet<QString>& dirs)
{
    for (const QString& dir : dirs) {
        if (isPathInside(dir, filePath))
            return true;
    }
    return false;
}

bool folderHasFilesOutside(const QString& path, const QSet<QString>& excludedDirs)
{
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (!fileIsInsideAny(it.next(), excludedDirs))
            return true;
    }
    return false;
}

QVariantList collectModFileEntries(const QString& sourceRoot, const QString& destRoot,
                                   const QString& sourceRootName, const QString& kind,
                                   bool skipUnrealModFiles)
{
    QVariantList entries;
    QDirIterator it(sourceRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();

        if (skipUnrealModFiles && mmdetail::isUnrealModExtension(QFileInfo(filePath).suffix()))
            continue;
        entries.append(makeFileEntry(filePath, sourceRoot, destRoot, kind, sourceRootName));
    }
    return entries;
}

struct LuaInstallSpec {
    QString modPath;
    QString name;
    QString supportRoot;
    QStringList supportPaths;
    bool rootLevel = false;
    int skippedDllMods = 0;
};

QVector<LuaInstallSpec> discoverLuaMods(const QString& rootPath,
                                        const QSet<QString>& pakRootPaths,
                                        const QString& fallbackName)
{
    QVector<LuaInstallSpec> specs;

    if (!luaEntryScript(QDir(rootPath)).isEmpty()) {
        LuaInstallSpec spec;
        spec.modPath = rootPath;
        spec.rootLevel = true;
        spec.name = fallbackName.trimmed().isEmpty()
                        ? leafName(rootPath) : fallbackName.trimmed();
        if (spec.name.isEmpty())
            spec.name = QStringLiteral("Lua Mod");
        specs.append(spec);
        return specs;
    }

    QStringList dirs;
    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext())
        dirs.append(QDir::cleanPath(dirIt.next()));

    QSet<QString> qualifying;
    for (const QString& dir : dirs) {
        if (!luaEntryScript(QDir(dir)).isEmpty())
            qualifying.insert(normalizedForCompare(dir));
    }
    if (qualifying.isEmpty())
        return specs;

    QStringList modDirs;
    for (const QString& dir : dirs) {
        if (!qualifying.contains(normalizedForCompare(dir)))
            continue;
        bool ancestorQualifies = false;
        QString parent = QFileInfo(dir).absolutePath();
        while (isPathInside(rootPath, parent)
               && normalizedForCompare(parent) != normalizedForCompare(rootPath)) {
            if (qualifying.contains(normalizedForCompare(parent))) {
                ancestorQualifies = true;
                break;
            }
            parent = QFileInfo(parent).absolutePath();
        }
        if (!ancestorQualifies)
            modDirs.append(dir);
    }

    QMap<QString, QStringList> modsByContainer;
    for (const QString& modDir : modDirs) {
        const QString container = QFileInfo(modDir).absolutePath();

        if (pakRootPaths.contains(normalizedForCompare(container))
            && folderHasUnrealModFiles(container)) {
            continue;
        }
        modsByContainer[container].append(modDir);
    }

    for (auto it = modsByContainer.cbegin(); it != modsByContainer.cend(); ++it) {
        const QString& container = it.key();
        const QStringList& containerMods = it.value();

        QSet<QString> modPaths;
        for (const QString& modDir : containerMods)
            modPaths.insert(normalizedForCompare(modDir));

        QStringList supportPaths;
        int skippedDllMods = 0;
        const QFileInfoList children =
            QDir(container).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& child : children) {
            const QString childPath = QDir::cleanPath(child.absoluteFilePath());
            if (modPaths.contains(normalizedForCompare(childPath)))
                continue;
            if (qualifying.contains(normalizedForCompare(childPath)))
                continue;
            if (folderHasExtension(childPath, QStringLiteral("lua"))) {
                supportPaths.append(childPath);
                continue;
            }
            if (folderHasExtension(childPath, QStringLiteral("dll")))
                ++skippedDllMods;
        }

        bool first = true;
        for (const QString& modDir : containerMods) {
            LuaInstallSpec spec;
            spec.modPath = modDir;
            spec.name = leafName(modDir);
            if (first) {
                spec.supportRoot = container;
                spec.supportPaths = supportPaths;
                spec.skippedDllMods = skippedDllMods;
                first = false;
            }
            specs.append(spec);
        }
    }

    return specs;
}

const QString kCppRootName = QStringLiteral("C++ Mods");

QString cppEntryDll(const QDir& modDir, const QString& name)
{
    if (modDir.exists(QStringLiteral("dlls/main.dll")))
        return QStringLiteral("dlls/main.dll");
    if (!name.isEmpty()
        && modDir.exists(QStringLiteral("dlls/") + name + QStringLiteral(".dll")))
        return QStringLiteral("dlls/") + name + QStringLiteral(".dll");
    if (!name.isEmpty() && modDir.exists(name + QStringLiteral(".dll")))
        return name + QStringLiteral(".dll");
    return {};
}

struct CppInstallSpec {
    QString modPath;
    QString name;
    bool rootLevel = false;
};

QVector<CppInstallSpec> discoverCppMods(const QString& rootPath,
                                        const QSet<QString>& pakRootPaths,
                                        const QString& fallbackName)
{
    QVector<CppInstallSpec> specs;

    if (luaEntryScript(QDir(rootPath)).isEmpty()
        && !cppEntryDll(QDir(rootPath), leafName(rootPath)).isEmpty()) {
        CppInstallSpec spec;
        spec.modPath = rootPath;
        spec.rootLevel = true;
        spec.name = fallbackName.trimmed().isEmpty()
                        ? leafName(rootPath) : fallbackName.trimmed();
        if (spec.name.isEmpty())
            spec.name = QStringLiteral("C++ Mod");
        specs.append(spec);
        return specs;
    }

    QStringList dirs;
    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext())
        dirs.append(QDir::cleanPath(dirIt.next()));

    QSet<QString> qualifying;
    for (const QString& dir : dirs) {
        if (!luaEntryScript(QDir(dir)).isEmpty())
            continue;
        if (!cppEntryDll(QDir(dir), leafName(dir)).isEmpty())
            qualifying.insert(normalizedForCompare(dir));
    }
    if (qualifying.isEmpty())
        return specs;

    for (const QString& dir : dirs) {
        if (!qualifying.contains(normalizedForCompare(dir)))
            continue;
        bool ancestorQualifies = false;
        QString parent = QFileInfo(dir).absolutePath();
        while (isPathInside(rootPath, parent)
               && normalizedForCompare(parent) != normalizedForCompare(rootPath)) {
            if (qualifying.contains(normalizedForCompare(parent))) {
                ancestorQualifies = true;
                break;
            }
            parent = QFileInfo(parent).absolutePath();
        }
        if (ancestorQualifies)
            continue;

        const QString container = QFileInfo(dir).absolutePath();
        if (pakRootPaths.contains(normalizedForCompare(container))
            && folderHasUnrealModFiles(container))
            continue;

        CppInstallSpec spec;
        spec.modPath = dir;
        spec.name = leafName(dir);
        specs.append(spec);
    }

    return specs;
}

}

ModInstallPlanner::Result ModInstallPlanner::analyze(const QString& extractedRoot, const Context& context)
{
    Result result;
    const QString rootPath = QDir::cleanPath(extractedRoot);
    if (!QDir(rootPath).exists()) {
        result.error = QStringLiteral("The extracted archive folder is unavailable.");
        return result;
    }

    QVector<ArchiveRoot> archiveRoots;

    const QString ownLeaf = leafName(rootPath);
    const QString ownKind = rootKindForName(ownLeaf);
    if (!ownKind.isEmpty()) {
        ArchiveRoot self;
        self.kind = ownKind;
        self.canonical = canonicalRootName(ownLeaf);
        self.displayName = ownLeaf;
        self.path = rootPath;
        archiveRoots.append(self);
    }

    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        const QFileInfo info(dirIt.next());
        const QString kind = rootKindForName(info.fileName());
        if (kind.isEmpty())
            continue;

        const QString parentPath = info.absolutePath();
        const bool parentIsPaks = leafName(parentPath).compare(QStringLiteral("Paks"), Qt::CaseInsensitive) == 0;
        const bool parentIsExtractRoot = samePath(parentPath, rootPath);
        if (!parentIsPaks && !parentIsExtractRoot)
            continue;

        ArchiveRoot root;
        root.kind = kind;
        root.canonical = canonicalRootName(info.fileName());
        root.displayName = info.fileName();
        root.path = QDir::cleanPath(info.absoluteFilePath());

        bool duplicate = false;
        for (const ArchiveRoot& existing : archiveRoots) {
            if (samePath(existing.path, root.path)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            archiveRoots.append(root);
    }

    const QString luaModsRoot = cleanPathInput(context.luaModsRoot);
    const QString cppModsRoot = cleanPathInput(context.cppModsRoot);
    QSet<QString> pakRootPaths;
    for (const ArchiveRoot& root : archiveRoots)
        pakRootPaths.insert(normalizedForCompare(root.path));

    QVector<LuaInstallSpec> luaSpecs;
    if (!luaModsRoot.isEmpty())
        luaSpecs = discoverLuaMods(rootPath, pakRootPaths, context.installName);

    QVector<CppInstallSpec> cppSpecs;
    if (!cppModsRoot.isEmpty())
        cppSpecs = discoverCppMods(rootPath, pakRootPaths, context.installName);

    QSet<QString> luaOwnedPaths;
    for (const LuaInstallSpec& spec : luaSpecs) {

        if (!spec.rootLevel)
            luaOwnedPaths.insert(normalizedForCompare(spec.modPath));
        for (const QString& supportPath : spec.supportPaths)
            luaOwnedPaths.insert(normalizedForCompare(supportPath));
    }
    for (const CppInstallSpec& spec : cppSpecs) {
        if (!spec.rootLevel)
            luaOwnedPaths.insert(normalizedForCompare(spec.modPath));
    }

    for (int i = archiveRoots.size() - 1; i >= 0; --i) {
        const ArchiveRoot& candidate = archiveRoots.at(i);
        if (folderHasUnrealModFiles(candidate.path))
            continue;
        if (!folderHasFilesOutside(candidate.path, luaOwnedPaths))
            archiveRoots.removeAt(i);
    }

    if (archiveRoots.isEmpty() && luaSpecs.isEmpty() && cppSpecs.isEmpty())
        return result;

    result.recognized = true;

    bool hasLogicRoots = false;
    bool archiveHasTildeMods = false;
    bool archiveHasMods = false;
    for (const ArchiveRoot& root : archiveRoots) {
        if (root.kind == QStringLiteral("logic"))
            hasLogicRoots = true;
        if (root.kind == QStringLiteral("normal")) {
            if (root.canonical == QStringLiteral("~mods"))
                archiveHasTildeMods = true;
            if (root.canonical == QStringLiteral("mods"))
                archiveHasMods = true;
        }
    }

    const QString modsPath = cleanPathInput(context.modsPath);
    QString error;
    QString paksRoot;
    if (!archiveRoots.isEmpty())
        paksRoot = resolvePaksRoot(modsPath, context.gamePaksPath, &error);
    result.paksRoot = paksRoot;
    result.valid = archiveRoots.isEmpty() ? true : !paksRoot.isEmpty();
    result.error = archiveRoots.isEmpty() ? QString() : error;

    if (!result.valid && !hasLogicRoots && luaSpecs.isEmpty() && cppSpecs.isEmpty()) {
        result.recognized = false;
        result.error.clear();
        return result;
    }

    QDir paksDir(paksRoot);
    QString logicRoot;
    QString normalRoot;
    QString tildeRoot;
    QString modsRoot;

    if (result.valid) {
        const QString activeLeaf = leafName(modsPath);
        const QString activeCanonical = canonicalRootName(activeLeaf);

        if (hasLogicRoots) {
            if (activeCanonical == QStringLiteral("logicmods")) {
                logicRoot = modsPath;
            } else {
                logicRoot = findChildFolder(paksDir, kLogicRootName);
                if (logicRoot.isEmpty())
                    logicRoot = paksDir.filePath(kLogicRootName);
            }
        }

        tildeRoot = findChildFolder(paksDir, kTildeModsName);
        if (tildeRoot.isEmpty())
            tildeRoot = paksDir.filePath(kTildeModsName);
        modsRoot = findChildFolder(paksDir, kModsName);
        if (modsRoot.isEmpty())
            modsRoot = paksDir.filePath(kModsName);

        if (activeCanonical == QStringLiteral("~mods")) {
            tildeRoot = modsPath;
        } else if (activeCanonical == QStringLiteral("mods")) {
            modsRoot = modsPath;
        }

        if (activeCanonical == QStringLiteral("~mods") || activeCanonical == QStringLiteral("mods")) {
            normalRoot = modsPath;
        } else {
            normalRoot = findChildFolder(paksDir, kTildeModsName);
            if (normalRoot.isEmpty())
                normalRoot = findChildFolder(paksDir, kModsName);
            if (normalRoot.isEmpty()) {
                QString preferredName;
                if (archiveHasTildeMods)
                    preferredName = kTildeModsName;
                else if (archiveHasMods)
                    preferredName = kModsName;
                if (preferredName.isEmpty())
                    preferredName = kTildeModsName;
                normalRoot = paksDir.filePath(preferredName);
            }
        }
    }

    result.logicRoot = logicRoot;
    result.normalRoot = normalRoot;

    if (!result.valid) {
        if (result.error.isEmpty())
            result.error = QStringLiteral("The game's Paks folder could not be resolved.");
    }

    QVector<ScannedRoot> scans;
    QHash<QString, QString> seenDestinations;
    int duplicateFilesSkipped = 0;

    auto scanRoot = [&](const ArchiveRoot& archiveRoot, const QString& destPath) {
        ScannedRoot scan;
        scan.archive = archiveRoot;
        scan.destPath = destPath;

        QDirIterator fileIt(archiveRoot.path, QDir::Files, QDirIterator::Subdirectories);
        while (fileIt.hasNext()) {
            const QString filePath = fileIt.next();
            const QFileInfo info(filePath);
            const QString relativePath = QDir(archiveRoot.path).relativeFilePath(filePath);
            const QString key = destinationKey(destPath, relativePath);
            if (!key.isEmpty()) {
                if (seenDestinations.contains(key)) {
                    ++duplicateFilesSkipped;
                    continue;
                }
                seenDestinations.insert(key, archiveRoot.path);
            }

            const QVariantMap entry = makeFileEntry(filePath, archiveRoot.path, destPath,
                                                    archiveRoot.kind, archiveRoot.displayName);
            if (mmdetail::isUnrealModExtension(info.suffix()))
                scan.coreFiles.append(entry);
            else
                scan.companionFiles.append(entry);
        }
        scans.append(scan);
    };

    for (const ArchiveRoot& archiveRoot : archiveRoots) {
        if (archiveRoot.kind == QStringLiteral("logic"))
            scanRoot(archiveRoot, logicRoot);
        else if (archiveRoot.canonical == QStringLiteral("~mods"))
            scanRoot(archiveRoot, tildeRoot);
        else if (archiveRoot.canonical == QStringLiteral("mods"))
            scanRoot(archiveRoot, modsRoot);
        else
            scanRoot(archiveRoot, normalRoot);
    }

    {
        ArchiveRoot looseRoot;
        looseRoot.kind = QStringLiteral("normal");
        looseRoot.canonical = QStringLiteral("root");
        looseRoot.displayName = QStringLiteral("Paks root");
        looseRoot.path = rootPath;

        ScannedRoot looseScan;
        looseScan.archive = looseRoot;
        looseScan.destPath = normalRoot;

        QDirIterator looseIt(rootPath, QDir::Files, QDirIterator::NoIteratorFlags);
        while (looseIt.hasNext()) {
            const QFileInfo info(looseIt.next());
            if (!mmdetail::isUnrealModExtension(info.suffix()))
                continue;

            if (!info.completeBaseName().endsWith(QStringLiteral("_P"), Qt::CaseInsensitive))
                continue;

            const QString relativePath = info.fileName();
            const QString key = destinationKey(normalRoot, relativePath);
            if (!key.isEmpty() && seenDestinations.contains(key)) {
                ++duplicateFilesSkipped;
                continue;
            }
            if (!key.isEmpty())
                seenDestinations.insert(key, rootPath);

            looseScan.coreFiles.append(makeFileEntry(info.absoluteFilePath(), rootPath, normalRoot,
                                                     looseRoot.kind, looseRoot.displayName));
        }

        if (!looseScan.coreFiles.isEmpty())
            scans.append(looseScan);
    }

    QMap<QString, QVariantList> coreByBase;
    for (const ScannedRoot& scan : scans) {
        for (const QVariant& value : scan.coreFiles) {
            const QVariantMap file = value.toMap();
            const QString baseName = mmdetail::normalizedModBaseName(QFileInfo(file.value("name").toString()));
            if (!baseName.isEmpty())
                coreByBase[baseName].append(file);
        }
    }

    if (coreByBase.isEmpty() && luaSpecs.isEmpty() && cppSpecs.isEmpty()) {
        result.valid = false;
        result.error = QStringLiteral("No Unreal Engine mod files (.pak, .ucas, .utoc) were found in the archive.");
        return result;
    }

    const QStringList baseNames = coreByBase.keys();
    const QMap<QString, QStringList> prefixGroups = mmdetail::groupByCommonPrefix(baseNames);

    QVector<BuiltGroup> builtGroups;
    for (auto groupIt = prefixGroups.cbegin(); groupIt != prefixGroups.cend(); ++groupIt) {
        BuiltGroup group;
        group.name = groupIt.key();

        for (const QString& baseName : groupIt.value()) {
            const QVariantList files = coreByBase.value(baseName);
            for (const QVariant& value : files) {
                const QVariantMap file = value.toMap();
                group.files.append(file);

                int rootIndex = findRootIndex(group.roots, file);
                if (rootIndex < 0) {
                    QVariantMap root;
                    root["rootName"] = rootNameForFile(file);
                    root["rootKind"] = file.value("rootKind");
                    root["rootPath"] = file.value("destRootPath");
                    const QString path = root.value("rootPath").toString();
                    root["create"] = !path.isEmpty() && !QDir(path).exists();
                    root["coreFiles"] = QVariantList();
                    root["companionFiles"] = QVariantList();
                    group.roots.append(root);
                    rootIndex = group.roots.size() - 1;
                }

                QVariantMap root = group.roots.at(rootIndex);
                QVariantList rootFiles = root.value("coreFiles").toList();
                rootFiles.append(file);
                root["coreFiles"] = rootFiles;
                group.roots[rootIndex] = root;
            }
        }

        if (group.name.isEmpty() || group.files.isEmpty())
            continue;
        builtGroups.append(group);
    }

    if (builtGroups.isEmpty() && luaSpecs.isEmpty() && cppSpecs.isEmpty()) {
        result.valid = false;
        result.error = QStringLiteral("No installable mod groups were found in the archive.");
        return result;
    }

    for (const ScannedRoot& scan : scans) {
        if (scan.companionFiles.isEmpty() || builtGroups.isEmpty())
            continue;

        int targetGroup = -1;
        for (int i = 0; i < builtGroups.size() && targetGroup < 0; ++i) {
            for (const QVariant& value : builtGroups.at(i).files) {
                const QVariantMap file = value.toMap();
                if (samePath(file.value("archiveRootPath").toString(), scan.archive.path)) {
                    targetGroup = i;
                    break;
                }
            }
        }

        if (targetGroup < 0 && !scan.destPath.isEmpty()) {
            for (int i = 0; i < builtGroups.size() && targetGroup < 0; ++i) {
                for (const QVariantMap& root : builtGroups.at(i).roots) {
                    if (samePath(root.value("rootPath").toString(), scan.destPath)) {
                        targetGroup = i;
                        break;
                    }
                }
            }
        }

        if (targetGroup < 0)
            targetGroup = 0;

        QVariantMap destRoot;
        destRoot["rootName"] = scan.destPath.isEmpty() ? scan.archive.displayName : leafName(scan.destPath);
        destRoot["rootKind"] = scan.archive.kind;
        destRoot["rootPath"] = scan.destPath;
        destRoot["create"] = !scan.destPath.isEmpty() && !QDir(scan.destPath).exists();
        destRoot["coreFiles"] = QVariantList();
        destRoot["companionFiles"] = QVariantList();

        int rootIndex = -1;
        for (int i = 0; i < builtGroups[targetGroup].roots.size(); ++i) {
            const QVariantMap root = builtGroups[targetGroup].roots.at(i);
            if ((!scan.destPath.isEmpty() && samePath(root.value("rootPath").toString(), scan.destPath))
                || (scan.destPath.isEmpty()
                    && root.value("rootName").toString() == scan.archive.displayName
                    && root.value("rootKind").toString() == scan.archive.kind)) {
                rootIndex = i;
                break;
            }
        }

        if (rootIndex < 0) {
            builtGroups[targetGroup].roots.append(destRoot);
            rootIndex = builtGroups[targetGroup].roots.size() - 1;
        }

        QVariantMap root = builtGroups[targetGroup].roots.at(rootIndex);
        QVariantList companions = root.value("companionFiles").toList();
        for (const QVariant& value : scan.companionFiles)
            companions.append(value);
        root["companionFiles"] = companions;
        builtGroups[targetGroup].roots[rootIndex] = root;

        QVariantList extras = builtGroups[targetGroup].extraFiles;
        for (const QVariant& value : scan.companionFiles)
            extras.append(value);
        builtGroups[targetGroup].extraFiles = extras;
    }

    for (const LuaInstallSpec& spec : luaSpecs) {
        int targetGroup = -1;
        for (int i = 0; i < builtGroups.size(); ++i) {
            if (builtGroups.at(i).name.compare(spec.name, Qt::CaseInsensitive) == 0) {
                targetGroup = i;
                break;
            }
        }
        if (targetGroup < 0) {
            BuiltGroup group;
            group.name = spec.name;
            group.installMode = QStringLiteral("lua");
            builtGroups.append(group);
            targetGroup = builtGroups.size() - 1;
        }

        QVariantMap root;
        root["rootName"] = kLuaRootName;
        root["rootKind"] = QStringLiteral("lua");
        root["rootPath"] = luaModsRoot;
        root["create"] = !QDir(luaModsRoot).exists();
        root["wrapInFolder"] = true;

        const QVariantList core = collectModFileEntries(spec.modPath, luaModsRoot, spec.name,
                                                        QStringLiteral("lua"), spec.rootLevel);
        QVariantList companions;
        for (const QString& supportPath : spec.supportPaths) {
            QDirIterator it(supportPath, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                companions.append(makeFileEntry(it.next(), spec.supportRoot, luaModsRoot,
                                                QStringLiteral("lua"), kLuaRootName));
            }
        }
        root["coreFiles"] = core;
        root["companionFiles"] = companions;

        builtGroups[targetGroup].roots.append(root);
        for (const QVariant& value : core)
            builtGroups[targetGroup].files.append(value);
        for (const QVariant& value : companions)
            builtGroups[targetGroup].extraFiles.append(value);
        if (spec.skippedDllMods > 0 && cppModsRoot.isEmpty()) {
            builtGroups[targetGroup].extraWarnings.append(
                QStringLiteral("%1 C++ (DLL) mod folder(s) were not installed: the Lua runtime loads script mods only.")
                    .arg(spec.skippedDllMods));
        }
    }

    for (const CppInstallSpec& spec : cppSpecs) {
        int targetGroup = -1;
        for (int i = 0; i < builtGroups.size(); ++i) {
            if (builtGroups.at(i).name.compare(spec.name, Qt::CaseInsensitive) == 0) {
                targetGroup = i;
                break;
            }
        }
        if (targetGroup < 0) {
            BuiltGroup group;
            group.name = spec.name;
            group.installMode = QStringLiteral("cpp");
            builtGroups.append(group);
            targetGroup = builtGroups.size() - 1;
        }

        QVariantMap root;
        root["rootName"] = kCppRootName;
        root["rootKind"] = QStringLiteral("cpp");
        root["rootPath"] = cppModsRoot;
        root["create"] = !QDir(cppModsRoot).exists();
        root["wrapInFolder"] = true;

        const QVariantList core = collectModFileEntries(spec.modPath, cppModsRoot, spec.name,
                                                        QStringLiteral("cpp"), false);
        root["coreFiles"] = core;
        root["companionFiles"] = QVariantList();

        builtGroups[targetGroup].roots.append(root);
        for (const QVariant& value : core)
            builtGroups[targetGroup].files.append(value);
    }

    QStringList globalWarnings;
    if (duplicateFilesSkipped > 0) {
        globalWarnings.append(QStringLiteral("Skipped %1 duplicate file(s) targeting the same destination.")
                                  .arg(duplicateFilesSkipped));
    }

    result.groups.clear();
    for (BuiltGroup& group : builtGroups) {
        for (QVariantMap& root : group.roots) {

            const QString rootKind = root.value("rootKind").toString();
            if (rootKind == QStringLiteral("lua") || rootKind == QStringLiteral("cpp"))
                continue;
            const QVariantList core = root.value("coreFiles").toList();
            bool wrapInFolder = !core.isEmpty();
            for (const QVariant& value : core) {
                const QString relative = value.toMap().value("relativePath").toString();
                if (relative.contains(QLatin1Char('/')) || relative.contains(QLatin1Char('\\'))) {
                    wrapInFolder = false;
                    break;
                }
            }
            root["wrapInFolder"] = wrapInFolder;
        }

        sortRoots(group.roots);

        int existingFiles = 0;
        for (const QVariantMap& root : group.roots) {
            const QString destRoot = root.value("rootPath").toString();
            if (destRoot.isEmpty())
                continue;

            QString destBase = destRoot;
            if (root.value("wrapInFolder").toBool()) {
                const QString plannedFolder = sanitizedFolderName(group.name);
                if (!plannedFolder.isEmpty())
                    destBase = QDir(destRoot).filePath(plannedFolder);
            }

            const QVariantList core = root.value("coreFiles").toList();
            const QVariantList companions = root.value("companionFiles").toList();
            for (const QVariant& value : core) {
                const QVariantMap file = value.toMap();
                const QString destination = QDir(destBase).filePath(file.value("relativePath").toString());
                if (!samePath(file.value("sourcePath").toString(), destination)
                    && QFileInfo::exists(destination)) {
                    ++existingFiles;
                }
            }
            for (const QVariant& value : companions) {
                const QVariantMap file = value.toMap();
                const QString destination = QDir(destBase).filePath(file.value("relativePath").toString());
                if (!samePath(file.value("sourcePath").toString(), destination)
                    && QFileInfo::exists(destination)) {
                    ++existingFiles;
                }
            }
        }

        QStringList warnings = globalWarnings;
        warnings.append(group.extraWarnings);
        if (existingFiles > 0)
            warnings.append(QStringLiteral("%1 existing file(s) will be replaced.").arg(existingFiles));

        QVariantList rootList;
        for (const QVariantMap& root : group.roots)
            rootList.append(root);

        QVariantList warningList;
        for (const QString& warning : warnings)
            warningList.append(warning);

        QVariantMap groupMap;
        groupMap["originalName"] = group.name;
        groupMap["newName"] = group.name;
        groupMap["files"] = group.files;
        groupMap["extraFiles"] = group.extraFiles;
        groupMap["installMode"] = group.installMode;
        groupMap["installRoots"] = rootList;
        groupMap["warnings"] = warningList;
        groupMap["installValid"] = result.valid;
        groupMap["installError"] = result.valid ? QString() : result.error;
        groupMap["paksRoot"] = result.paksRoot;
        result.groups.append(groupMap);
    }

    return result;
}

bool ModInstallPlanner::installGroup(const QVariantMap& group, const QStringList& selectedFiles,
                                     const QString& folderName, const QString& bundleFolder,
                                     int* copiedFiles, QString* error)
{
    if (copiedFiles)
        *copiedFiles = 0;

    QString effectiveFolder = sanitizedFolderName(folderName);
    if (effectiveFolder.isEmpty())
        effectiveFolder = sanitizedFolderName(group.value("originalName").toString());

    const QString bundlePrefix = sanitizedRelativeFolder(bundleFolder);
    if (!bundleFolder.trimmed().isEmpty() && bundlePrefix.isEmpty()) {
        if (error)
            *error = QStringLiteral("Invalid bundle folder name.");
        return false;
    }

    QSet<QString> selected;
    for (const QString& name : selectedFiles)
        selected.insert(name.toLower());

    QStringList failures;
    int copied = 0;

    const QVariantList roots = group.value("installRoots").toList();
    for (const QVariant& rootValue : roots) {
        const QVariantMap root = rootValue.toMap();
        const QString destRoot = root.value("rootPath").toString();
        if (destRoot.isEmpty()) {
            failures.append(QStringLiteral("Destination for %1 is unavailable.")
                                .arg(root.value("rootName").toString()));
            continue;
        }

        const QString rootKind = root.value("rootKind").toString();
        const bool isProfileRoot = rootKind == QStringLiteral("lua") || rootKind == QStringLiteral("cpp");
        QString destBase = destRoot;

        if (!bundlePrefix.isEmpty() && !isProfileRoot)
            destBase = QDir(destBase).filePath(bundlePrefix);
        if (root.value("wrapInFolder").toBool() && !effectiveFolder.isEmpty())
            destBase = QDir(destBase).filePath(effectiveFolder);

        auto copyFiles = [&](const QVariantList& files, bool respectSelection, bool canonicalize) {
            for (const QVariant& value : files) {
                const QVariantMap file = value.toMap();
                const QString fileName = file.value("name").toString();
                if (respectSelection && !selected.isEmpty() && !selected.contains(fileName.toLower()))
                    continue;

                const QString sourcePath = file.value("sourcePath").toString();
                QString relativePath = file.value("relativePath").toString();
                if (canonicalize) {
                    relativePath = canonicalModRelativePath(
                        relativePath, group.value("originalName").toString(), effectiveFolder);
                }
                if (sourcePath.isEmpty() || relativePath.isEmpty()) {
                    failures.append(QStringLiteral("Missing source data for %1.").arg(fileName));
                    continue;
                }

                const QString destination = QDir::cleanPath(QDir(destBase).filePath(relativePath));
                if (!isPathInside(destRoot, destination)) {
                    failures.append(QStringLiteral("Refused to copy %1 outside %2.").arg(fileName, destRoot));
                    continue;
                }

                if (samePath(sourcePath, destination))
                    continue;

                if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
                    failures.append(QStringLiteral("Could not create folder for %1.").arg(fileName));
                    continue;
                }

                if (QFile::exists(destination) && !QFile::remove(destination)) {
                    failures.append(QStringLiteral("Could not replace %1.").arg(destination));
                    continue;
                }

                if (QFile::copy(sourcePath, destination)) {
                    ++copied;
                } else {
                    failures.append(QStringLiteral("Failed to copy %1 to %2.").arg(fileName, destBase));
                }
            }
        };

        copyFiles(root.value("coreFiles").toList(), true, !isProfileRoot);
        copyFiles(root.value("companionFiles").toList(), false, false);
    }

    if (copiedFiles)
        *copiedFiles = copied;

    if (!failures.isEmpty()) {
        if (error)
            *error = failures.join(QStringLiteral("\n"));
        return false;
    }
    return true;
}
