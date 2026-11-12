#include "ModManagerInternal.h"
#include "ModInstallPlanner.h"

#include <QHash>
#include <QRegularExpression>

using namespace mmdetail;

namespace {

bool planHasPakGroups(const ModInstallPlanner::Result& plan)
{
    for (const QVariant& value : plan.groups) {
        if (value.toMap().value("installMode").toString() == QStringLiteral("paks"))
            return true;
    }
    return false;
}

bool groupHasLuaRoot(const QVariantMap& group)
{
    for (const QVariant& value : group.value("installRoots").toList()) {
        if (value.toMap().value("rootKind").toString() == QStringLiteral("lua"))
            return true;
    }
    return false;
}

bool groupHasCppRoot(const QVariantMap& group)
{
    for (const QVariant& value : group.value("installRoots").toList()) {
        if (value.toMap().value("rootKind").toString() == QStringLiteral("cpp"))
            return true;
    }
    return false;
}

QSet<QString> modExtensionsInGroup(const QVariantMap& group)
{
    QSet<QString> extensions;
    const auto addName = [&extensions](const QString& fileName) {
        QString extension = QFileInfo(fileName).suffix().toLower();
        if (extension.endsWith(QLatin1String("-x")))
            extension.chop(2);
        if (extension == QLatin1String("pak") || extension == QLatin1String("ucas")
            || extension == QLatin1String("utoc"))
            extensions.insert(extension);
    };

    for (const QVariant& value : group.value("files").toList())
        addName(value.toMap().value("name").toString());
    for (const QVariant& rootValue : group.value("installRoots").toList()) {
        for (const QVariant& fileValue : rootValue.toMap().value("coreFiles").toList())
            addName(fileValue.toMap().value("name").toString());
    }
    return extensions;
}

bool copyFileReplacing(const QString& sourcePath, const QString& destPath)
{
    const QString source = QDir::cleanPath(QFileInfo(sourcePath).absoluteFilePath());
    const QString dest = QDir::cleanPath(QFileInfo(destPath).absoluteFilePath());
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
    if (source.compare(dest, pathCase) == 0)
        return true;

    if (QFileInfo::exists(destPath) && !QFile::remove(destPath))
        return false;
    return QFile::copy(sourcePath, destPath);
}

QString sanitizedBundleFolder(const QString& raw)
{
    QString name = raw.trimmed();
    static const QRegularExpression invalidChars(QStringLiteral("[\\\\/:*?\"<>|\\r\\n\\t]"));
    name.remove(invalidChars);
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' ')))
        name.chop(1);
    if (name == QLatin1String(".") || name == QLatin1String(".."))
        return {};
    return name.trimmed();
}

bool groupWrapsInFolder(const QVariantMap& group)
{
    for (const QVariant& rootValue : group.value("installRoots").toList()) {
        if (rootValue.toMap().value("wrapInFolder").toBool())
            return true;
    }
    return false;
}

struct PlannerFolders {
    QString leaf;
    QString prefix;
};

PlannerFolders plannerFoldersFor(const QVariantMap& group, const QString& installName,
                                 const QString& relativeFolder, const QStringList& bundleFolders)
{
    PlannerFolders folders;
    folders.leaf = installName;

    for (const QString& bundle : bundleFolders) {
        if (relativeFolder.compare(bundle, Qt::CaseInsensitive) == 0
            || relativeFolder.startsWith(bundle + QLatin1Char('/'), Qt::CaseInsensitive)) {
            folders.prefix = bundle;
            break;
        }
    }

    if (folders.prefix.isEmpty() && !relativeFolder.isEmpty()
        && groupWrapsInFolder(group)) {
        const QFileInfo info(relativeFolder);
        folders.leaf = info.fileName();
        const QString parent = info.path();
        if (parent != QLatin1String("."))
            folders.prefix = parent;
    }

    return folders;
}
}

int ModManager::lastInstalledModCount() const
{
    return m_lastInstalledModCount;
}

void ModManager::backupModsAsync(const QStringList& modNames) {
    (void)QtConcurrent::run([this, modNames]() {
        this->backupMods(modNames);
    });
}

void ModManager::backupMods(const QStringList& modNames) {
    if (m_backupPath.isEmpty()) {
        emit backupComplete(false, 0, modNames.count());
        return;
    }

    if (m_modsPathInternal.isEmpty()) {
        emit backupComplete(false, 0, modNames.count());
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString backupDir = m_backupPath + "/Backup_" + timestamp;

    QDir dir;
    if (!dir.mkpath(backupDir)) {
        emit backupComplete(false, 0, modNames.count());
        return;
    }

    int successCount = 0;
    int current = 0;
    QStringList backedUpMods;
    QStringList failedMods;
    qint64 totalBackupSize = 0;

    QDir modsRoot(m_modsPathInternal);

    for (const QString& modName : modNames) {
        current++;
        emit backupProgress(current, modNames.count(), modName);

        bool modBackedUp = false;
        qint64 modSize = 0;

        QString modFolderPath = modsRoot.filePath(modName);
        if (QDir(modFolderPath).exists()) {
            QString destPath = backupDir + "/" + modName;
            if (copyRecursively(modFolderPath, destPath)) {
                successCount++;
                modBackedUp = true;

                QDirIterator it(modFolderPath, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    it.next();
                    modSize += QFileInfo(it.filePath()).size();
                }
            }
        } else {
            QStringList extensions = {"pak", "pak-x", "ucas", "ucas-x", "utoc", "utoc-x"};
            QStringList filesToBackup;

            for (const QString& ext : extensions) {
                QString fileName = modName + "." + ext;
                QString filePath = modsRoot.filePath(fileName);
                if (QFileInfo::exists(filePath)) {
                    filesToBackup.append(filePath);
                }

                QString fileNameP = modName + "_P." + ext;
                QString filePathP = modsRoot.filePath(fileNameP);
                if (QFileInfo::exists(filePathP)) {
                    filesToBackup.append(filePathP);
                }
            }

            if (!filesToBackup.isEmpty()) {

                QString looseModFolder = backupDir + "/" + modName;
                QDir().mkpath(looseModFolder);

                bool allCopied = true;
                for (const QString& srcFile : filesToBackup) {
                    QString fileName = QFileInfo(srcFile).fileName();
                    QString destFile = looseModFolder + "/" + fileName;
                    if (!QFile::copy(srcFile, destFile)) {
                        allCopied = false;
                    } else {
                        modSize += QFileInfo(srcFile).size();
                    }
                }

                if (allCopied) {
                    successCount++;
                    modBackedUp = true;
                }
            }
        }

        if (modBackedUp) {
            backedUpMods.append(modName);
            totalBackupSize += modSize;
        } else {
            failedMods.append(modName);
        }
    }

    QString infoFilePath = backupDir + "/backup_info.txt";
    QFile infoFile(infoFilePath);
    if (infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&infoFile);
        out.setEncoding(QStringConverter::Utf8);

        out << "===============================================" << Qt::endl;
        out << "      Universal Manager Backup Info       " << Qt::endl;
        out << "===============================================" << Qt::endl << Qt::endl;

        out << "Backup Date: " << QDateTime::currentDateTime().toString("dddd, MMMM d, yyyy") << Qt::endl;
        out << "Backup Time: " << QDateTime::currentDateTime().toString("hh:mm:ss AP") << Qt::endl;
        out << "Backup Location: " << backupDir << Qt::endl << Qt::endl;

        out << "-----------------------------------------------" << Qt::endl;
        out << "BACKUP SUMMARY" << Qt::endl;
        out << "-----------------------------------------------" << Qt::endl;
        out << "Total Mods Selected: " << modNames.count() << Qt::endl;
        out << "Successfully Backed Up: " << successCount << Qt::endl;
        out << "Failed: " << failedMods.count() << Qt::endl;
        out << "Total Backup Size: " << formatBytes(totalBackupSize) << Qt::endl << Qt::endl;

        if (!backedUpMods.isEmpty()) {
            out << "-----------------------------------------------" << Qt::endl;
            out << "BACKED UP MODS (" << backedUpMods.count() << ")" << Qt::endl;
            out << "-----------------------------------------------" << Qt::endl;
            for (int i = 0; i < backedUpMods.count(); i++) {
                out << (i + 1) << ". " << backedUpMods[i] << Qt::endl;
            }
            out << Qt::endl;
        }

        if (!failedMods.isEmpty()) {
            out << "-----------------------------------------------" << Qt::endl;
            out << "FAILED MODS (" << failedMods.count() << ")" << Qt::endl;
            out << "-----------------------------------------------" << Qt::endl;
            for (int i = 0; i < failedMods.count(); i++) {
                out << (i + 1) << ". " << failedMods[i] << Qt::endl;
            }
            out << Qt::endl;
        }

        out << "-----------------------------------------------" << Qt::endl;
        out << "NOTES" << Qt::endl;
        out << "-----------------------------------------------" << Qt::endl;
        out << "This backup was created using Universal Manager." << Qt::endl;
        out << "To restore these mods, copy them back to your" << Qt::endl;
        out << "mods directory: " << m_modsPathInternal << Qt::endl << Qt::endl;

        out << "===============================================" << Qt::endl;

        out.flush();
        infoFile.flush();
        infoFile.close();

        qDebug() << "Backup info file created at:" << infoFilePath;
    } else {
        qDebug() << "Failed to create backup info file at:" << infoFilePath;
        qDebug() << "Error:" << infoFile.errorString();
    }

    emit backupComplete(true, successCount, modNames.count());
}

bool ModManager::copyRecursively(const QString& srcPath, const QString& dstPath) {
    QFileInfo srcInfo(srcPath);

    if (srcInfo.isDir()) {
        QDir dstDir(dstPath);
        if (!dstDir.mkpath(".")) {
            return false;
        }

        QDir srcDir(srcPath);
        QStringList entries = srcDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString& entry : entries) {
            QString srcEntry = srcPath + "/" + entry;
            QString dstEntry = dstPath + "/" + entry;
            if (!copyRecursively(srcEntry, dstEntry)) {
                return false;
            }
        }
    } else if (srcInfo.isFile()) {
        if (!copyFileReplacing(srcPath, dstPath)) {
            return false;
        }
    }

    return true;
}

void ModManager::installModsAsync(const QStringList& filePaths) {
    QMap<QString, QString> updateTargets;
    for (const QString& path : filePaths) {
        const QVariantMap target = nexusUpdateTargetForArchive(path);
        const QString name = target.value("name").toString();
        if (target.value("available").toBool() && !name.isEmpty())
            updateTargets.insert(path, name);
    }

    const QMap<QString, QString> installTargets = modInstallTargets();
    const QMap<QString, QStringList> looseExtensions = looseModExtensions();

    (void)QtConcurrent::run([this, filePaths, updateTargets, installTargets, looseExtensions]() {
        this->installMods(filePaths, updateTargets, installTargets, looseExtensions);
    });
}

void ModManager::installMods(const QStringList& filePaths,
                             const QMap<QString, QString>& updateTargets,
                             const QMap<QString, QString>& installTargets,
                             const QMap<QString, QStringList>& looseExtensions) {
    if (m_modsPathInternal.isEmpty()) {
        emit installComplete(false, "Mods path not configured");
        return;
    }

    bool containsArchive = false;
    for (const QString& filePath : filePaths) {
        const QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == "zip" || ext == "rar" || ext == "7z") {
            containsArchive = true;
            break;
        }
    }

    if (containsArchive && m_sevenZipPath.trimmed().isEmpty()) {
        m_pendingInstallFiles.clear();
        emit archiveToolConfigurationRequired();
        return;
    }

    m_pendingInstallFiles = filePaths;
    m_modNameMappings.clear();
    m_selectedFiles.clear();
    m_installSkipGroups.clear();

    emit installProgress("Analyzing mods...");

    QMap<QString, QVariantMap> modGroups;
    auto buildFileData = [this](const QFileInfo& info) -> QVariantMap {
        QVariantMap fileData;
        fileData["name"] = info.fileName();
        fileData["size"] = formatFileSize(info.size());
        fileData["extension"] = info.suffix().toUpper();
        fileData["selected"] = true;
        return fileData;
    };
    auto collectExtraFiles = [this](const QString& scopePath, bool recursive) -> QVariantList {
        QVariantList extraFiles;
        const QDir scopeDir(scopePath);
        QDirIterator it(scopePath,
                        QDir::Files | QDir::NoDotAndDotDot,
                        recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

        while (it.hasNext()) {
            const QString extraFilePath = it.next();
            const QFileInfo extraInfo(extraFilePath);
            if (isUnrealModExtension(extraInfo.suffix())) {
                continue;
            }

            QVariantMap extraFile;
            extraFile["name"] = extraInfo.fileName();
            extraFile["relativePath"] = scopeDir.relativeFilePath(extraFilePath);
            extraFile["size"] = formatFileSize(extraInfo.size());

            const QString extension = extraInfo.suffix().toUpper();
            extraFile["extension"] = extension.isEmpty() ? "FILE" : extension;
            extraFiles.append(extraFile);
        }

        return extraFiles;
    };

    for (const QString& filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        QString fullFileName = fileInfo.fileName();
        QString ext = fileInfo.suffix().toLower();
        QString baseName = fileInfo.completeBaseName();

        bool isDisabled = false;
        if (ext.endsWith("-x")) {
            ext = ext.left(ext.length() - 2);
            isDisabled = true;
        }

        if (ext == "zip" || ext == "rar" || ext == "7z") {

            QString tempDir = QDir::tempPath() + "/modmanager_temp_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            QDir().mkpath(tempDir);

            if (extractArchive(filePath, tempDir)) {

                QDirIterator nestedArchiveIt(tempDir, { "*.zip", "*.rar", "*.7z" }, QDir::Files, QDirIterator::Subdirectories);
                QStringList nestedArchives;
                while (nestedArchiveIt.hasNext()) {
                    nestedArchives.append(nestedArchiveIt.next());
                }

                for (const QString& nestedArchivePath : nestedArchives) {
                    QFileInfo nestedInfo(nestedArchivePath);
                    QString nestedExtractDir = nestedInfo.absolutePath() + "/" + nestedInfo.completeBaseName();
                    QDir().mkpath(nestedExtractDir);

                    if (extractArchive(nestedArchivePath, nestedExtractDir)) {
                        qDebug() << "Extracted nested archive:" << nestedInfo.fileName() << "to" << nestedExtractDir;

                        QFile::remove(nestedArchivePath);
                    }
                }

                ModInstallPlanner::Context planContext;
                planContext.modsPath = m_modsPathInternal;
                planContext.gamePaksPath = m_gamePaksPath;
                planContext.luaModsRoot = activeLuaModsRoot();
                planContext.cppModsRoot = activeCppModsRoot();
                planContext.installName = baseName;
                const ModInstallPlanner::Result installPlan =
                    ModInstallPlanner::analyze(tempDir, planContext);

                if (installPlan.recognized) {
                    if (installPlan.groups.isEmpty()) {
                        QDir(tempDir).removeRecursively();
                        continue;
                    }

                    for (const QVariant& groupValue : installPlan.groups) {
                        QVariantMap group = groupValue.toMap();
                        const QString groupName = group.value("originalName").toString();
                        if (groupName.isEmpty() || modGroups.contains(groupName))
                            continue;
                        group["archivePath"] = filePath;
                        group["modId"] = createRandomModId();
                        group["installValid"] = installPlan.valid;
                        if (!installPlan.valid)
                            group["installError"] = installPlan.error;
                        modGroups[groupName] = group;
                    }

                    QDir(tempDir).removeRecursively();
                    continue;
                }

                QDir extractedDir(tempDir);
                QStringList rootFiles = extractedDir.entryList({ "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files);

                QStringList modSubFolders;
                QDirIterator allDirsIt(tempDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                while (allDirsIt.hasNext()) {
                    QString dirPath = allDirsIt.next();
                    QDir dir(dirPath);

                    QStringList modFiles = dir.entryList({ "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files);
                    if (!modFiles.isEmpty()) {

                        QString relativePath = QDir(tempDir).relativeFilePath(dirPath);
                        modSubFolders.append(relativePath);
                    }
                }

                if (modSubFolders.size() > 1) {
                    for (const QString& modSubFolder : modSubFolders) {

                        QString subFolderPath = extractedDir.filePath(modSubFolder);
                        QDirIterator fileIt(subFolderPath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files, QDirIterator::Subdirectories);
                        const QVariantList extraFiles = collectExtraFiles(subFolderPath, true);

                        QMap<QString, QVariantList> filesByBaseName;

                        while (fileIt.hasNext()) {
                            QString filePath = fileIt.next();
                            QFileInfo fileInfo(filePath);
                            const QString baseName = normalizedModBaseName(fileInfo);
                            filesByBaseName[baseName].append(buildFileData(fileInfo));
                        }

                        QStringList baseNames = filesByBaseName.keys();
                        QMap<QString, QStringList> prefixGroups = groupByCommonPrefix(baseNames);

                        for (auto prefixIt = prefixGroups.begin(); prefixIt != prefixGroups.end(); ++prefixIt) {
                            QString groupName = prefixIt.key();
                            QStringList relatedNames = prefixIt.value();

                            if (!modGroups.contains(groupName)) {
                                QVariantList allFiles;
                                for (const QString& name : relatedNames) {
                                    allFiles.append(filesByBaseName[name]);
                                }

                                QVariantMap group;
                                group["originalName"] = groupName;
                                group["newName"] = groupName;
                                group["archivePath"] = filePath;
                                group["modSubFolder"] = modSubFolder;
                                group["files"] = allFiles;
                                group["extraFiles"] = extraFiles;
                                group["modId"] = createRandomModId();
                                modGroups[groupName] = group;
                            }
                        }
                    }
                } else if (modSubFolders.size() == 1) {

                    QString modSubFolder = modSubFolders.first();

                    QString subFolderPath = extractedDir.filePath(modSubFolder);
                    QDirIterator fileIt(subFolderPath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files, QDirIterator::Subdirectories);
                    const QVariantList extraFiles = collectExtraFiles(subFolderPath, true);

                    QMap<QString, QVariantList> filesByBaseName;

                    while (fileIt.hasNext()) {
                        QString filePath = fileIt.next();
                        QFileInfo fileInfo(filePath);
                        const QString baseName = normalizedModBaseName(fileInfo);
                        filesByBaseName[baseName].append(buildFileData(fileInfo));
                    }

                    QStringList baseNames = filesByBaseName.keys();
                    QMap<QString, QStringList> prefixGroups = groupByCommonPrefix(baseNames);

                    for (auto prefixIt = prefixGroups.begin(); prefixIt != prefixGroups.end(); ++prefixIt) {
                        QString groupName = prefixIt.key();
                        QStringList relatedNames = prefixIt.value();

                        if (!modGroups.contains(groupName)) {
                            QVariantList allFiles;
                            for (const QString& name : relatedNames) {
                                allFiles.append(filesByBaseName[name]);
                            }

                            QVariantMap group;
                            group["originalName"] = groupName;
                            group["newName"] = groupName;
                            group["archivePath"] = filePath;
                            group["modSubFolder"] = modSubFolder;
                            group["files"] = allFiles;
                            group["extraFiles"] = extraFiles;
                            group["modId"] = createRandomModId();
                            modGroups[groupName] = group;
                        }
                    }
                } else if (!rootFiles.isEmpty()) {

                    QMap<QString, QVariantList> filesByBaseName;
                    const QVariantList extraFiles = collectExtraFiles(tempDir, false);

                    for (const QString& rootFile : rootFiles) {
                        QFileInfo rootInfo(extractedDir.filePath(rootFile));
                        const QString groupName = normalizedModBaseName(rootInfo);
                        filesByBaseName[groupName].append(buildFileData(rootInfo));
                    }

                    QStringList baseNames = filesByBaseName.keys();
                    QMap<QString, QStringList> prefixGroups = groupByCommonPrefix(baseNames);

                    for (auto prefixIt = prefixGroups.begin(); prefixIt != prefixGroups.end(); ++prefixIt) {
                        QString groupName = prefixIt.key();
                        QStringList relatedNames = prefixIt.value();

                        if (modGroups.contains(groupName)) {
                            continue;
                        }

                        QVariantList allFiles;
                        for (const QString& name : relatedNames) {
                            allFiles.append(filesByBaseName[name]);
                        }

                        QVariantMap group;
                        group["originalName"] = groupName;
                        group["newName"] = groupName;
                        group["archivePath"] = filePath;
                        group["modSubFolder"] = "";
                        group["files"] = allFiles;
                        group["extraFiles"] = extraFiles;
                        group["modId"] = createRandomModId();
                        modGroups[groupName] = group;
                    }
                }

                QDir(tempDir).removeRecursively();
            }
        } else if (ext == "pak" || ext == "ucas" || ext == "utoc") {

            QString modName = baseName;
            if (modName.toUpper().endsWith("_P")) {
                modName = modName.left(modName.length() - 2);
            }

            if (!modGroups.contains(modName)) {
                QVariantMap group;
                group["originalName"] = modName;
                group["newName"] = modName;
                group["files"] = QVariantList();
                group["extraFiles"] = QVariantList();
                group["modId"] = createRandomModId();
                modGroups[modName] = group;
            }

            QVariantMap group = modGroups[modName];
            QVariantList files = group["files"].toList();
            files.append(buildFileData(fileInfo));

            group["files"] = files;
            modGroups[modName] = group;
        } else if (fileInfo.isDir()) {

            ModInstallPlanner::Context planContext;
            planContext.modsPath = m_modsPathInternal;
            planContext.gamePaksPath = m_gamePaksPath;
            planContext.luaModsRoot = activeLuaModsRoot();
            planContext.cppModsRoot = activeCppModsRoot();
            planContext.installName = fileInfo.fileName();
            const ModInstallPlanner::Result installPlan =
                ModInstallPlanner::analyze(filePath, planContext);

            if (installPlan.recognized) {
                if (installPlan.groups.isEmpty())
                    continue;

                for (const QVariant& groupValue : installPlan.groups) {
                    QVariantMap group = groupValue.toMap();
                    const QString groupName = group.value("originalName").toString();
                    if (groupName.isEmpty() || modGroups.contains(groupName))
                        continue;
                    group["folderPath"] = filePath;
                    group["modId"] = createRandomModId();
                    group["installValid"] = installPlan.valid;
                    if (!installPlan.valid)
                        group["installError"] = installPlan.error;
                    modGroups[groupName] = group;
                }
                continue;
            }

            QDir folderDir(filePath);

            QStringList modFiles = folderDir.entryList({ "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files);
            if (modFiles.isEmpty()) {
                continue;
            }

            QString folderName = fileInfo.fileName();

            QMap<QString, QVariantList> filesByBaseName;
            QDirIterator folderIt(filePath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files, QDirIterator::Subdirectories);

            while (folderIt.hasNext()) {
                QString modFilePath = folderIt.next();
                QFileInfo modFileInfo(modFilePath);
                const QString baseName = normalizedModBaseName(modFileInfo);
                filesByBaseName[baseName].append(buildFileData(modFileInfo));
            }

            const QVariantList extraFiles = collectExtraFiles(filePath, true);

            QStringList baseNames = filesByBaseName.keys();
            QMap<QString, QStringList> prefixGroups = groupByCommonPrefix(baseNames);

            for (auto prefixIt = prefixGroups.begin(); prefixIt != prefixGroups.end(); ++prefixIt) {
                QString groupName = prefixIt.key();
                QStringList relatedNames = prefixIt.value();

                if (prefixGroups.size() == 1) {
                    groupName = folderName;
                }

                if (!modGroups.contains(groupName)) {
                    QVariantList allFiles;
                    for (const QString& name : relatedNames) {
                        allFiles.append(filesByBaseName[name]);
                    }

                    QVariantMap group;
                    group["originalName"] = groupName;
                    group["newName"] = groupName;
                    group["folderPath"] = filePath;
                    group["files"] = allFiles;
                    group["extraFiles"] = extraFiles;
                    group["modId"] = createRandomModId();
                    modGroups[groupName] = group;
                }
            }
        }
    }

    if (!updateTargets.isEmpty()) {
        QMap<QString, int> groupsPerArchive;
        for (auto it = modGroups.cbegin(); it != modGroups.cend(); ++it)
            ++groupsPerArchive[it.value().value("archivePath").toString()];

        for (auto it = modGroups.begin(); it != modGroups.end(); ++it) {
            const QString archive = it.value().value("archivePath").toString();
            const QString target = updateTargets.value(archive);
            if (target.isEmpty() || groupsPerArchive.value(archive) != 1)
                continue;
            it.value()["newName"] = target;
            it.value()["updateOf"] = target;
        }
    }

    if (!installTargets.isEmpty()) {
        for (auto it = modGroups.begin(); it != modGroups.end(); ++it) {
            if (!it.value().value("updateOf").toString().isEmpty())
                continue;

            const QString key =
                mmdetail::normalizedModKey(it.value().value("originalName").toString());
            const QString target = installTargets.value(key);
            if (target.isEmpty())
                continue;

            const QStringList targetExtensions = looseExtensions.value(key);
            if (!targetExtensions.isEmpty()) {
                const QSet<QString> incoming = modExtensionsInGroup(it.value());
                bool extensionMatch = false;
                for (const QString& extension : targetExtensions) {
                    if (incoming.contains(extension)) {
                        extensionMatch = true;
                        break;
                    }
                }
                if (!extensionMatch) {
                    qDebug() << "install target skipped, loose file extension differs:"
                             << it.value().value("originalName").toString()
                             << "vs" << target;
                    continue;
                }
            }

            it.value()["newName"] = target;
            it.value()["updateOf"] = target;
        }
    }

    QVariantList modGroupsList;
    for (const QVariantMap& group : modGroups) {
        modGroupsList.append(group);
    }

    if (modGroupsList.isEmpty()) {
        m_pendingInstallFiles.clear();
        emit installComplete(false, "No Unreal Engine mod files (.pak, .ucas, .utoc) were found in the selected file(s).");
        return;
    }

    emit modsDetectedForRename(modGroupsList);
}

void ModManager::confirmModInstallation(const QVariantList& modMappings) {
    if (m_pendingInstallFiles.isEmpty()) {
        emit installComplete(false, "No pending installation");
        return;
    }

    m_batchOperationInProgress = true;

    m_installDestFolders.clear();
    m_installBundleSources.clear();

    QHash<QString, QString> liveFolderByName;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap row = modValue.toMap();
        if (row.value("type").toString() == QLatin1String("folder"))
            liveFolderByName.insert(row.value("name").toString(),
                                    row.value("folderPath").toString());
    }
    QSet<QString> knownBundleFolders;
    for (const QVariant& bundleValue : m_modBundles) {
        const QString folder = bundleValue.toMap().value("folder").toString();
        if (!folder.isEmpty())
            knownBundleFolders.insert(folder.toLower());
    }

    QSet<QString> luaEnableGroups;
    for (const QVariant& mappingVar : modMappings) {
        const QVariantMap mapping = mappingVar.toMap();
        if (!mapping.value("install", true).toBool())
            continue;

        const QString originalName = mapping.value("originalName").toString();
        if (mapping.value("luaEnabled").toBool())
            luaEnableGroups.insert(originalName);
        const QString newName = mapping.value("newName").toString();
        const QString updateOf = mapping.value("updateOf").toString();
        const QString bundle = sanitizedBundleFolder(mapping.value("bundleFolder").toString());
        QString installName = updateOf.isEmpty() ? newName : updateOf;
        if (installName.isEmpty())
            installName = originalName;

        QString relativeFolder = updateOf.isEmpty()
            ? QString() : liveFolderByName.value(updateOf);
        if (relativeFolder.isEmpty()) {
            QString leaf = sanitizedBundleFolder(installName);
            if (leaf.isEmpty())
                leaf = installName;
            relativeFolder = bundle.isEmpty()
                ? leaf : bundle + QLatin1Char('/') + leaf;
        }
        m_installDestFolders.insert(originalName, relativeFolder);

        const QString archive = mapping.value("archivePath").toString();
        if (!bundle.isEmpty()) {
            if (!m_installBundleSources.contains(bundle)
                || m_installBundleSources.value(bundle).isEmpty()) {
                m_installBundleSources.insert(bundle, archive);
            }
            continue;
        }

        const QString firstSegment = relativeFolder.section(QLatin1Char('/'), 0, 0);
        if (firstSegment != relativeFolder && knownBundleFolders.contains(firstSegment.toLower())) {
            if (!m_installBundleSources.contains(firstSegment)
                || m_installBundleSources.value(firstSegment).isEmpty()) {
                m_installBundleSources.insert(firstSegment, archive);
            }
        }
    }

    for (auto it = m_installBundleSources.cbegin(); it != m_installBundleSources.cend(); ++it) {
        const QString bundleFolder = it.key();
        const QFileInfo bundleInfo(QDir(m_modsPathInternal).filePath(bundleFolder));
        if (bundleInfo.exists() && bundleInfo.isDir()
            && !knownBundleFolders.contains(bundleFolder.toLower())) {
            m_batchOperationInProgress = false;
            m_installDestFolders.clear();
            m_installBundleSources.clear();
            emit installComplete(false, QStringLiteral(
                "A folder named \"%1\" already exists and is not a bundle.").arg(bundleFolder));
            return;
        }
    }

    QMap<QString, QStringList> installedByArchive;
    for (const QVariant& mappingVar : modMappings) {        const QVariantMap mapping = mappingVar.toMap();
        if (!mapping.value("install", true).toBool())
            continue;
        const QString archive = mapping.value("archivePath").toString();
        const QString updateOf = mapping.value("updateOf").toString();
        const QString newName = mapping.value("newName").toString();
        const QString installedName = updateOf.isEmpty() ? newName : updateOf;
        if (!archive.isEmpty() && !installedName.isEmpty())
            installedByArchive[QFileInfo(archive).absoluteFilePath()].append(installedName);
    }

    const QMap<QString, QString> installBundleSources = m_installBundleSources;
    const QString luaModsRoot = activeLuaModsRoot();

    if (!luaModsRoot.isEmpty() && !QDir(luaModsRoot).exists() && m_luaProfileManager) {
        m_luaProfileManager->createProfileFolder(m_activePresetId, QString());
    }

    const QString cppModsRoot = activeCppModsRoot();
    if (!cppModsRoot.isEmpty())
        QDir().mkpath(cppModsRoot);

    (void)QtConcurrent::run([this, modMappings, installedByArchive, installBundleSources, luaModsRoot,
                             cppModsRoot, luaEnableGroups]() {

        m_modNameMappings.clear();
        m_selectedFiles.clear();
        m_installSkipGroups.clear();
        QMap<QString, QString> renameAfterInstall;
        QMap<QString, QStringList> aliasRecords;
        QStringList updatedTargets;

        for (const QVariant& mappingVar : modMappings) {
            QVariantMap mapping = mappingVar.toMap();
            QString originalName = mapping["originalName"].toString();
            const QString newName = mapping["newName"].toString();
            const QString updateOf = mapping.value("updateOf").toString();

            const bool installEnabled = mapping.value("install", true).toBool();
            QVariantList filesArray = mapping["files"].toList();
            bool anyFileSelected = filesArray.isEmpty();
            for (const QVariant& fileVar : filesArray) {
                if (fileVar.toMap().value("selected").toBool()) {
                    anyFileSelected = true;
                    break;
                }
            }
            if (!installEnabled || !anyFileSelected) {
                m_installSkipGroups.insert(originalName);
                continue;
            }

            const QString installName = updateOf.isEmpty() ? newName : updateOf;
            m_modNameMappings[originalName] = installName;

            if (m_modIds.value(installName).isEmpty())
                m_modIds[installName] = mapping["modId"].toString();

            const QString uniqueId = m_modIds.value(installName);
            if (!uniqueId.isEmpty()) {
                QStringList& names = aliasRecords[uniqueId];
                names << originalName << installName;
                if (!newName.isEmpty())
                    names << newName;
            }

            QStringList selectedFilesList;
            for (const QVariant& fileVar : filesArray) {
                QVariantMap fileMap = fileVar.toMap();
                QString fileName = fileMap["name"].toString();
                bool isSelected = fileMap["selected"].toBool();

                if (isSelected) {
                    selectedFilesList.append(fileName);
                }
            }
            m_selectedFiles[originalName] = selectedFilesList;

            if (!updateOf.isEmpty()) {
                if (!updatedTargets.contains(updateOf))
                    updatedTargets.append(updateOf);
                if (!newName.isEmpty() && newName != updateOf)
                    renameAfterInstall.insert(updateOf, newName);
            }
        }

        emit installProgress("Starting installation...");

        int successCount = 0;
        int totalFiles = m_pendingInstallFiles.count();
        QStringList installErrors;
        QSet<QString> paksInstalledArchives;
        bool luaInstalled = false;
        bool cppInstalled = false;
        QStringList luaEnableAfterInstall;
        auto copyExtraArchiveFiles = [](const QString& scopePath, const QString& modFolder, bool recursive) {
            const QDir scopeDir(scopePath);
            QDirIterator extraIt(scopePath,
                                 QDir::Files | QDir::NoDotAndDotDot,
                                 recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);

            while (extraIt.hasNext()) {
                const QString sourceFile = extraIt.next();
                const QFileInfo sourceInfo(sourceFile);
                if (isUnrealModExtension(sourceInfo.suffix())) {
                    continue;
                }

                const QString relativePath = scopeDir.relativeFilePath(sourceFile);
                const QString destFile = QDir(modFolder).filePath(relativePath);
                const QFileInfo destInfo(destFile);
                QDir().mkpath(destInfo.absolutePath());

                if (copyFileReplacing(sourceFile, destFile)) {
                    qDebug() << "Copied companion archive file:" << sourceInfo.fileName() << "to" << destFile;
                }
            }
        };

        QElapsedTimer progressTimer;
        progressTimer.start();
        const qint64 progressThrottleMs = 150;

        for (int i = 0; i < m_pendingInstallFiles.count(); i++) {
            const QString& filePath = m_pendingInstallFiles[i];
            QFileInfo fileInfo(filePath);
            QString fullFileName = fileInfo.fileName();
            QString ext = fileInfo.suffix().toLower();
            QString baseName = fileInfo.completeBaseName();

            bool isDisabled = false;
            if (ext.endsWith("-x")) {
                ext = ext.left(ext.length() - 2);
                isDisabled = true;
            }

            bool shouldUpdateProgress = (i == 0) || (i == totalFiles - 1) || (progressTimer.elapsed() >= progressThrottleMs);
            if (shouldUpdateProgress) {
                emit installProgress(QString("Processing %1/%2: %3").arg(i + 1).arg(totalFiles).arg(fileInfo.fileName()));
                progressTimer.restart();
            }

            if (ext == "zip" || ext == "rar" || ext == "7z") {

                if (shouldUpdateProgress) {
                    emit installProgress(QString("Extracting %1...").arg(fileInfo.fileName()));
                }

                QString tempExtractDir = QDir::tempPath() + "/modmanager_extract_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
                QDir().mkpath(tempExtractDir);

                if (extractArchive(filePath, tempExtractDir)) {

                    QDirIterator nestedArchiveIt(tempExtractDir, { "*.zip", "*.rar", "*.7z" }, QDir::Files, QDirIterator::Subdirectories);
                    QStringList nestedArchives;
                    while (nestedArchiveIt.hasNext()) {
                        nestedArchives.append(nestedArchiveIt.next());
                    }

                    for (const QString& nestedArchivePath : nestedArchives) {
                        QFileInfo nestedInfo(nestedArchivePath);
                        QString nestedExtractDir = nestedInfo.absolutePath() + "/" + nestedInfo.completeBaseName();
                        QDir().mkpath(nestedExtractDir);

                        if (extractArchive(nestedArchivePath, nestedExtractDir)) {
                            qDebug() << "Extracted nested archive:" << nestedInfo.fileName() << "to" << nestedExtractDir;

                            QFile::remove(nestedArchivePath);
                        }
                    }

                    ModInstallPlanner::Context planContext;
                    planContext.modsPath = m_modsPathInternal;
                    planContext.gamePaksPath = m_gamePaksPath;
                    planContext.luaModsRoot = luaModsRoot;
                    planContext.cppModsRoot = cppModsRoot;
                    planContext.installName = QFileInfo(filePath).completeBaseName();
                    const ModInstallPlanner::Result installPlan =
                        ModInstallPlanner::analyze(tempExtractDir, planContext);

                    if (installPlan.recognized) {
                        if (planHasPakGroups(installPlan))
                            paksInstalledArchives.insert(QFileInfo(filePath).absoluteFilePath());
                        if (!installPlan.valid) {
                            installErrors.append(installPlan.error.isEmpty()
                                ? QStringLiteral("Install destination could not be resolved.")
                                : installPlan.error);
                        } else {
                            for (const QVariant& groupValue : installPlan.groups) {
                                const QVariantMap group = groupValue.toMap();
                                const QString originalName = group.value("originalName").toString();
                                if (!m_modNameMappings.contains(originalName))
                                    continue;
                                const QStringList selectedFiles = m_selectedFiles.value(originalName, QStringList());
                                const QString folderName = m_modNameMappings.value(originalName, originalName);
                                const QString relativeFolder = m_installDestFolders.value(originalName, folderName);
                                const PlannerFolders planned = plannerFoldersFor(
                                    group, folderName, relativeFolder, m_installBundleSources.keys());
                                int copiedFiles = 0;
                                QString copyError;
                                if (ModInstallPlanner::installGroup(group, selectedFiles, planned.leaf,
                                                                   planned.prefix, &copiedFiles, &copyError)) {
                                    ++successCount;
                                    if (groupHasLuaRoot(group)) {
                                        luaInstalled = true;
                                        if (luaEnableGroups.contains(originalName)) {
                                            luaEnableAfterInstall.append(
                                                planned.leaf.isEmpty() ? originalName : planned.leaf);
                                        }
                                    }
                                    if (groupHasCppRoot(group))
                                        cppInstalled = true;
                                    emit installProgress(QString("Installed %1 file(s) for %2")
                                                             .arg(copiedFiles)
                                                             .arg(originalName));
                                } else {
                                    installErrors.append(copyError.isEmpty()
                                        ? QStringLiteral("Failed to install %1").arg(originalName)
                                        : copyError);
                                }
                            }
                        }
                        QDir(tempExtractDir).removeRecursively();
                        continue;
                    }

                    if (shouldUpdateProgress) {
                        emit installProgress(QString("Processing files from %1...").arg(fileInfo.fileName()));
                    }

                    QDir extractedDir(tempExtractDir);
                    QStringList rootFiles = extractedDir.entryList({ "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files);

                    QStringList modSubFolders;
                    QDirIterator allDirsIt(tempExtractDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                    while (allDirsIt.hasNext()) {
                        QString dirPath = allDirsIt.next();
                        QDir dir(dirPath);

                        QStringList modFiles = dir.entryList({ "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files);
                        if (!modFiles.isEmpty()) {

                            QString relativePath = QDir(tempExtractDir).relativeFilePath(dirPath);
                            modSubFolders.append(relativePath);
                        }
                    }

                    if (modSubFolders.size() > 1) {

                        for (const QString& modSubFolder : modSubFolders) {
                            QString sourceModPath = extractedDir.filePath(modSubFolder);

                            QDirIterator fileIt(sourceModPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                            QMap<QString, QStringList> filesByBaseName;

                            while (fileIt.hasNext()) {
                                QString sourceFile = fileIt.next();
                                QFileInfo fileInfo(sourceFile);
                                QString ext = fileInfo.suffix().toLower();

                                if (ext == "pak" || ext == "pak-x" || ext == "ucas" || ext == "ucas-x" || ext == "utoc" || ext == "utoc-x") {

                                    QString baseName = fileInfo.completeBaseName();
                                    if (baseName.toUpper().endsWith("_P")) {
                                        baseName = baseName.left(baseName.length() - 2);
                                    }
                                    filesByBaseName[baseName].append(sourceFile);
                                }
                            }

                            for (auto it = filesByBaseName.begin(); it != filesByBaseName.end(); ++it) {
                                QString groupName = it.key();
                                QStringList groupFiles = it.value();
                                if (m_installSkipGroups.contains(groupName))
                                    continue;

                                QString modFolderName = groupName;
                                if (m_modNameMappings.contains(groupName)) {
                                    modFolderName = m_modNameMappings[groupName];
                                }

                                QString modFolder = QDir(m_modsPathInternal).filePath(
                                    m_installDestFolders.value(groupName, modFolderName));
                                QDir().mkpath(modFolder);

                                for (const QString& sourceFile : groupFiles) {
                                    QFileInfo sourceInfo(sourceFile);
                                    QString ext = sourceInfo.suffix().toLower();

                                    QStringList selectedForThisMod = m_selectedFiles.value(groupName, QStringList());
                                    bool isFileSelected = selectedForThisMod.isEmpty();

                                    if (!selectedForThisMod.isEmpty()) {

                                        QString fileExt = ext.toLower();
                                        if (fileExt.endsWith("-x")) {
                                            fileExt = fileExt.left(fileExt.length() - 2);
                                        }

                                        for (const QString& selectedFile : selectedForThisMod) {
                                            QString selectedExt = QFileInfo(selectedFile).suffix().toLower();
                                            if (selectedExt.endsWith("-x")) {
                                                selectedExt = selectedExt.left(selectedExt.length() - 2);
                                            }
                                            if (fileExt == selectedExt) {
                                                isFileSelected = true;
                                                break;
                                            }
                                        }
                                    }

                                    if (isFileSelected) {

                                        QString sourceBaseName = sourceInfo.completeBaseName();
                                        if (sourceBaseName.toUpper().endsWith("_P")) {
                                            sourceBaseName = sourceBaseName.left(sourceBaseName.length() - 2);
                                        }

                                        QString cleanExt = ext;
                                        if (cleanExt.endsWith("-x")) {
                                            cleanExt = cleanExt.left(cleanExt.length() - 2);
                                        }

                                        QString newFileName;
                                        if (sourceBaseName == groupName) {

                                            newFileName = modFolderName + "_P." + cleanExt;
                                        } else {

                                            newFileName = sourceInfo.completeBaseName() + "." + cleanExt;
                                        }

                                        QString destFile = modFolder + "/" + newFileName;
                                        copyFileReplacing(sourceFile, destFile);
                                        qDebug() << "Copied and renamed:" << sourceInfo.fileName() << "to" << newFileName;
                                    }
                                }

                                QStringList imageFilters = {"*.png", "*.jpg", "*.jpeg", "*.gif", "*.bmp", "*.webp"};
                                QDir imageDir(sourceModPath);
                                QStringList imageFiles = imageDir.entryList(imageFilters, QDir::Files);
                                if (!imageFiles.isEmpty()) {
                                    QString sourceImagePath = imageDir.filePath(imageFiles.first());
                                    QFileInfo imgInfo(sourceImagePath);
                                    QString destImagePath = modFolder + "/" + imgInfo.fileName();
                                    copyFileReplacing(sourceImagePath, destImagePath);
                                }

                                copyExtraArchiveFiles(sourceModPath, modFolder, true);

                                successCount++;
                            }
                        }
                    } else if (modSubFolders.size() == 1) {

                        QString modSubFolder = modSubFolders.first();
                        QString sourceModPath = extractedDir.filePath(modSubFolder);

                        QDirIterator fileIt(sourceModPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
                        QMap<QString, QStringList> filesByBaseName;

                        while (fileIt.hasNext()) {
                            QString sourceFile = fileIt.next();
                            QFileInfo fileInfo(sourceFile);
                            QString ext = fileInfo.suffix().toLower();

                            if (ext == "pak" || ext == "pak-x" || ext == "ucas" || ext == "ucas-x" || ext == "utoc" || ext == "utoc-x") {

                                QString baseName = fileInfo.completeBaseName();
                                if (baseName.toUpper().endsWith("_P")) {
                                    baseName = baseName.left(baseName.length() - 2);
                                }
                                filesByBaseName[baseName].append(sourceFile);
                            }
                        }

                        for (auto it = filesByBaseName.begin(); it != filesByBaseName.end(); ++it) {
                            QString groupName = it.key();
                            QStringList groupFiles = it.value();
                            if (m_installSkipGroups.contains(groupName))
                                continue;

                            QString modFolderName = groupName;
                            if (m_modNameMappings.contains(groupName)) {
                                modFolderName = m_modNameMappings[groupName];
                            }

                            QString modFolder = QDir(m_modsPathInternal).filePath(
                                    m_installDestFolders.value(groupName, modFolderName));
                            QDir().mkpath(modFolder);

                            for (const QString& sourceFile : groupFiles) {
                                QFileInfo sourceInfo(sourceFile);
                                QString ext = sourceInfo.suffix().toLower();

                                QStringList selectedForThisMod = m_selectedFiles.value(groupName, QStringList());
                                bool isFileSelected = selectedForThisMod.isEmpty();

                                if (!selectedForThisMod.isEmpty()) {

                                    QString fileExt = ext.toLower();
                                    if (fileExt.endsWith("-x")) {
                                        fileExt = fileExt.left(fileExt.length() - 2);
                                    }

                                    for (const QString& selectedFile : selectedForThisMod) {
                                        QString selectedExt = QFileInfo(selectedFile).suffix().toLower();
                                        if (selectedExt.endsWith("-x")) {
                                            selectedExt = selectedExt.left(selectedExt.length() - 2);
                                        }
                                        if (fileExt == selectedExt) {
                                            isFileSelected = true;
                                            break;
                                        }
                                    }
                                }

                                if (isFileSelected) {

                                    QString sourceBaseName = sourceInfo.completeBaseName();
                                    if (sourceBaseName.toUpper().endsWith("_P")) {
                                        sourceBaseName = sourceBaseName.left(sourceBaseName.length() - 2);
                                    }

                                    QString cleanExt = ext;
                                    if (cleanExt.endsWith("-x")) {
                                        cleanExt = cleanExt.left(cleanExt.length() - 2);
                                    }

                                    QString newFileName;
                                    if (sourceBaseName == groupName) {

                                        newFileName = modFolderName + "_P." + cleanExt;
                                    } else {

                                        newFileName = sourceInfo.completeBaseName() + "." + cleanExt;
                                    }

                                    QString destFile = modFolder + "/" + newFileName;
                                    copyFileReplacing(sourceFile, destFile);
                                    qDebug() << "Copied and renamed:" << sourceInfo.fileName() << "to" << newFileName;
                                }
                            }

                            QStringList imageFilters = {"*.png", "*.jpg", "*.jpeg", "*.gif", "*.bmp", "*.webp"};
                            QDir imageDir(sourceModPath);
                            QStringList imageFiles = imageDir.entryList(imageFilters, QDir::Files);
                            if (!imageFiles.isEmpty()) {
                                QString sourceImagePath = imageDir.filePath(imageFiles.first());
                                QFileInfo imgInfo(sourceImagePath);
                                QString destImagePath = modFolder + "/" + imgInfo.fileName();
                                copyFileReplacing(sourceImagePath, destImagePath);
                            }

                            copyExtraArchiveFiles(sourceModPath, modFolder, true);

                            successCount++;
                        }
                    } else if (!rootFiles.isEmpty()) {

                        QMap<QString, QStringList> filesByBaseName;

                        for (const QString& rootFile : rootFiles) {
                            QFileInfo rootInfo(extractedDir.filePath(rootFile));
                            QString groupName = rootInfo.completeBaseName();
                            if (groupName.toUpper().endsWith("_P")) {
                                groupName = groupName.left(groupName.length() - 2);
                            }
                            filesByBaseName[groupName].append(rootInfo.absoluteFilePath());
                        }

                        QStringList baseNames = filesByBaseName.keys();
                        QMap<QString, QStringList> prefixGroups = groupByCommonPrefix(baseNames);

                        for (auto prefixIt = prefixGroups.begin(); prefixIt != prefixGroups.end(); ++prefixIt) {
                            QString groupName = prefixIt.key();
                            QStringList relatedNames = prefixIt.value();
                            if (m_installSkipGroups.contains(groupName))
                                continue;

                            QString modFolderName = groupName;
                            if (m_modNameMappings.contains(groupName)) {
                                modFolderName = m_modNameMappings[groupName];
                            }

                            QString modFolder = QDir(m_modsPathInternal).filePath(
                                    m_installDestFolders.value(groupName, modFolderName));
                            QDir().mkpath(modFolder);

                            QStringList allGroupFiles;
                            for (const QString& relatedName : relatedNames) {
                                allGroupFiles.append(filesByBaseName[relatedName]);
                            }

                            for (const QString& sourceFile : allGroupFiles) {
                                QFileInfo sourceInfo(sourceFile);
                                QString ext = sourceInfo.suffix().toLower();

                                QStringList selectedForThisMod = m_selectedFiles.value(groupName, QStringList());
                                bool isFileSelected = selectedForThisMod.isEmpty();

                                if (!selectedForThisMod.isEmpty()) {
                                    QString fileExt = ext;
                                    if (fileExt.endsWith("-x")) {
                                        fileExt = fileExt.left(fileExt.length() - 2);
                                    }

                                    for (const QString& selectedFile : selectedForThisMod) {
                                        QString selectedExt = QFileInfo(selectedFile).suffix().toLower();
                                        if (selectedExt.endsWith("-x")) {
                                            selectedExt = selectedExt.left(selectedExt.length() - 2);
                                        }
                                        if (fileExt == selectedExt) {
                                            isFileSelected = true;
                                            break;
                                        }
                                    }
                                }

                                if (isFileSelected) {

                                    QString sourceBaseName = sourceInfo.completeBaseName();
                                    if (sourceBaseName.toUpper().endsWith("_P")) {
                                        sourceBaseName = sourceBaseName.left(sourceBaseName.length() - 2);
                                    }

                                    QString cleanExt = ext;
                                    if (cleanExt.endsWith("-x")) {
                                        cleanExt = cleanExt.left(cleanExt.length() - 2);
                                    }

                                    QString newFileName;
                                    if (sourceBaseName == groupName) {

                                        newFileName = modFolderName + "_P." + cleanExt;
                                    } else {

                                        newFileName = sourceInfo.completeBaseName() + "." + cleanExt;
                                    }

                                    QString destFile = modFolder + "/" + newFileName;
                                    copyFileReplacing(sourceFile, destFile);
                                    qDebug() << "Copied and renamed (root archive file):" << sourceInfo.fileName() << "to" << newFileName;
                                }
                            }

                            copyExtraArchiveFiles(tempExtractDir, modFolder, false);

                            successCount++;
                        }
                    }

                    QDir(tempExtractDir).removeRecursively();
                }
            } else if (ext == "pak" || ext == "ucas" || ext == "utoc") {

                QString modBaseName = baseName;
                if (modBaseName.toUpper().endsWith("_P")) {
                    modBaseName = modBaseName.left(modBaseName.length() - 2);
                }
                if (m_installSkipGroups.contains(modBaseName))
                    continue;

                QStringList selectedForThisMod = m_selectedFiles.value(modBaseName, QStringList());
                bool isFileSelected = true;

                if (!selectedForThisMod.isEmpty()) {

                    isFileSelected = false;
                    QString fileExt = ext.toLower();

                    if (fileExt.endsWith("-x")) {
                        fileExt = fileExt.left(fileExt.length() - 2);
                    }
                    for (const QString& selectedFile : selectedForThisMod) {
                        QString selectedExt = QFileInfo(selectedFile).suffix().toLower();

                        if (selectedExt.endsWith("-x")) {
                            selectedExt = selectedExt.left(selectedExt.length() - 2);
                        }
                        if (fileExt == selectedExt) {
                            isFileSelected = true;
                            break;
                        }
                    }
                }

                if (isFileSelected) {

                    QString finalModName = modBaseName;
                    if (m_modNameMappings.contains(modBaseName)) {
                        finalModName = m_modNameMappings[modBaseName];
                    }

                    finalModName = ensurePSuffix(finalModName);
                    QString finalFileName = finalModName + "." + ext;

                    QString destPath = m_modsPathInternal + "/" + finalFileName;

                    if (copyFileReplacing(filePath, destPath)) {
                        successCount++;
                    }
                }
            } else if (QFileInfo(filePath).isDir()) {

                QVariantList folderMappings;
                int folderGroupCount = 0;
                for (const QVariant& mappingValue : modMappings) {
                    const QVariantMap mapping = mappingValue.toMap();
                    const QString sourceFolder = mapping.value("sourceFolder").toString();
                    if (sourceFolder.isEmpty())
                        continue;
                    if (QDir::cleanPath(QFileInfo(sourceFolder).absoluteFilePath())
                            .compare(QDir::cleanPath(QFileInfo(filePath).absoluteFilePath()),
                                     Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    ++folderGroupCount;
                    if (!mapping.value("install", true).toBool())
                        continue;
                    folderMappings.append(mapping);
                }

                if (folderGroupCount > 1) {
                    QDir sourceDir2(filePath);
                    QHash<QString, QString> sourceByName;
                    QDirIterator sourceIt(filePath,
                                          {"*.pak", "*.pak-x", "*.ucas", "*.ucas-x",
                                           "*.utoc", "*.utoc-x"},
                                          QDir::Files, QDirIterator::Subdirectories);
                    while (sourceIt.hasNext()) {
                        const QString source = sourceIt.next();
                        sourceByName.insert(QFileInfo(source).fileName().toLower(), source);
                    }

                    for (const QVariant& mappingValue : folderMappings) {
                        const QVariantMap mapping = mappingValue.toMap();
                        const QString originalName = mapping.value("originalName").toString();
                        const QString updateOf = mapping.value("updateOf").toString();
                        const QString newName = mapping.value("newName").toString();
                        QString installName = updateOf.isEmpty() ? newName : updateOf;
                        if (installName.isEmpty())
                            installName = originalName;

                        QString relativeFolder = m_installDestFolders.value(originalName);
                        if (relativeFolder.isEmpty()) {
                            QString leaf = sanitizedBundleFolder(installName);
                            if (leaf.isEmpty())
                                leaf = originalName;
                            const QString bundle = sanitizedBundleFolder(
                                mapping.value("bundleFolder").toString());
                            relativeFolder = bundle.isEmpty()
                                ? leaf : bundle + QLatin1Char('/') + leaf;
                        }
                        const QString modFolder = QDir(m_modsPathInternal).filePath(relativeFolder);
                        QDir().mkpath(modFolder);

                        int copied = 0;
                        for (const QVariant& fileValue : mapping.value("files").toList()) {
                            const QVariantMap file = fileValue.toMap();
                            if (!file.value("selected", true).toBool())
                                continue;
                            const QString source = sourceByName.value(
                                file.value("name").toString().toLower());
                            if (source.isEmpty())
                                continue;

                            const QFileInfo sourceInfo(source);
                            QString extension = sourceInfo.suffix().toLower();
                            bool disabled = false;
                            if (extension.endsWith(QLatin1String("-x"))) {
                                disabled = true;
                                extension.chop(2);
                            }
                            QString sourceBase = sourceInfo.completeBaseName();
                            if (sourceBase.endsWith(QLatin1String("-x"), Qt::CaseInsensitive))
                                sourceBase.chop(2);
                            if (sourceBase.toUpper().endsWith(QLatin1String("_P")))
                                sourceBase.chop(2);

                            QString targetName = sourceInfo.fileName();
                            if (sourceBase.compare(originalName, Qt::CaseInsensitive) == 0
                                || sourceBase.compare(installName, Qt::CaseInsensitive) == 0) {
                                QString targetBase = installName;
                                if (targetBase.toUpper().endsWith(QLatin1String("_P")))
                                    targetBase.chop(2);
                                targetName = targetBase + QLatin1String("_P.") + extension;
                                if (disabled)
                                    targetName += QLatin1String("-x");
                            }

                            if (copyFileReplacing(source, QDir(modFolder).filePath(targetName)))
                                ++copied;
                        }

                        if (copied == 0)
                            continue;

                        QDirIterator extraIt(filePath, QDir::Files,
                                             QDirIterator::Subdirectories);
                        while (extraIt.hasNext()) {
                            const QString extra = extraIt.next();
                            const QFileInfo extraInfo(extra);
                            if (isUnrealModExtension(extraInfo.suffix()))
                                continue;
                            const QString relative = sourceDir2.relativeFilePath(extra);
                            const QString destExtra = QDir(modFolder).filePath(relative);
                            QDir().mkpath(QFileInfo(destExtra).absolutePath());
                            copyFileReplacing(extra, destExtra);
                        }

                        ++successCount;
                        emit installProgress(QString("Installed %1 file(s) for %2")
                                                 .arg(copied)
                                                 .arg(originalName));
                    }
                    continue;
                }

                ModInstallPlanner::Context planContext;
                planContext.modsPath = m_modsPathInternal;
                planContext.gamePaksPath = m_gamePaksPath;
                planContext.luaModsRoot = luaModsRoot;
                planContext.cppModsRoot = cppModsRoot;
                planContext.installName = QFileInfo(filePath).fileName();
                const ModInstallPlanner::Result installPlan =
                    ModInstallPlanner::analyze(filePath, planContext);

                if (installPlan.recognized) {
                    if (planHasPakGroups(installPlan))
                        paksInstalledArchives.insert(QFileInfo(filePath).absoluteFilePath());
                    if (!installPlan.valid) {
                        installErrors.append(installPlan.error.isEmpty()
                            ? QStringLiteral("Install destination could not be resolved.")
                            : installPlan.error);
                    } else {
                        for (const QVariant& groupValue : installPlan.groups) {
                            const QVariantMap group = groupValue.toMap();
                            const QString originalName = group.value("originalName").toString();
                            if (!m_modNameMappings.contains(originalName))
                                continue;
                            const QStringList selectedFiles = m_selectedFiles.value(originalName, QStringList());
                            const QString folderName = m_modNameMappings.value(originalName, originalName);
                            const QString relativeFolder = m_installDestFolders.value(originalName, folderName);
                            const PlannerFolders planned = plannerFoldersFor(
                                group, folderName, relativeFolder, m_installBundleSources.keys());
                            int copiedFiles = 0;
                            QString copyError;
                            if (ModInstallPlanner::installGroup(group, selectedFiles, planned.leaf,
                                                               planned.prefix, &copiedFiles, &copyError)) {
                                ++successCount;
                                if (groupHasCppRoot(group))
                                    cppInstalled = true;
                                emit installProgress(QString("Installed %1 file(s) for %2")
                                                         .arg(copiedFiles)
                                                         .arg(originalName));
                            } else {
                                installErrors.append(copyError.isEmpty()
                                    ? QStringLiteral("Failed to install %1").arg(originalName)
                                    : copyError);
                            }
                        }
                    }
                    continue;
                }

                QFileInfo folderInfo(filePath);
                QString folderName = folderInfo.fileName();

                QDir sourceDir(filePath);
                QDirIterator modFileIt(filePath, { "*.pak", "*.pak-x", "*.ucas", "*.ucas-x", "*.utoc", "*.utoc-x" }, QDir::Files, QDirIterator::Subdirectories);

                bool hasModFiles = false;
                if (modFileIt.hasNext()) {
                    hasModFiles = true;
                }

                if (!hasModFiles) {
                    continue;
                }

                QString finalModFolderName = folderName;
                if (m_modNameMappings.contains(folderName)) {
                    finalModFolderName = m_modNameMappings[folderName];
                }

                QString destModFolder = m_modsPathInternal + "/" + finalModFolderName;

                if (copyRecursively(filePath, destModFolder)) {
                    qDebug() << "Installed folder mod:" << folderName << "to" << destModFolder;
                    successCount++;
                }
            }
        }

        if (luaInstalled && !luaModsRoot.isEmpty()) {
            const QString luaError = registerLuaModsDisabled(luaModsRoot);
            if (!luaError.isEmpty()) {
                emit installProgress(
                    QStringLiteral("Lua mods were copied, but mods.json could not be updated: %1")
                        .arg(luaError));
            } else {
                for (const QString& luaName : luaEnableAfterInstall) {
                    const QString enableError = setLuaModEnabled(luaName, true, luaModsRoot);
                    if (!enableError.isEmpty()) {
                        emit installProgress(
                            QStringLiteral("Lua mod \"%1\" was installed but could not be enabled: %2")
                                .arg(luaName, enableError));
                        break;
                    }
                }
            }
        }

        if (cppInstalled && !cppModsRoot.isEmpty()) {
            const QString cppError = setAvaloniumCppModsPath(cppModsRoot);
            if (!cppError.isEmpty()) {
                emit installProgress(
                    QStringLiteral("C++ mods were copied, but Avalonium.ini could not be updated: %1")
                        .arg(cppError));
            }
        }

        QMetaObject::invokeMethod(this, [this, successCount, totalFiles, installedByArchive,
                                         installErrors, paksInstalledArchives, luaInstalled,
                                         renameAfterInstall, aliasRecords, updatedTargets,
                                         installBundleSources]() {
            m_batchOperationInProgress = false;

            emit installProgress("Finalizing installation...");

            if (installErrors.isEmpty() && successCount > 0) {
                if (!installedByArchive.isEmpty()) {
                    for (auto it = installedByArchive.cbegin(); it != installedByArchive.cend(); ++it)
                        linkNexusInstalledMods(QStringList{it.key()}, it.value());
                } else if (paksInstalledArchives.isEmpty() && !m_modNameMappings.isEmpty()) {
                    linkNexusInstalledMods(m_pendingInstallFiles, m_modNameMappings.values());
                }

                for (const QString& archive : paksInstalledArchives) {
                    if (!installedByArchive.contains(QFileInfo(archive).absoluteFilePath()))
                        linkNexusInstalledMods(QStringList{archive}, QStringList());
                }
            }

            if (installErrors.isEmpty() && successCount > 0)
                removeLooseUpdateTargets(updatedTargets);

            scanMods(m_modsPathInternal);
            saveSettings();

            if (successCount > 0) {
                for (auto it = installBundleSources.cbegin();
                     it != installBundleSources.cend(); ++it) {
                    createBundleFromInstalledFolder(it.key(), it.value());
                }
            }
            m_installDestFolders.clear();
            m_installBundleSources.clear();

            if (installErrors.isEmpty() && successCount > 0)
                mergeModAliases(aliasRecords);

            if (installErrors.isEmpty() && successCount > 0) {
                for (auto it = renameAfterInstall.cbegin(); it != renameAfterInstall.cend(); ++it) {
                    bool conflicts = false;
                    for (const QVariant& modValue : m_modList) {
                        const QString existing = modValue.toMap().value("name").toString();
                        if (existing.compare(it.value(), Qt::CaseInsensitive) == 0
                            && existing.compare(it.key(), Qt::CaseInsensitive) != 0) {
                            conflicts = true;
                            break;
                        }
                    }
                    if (conflicts) {
                        qDebug() << "update rename skipped, name already exists:" << it.value();
                        continue;
                    }
                    renameMod(it.key(), it.value());
                }
            }

            m_pendingInstallFiles.clear();
            m_modNameMappings.clear();
            m_selectedFiles.clear();
            m_installSkipGroups.clear();

            QTimer::singleShot(300, this, [this, successCount, totalFiles, installErrors,
                                           paksInstalledArchives, luaInstalled]() {
                m_lastInstalledModCount = installErrors.isEmpty() ? successCount : 0;
                emit lastInstalledModCountChanged();

                if (!installErrors.isEmpty()) {
                    emit installComplete(false, installErrors.join(QStringLiteral("\n")));
                    return;
                }
                if (!paksInstalledArchives.isEmpty()) {
                    emit installComplete(true,
                        luaInstalled
                            ? QStringLiteral("Installed %1 mod(s) to the game's Paks folders and the Lua mods profile")
                                  .arg(successCount)
                            : QStringLiteral("Installed %1 mod(s) to the game's Paks folders")
                                  .arg(successCount));
                    return;
                }
                if (luaInstalled) {
                    emit installComplete(true, QStringLiteral("Installed %1 Lua mod(s)")
                                                  .arg(successCount));
                    return;
                }
                emit installComplete(true, QString("Successfully installed %1 mod(s) from %2 file(s)")
                                              .arg(successCount)
                                              .arg(totalFiles));
            });
        }, Qt::QueuedConnection);
    });
}

QMap<QString, QStringList> ModManager::looseModExtensions() const
{
    QMap<QString, QStringList> result;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        if (mod.value("type").toString() != QLatin1String("loose"))
            continue;

        const QString key = mmdetail::normalizedModKey(mod.value("name").toString());
        QString extension = QFileInfo(mod.value("fileName").toString()).suffix().toLower();
        if (extension.endsWith(QLatin1String("-x")))
            extension.chop(2);
        if (key.isEmpty() || (extension != QLatin1String("pak")
                              && extension != QLatin1String("ucas")
                              && extension != QLatin1String("utoc")))
            continue;

        QStringList& extensions = result[key];
        if (!extensions.contains(extension))
            extensions.append(extension);
    }
    return result;
}

void ModManager::removeLooseUpdateTargets(const QStringList& targets)
{
    if (targets.isEmpty() || m_modsPathInternal.isEmpty())
        return;

    QSet<QString> looseKeys;
    for (const QVariant& modValue : m_modList) {
        const QVariantMap mod = modValue.toMap();
        if (mod.value("type").toString() != QLatin1String("loose"))
            continue;
        const QString name = mod.value("name").toString();
        for (const QString& target : targets) {
            if (name.compare(target, Qt::CaseInsensitive) == 0) {
                looseKeys.insert(mmdetail::normalizedModKey(name));
                break;
            }
        }
    }
    if (looseKeys.isEmpty())
        return;

    QDir modsRoot(m_modsPathInternal);
    const QStringList folders = modsRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QSet<QString> folderKeys;
    for (const QString& folder : folders)
        folderKeys.insert(mmdetail::normalizedModKey(folder));

    const QStringList files = modsRoot.entryList(QDir::Files);
    for (const QString& fileName : files) {
        if (!mmdetail::isUnrealModExtension(QFileInfo(fileName).suffix()))
            continue;

        const QString key = mmdetail::normalizedModKey(fileName);
        if (!looseKeys.contains(key) || !folderKeys.contains(key))
            continue;

        const QString path = modsRoot.absoluteFilePath(fileName);
        const bool removed = m_deleteToRecycleBin ? QFile::moveToTrash(path)
                                                  : QFile::remove(path);
        if (removed)
            qDebug() << "update: removed loose file replaced by installed mod:" << path;
    }
}

void ModManager::cancelModInstallation() {
    m_pendingInstallFiles.clear();
    m_modNameMappings.clear();
    emit installComplete(false, "Not cool, installation cancelled by YOU!!");
}

QString ModManager::formatFileSize(qint64 size) {
    if (size < 1024)
        return QString("%1 B").arg(size);
    if (size < 1024 * 1024)
        return QString("%1 KB").arg(QString::number(size / 1024.0, 'f', 1));
    if (size < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(QString::number(size / (1024.0 * 1024.0), 'f', 1));
    return QString("%1 GB").arg(QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 1));
}

bool ModManager::extractArchive(const QString& archivePath, const QString& destPath) {
    if (m_sevenZipPath.isEmpty()) {
        qDebug() << "ERROR: 7-Zip/WinRAR path is not configured";
        emit installProgress("ERROR: 7-Zip/WinRAR path is not configured. Go to Settings to set it.");
        return false;
    }

    QProcess process;
    QStringList args;

    if (m_sevenZipPath.contains("7z.exe", Qt::CaseInsensitive)) {
        args << "x" << archivePath << QString("-o%1").arg(destPath) << "-y";
    } else if (m_sevenZipPath.contains("WinRAR.exe", Qt::CaseInsensitive)) {
        args << "x" << "-ibck" << archivePath << destPath;
    } else {
        qDebug() << "ERROR: Unknown archive tool:" << m_sevenZipPath;
        return false;
    }

    qDebug() << "Extracting with command:" << m_sevenZipPath << args.join(" ");

    process.start(m_sevenZipPath, args);
    if (!process.waitForFinished(60000)) {
        qDebug() << "ERROR: Extraction process timed out or failed to start";
        qDebug() << "Process error:" << process.errorString();
        return false;
    }

    if (process.exitCode() != 0) {
        qDebug() << "ERROR: Extraction failed with exit code:" << process.exitCode();
        qDebug() << "Standard output:" << process.readAllStandardOutput();
        qDebug() << "Standard error:" << process.readAllStandardError();
        return false;
    }

    qDebug() << "Extraction successful";
    return true;
}

void ModManager::processModFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();

    if (ext != "pak" && ext != "ucas" && ext != "utoc") {
        return;
    }

    QString baseName = fileInfo.completeBaseName();
    QString fileName = fileInfo.fileName();

    baseName = ensurePSuffix(baseName);
    fileName = baseName + "." + ext;

    QString destPath = m_modsPathInternal + "/" + fileName;

    copyFileReplacing(filePath, destPath);
}
