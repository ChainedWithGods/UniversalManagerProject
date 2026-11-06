#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::resolveExistingPakViewerPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString projectDir = findProjectRoot();
    QString configuredPath = m_pakViewerPath;

    if (QFileInfo(configuredPath).isDir()) {
        const QString exeInFolder = QDir(configuredPath).filePath("PakViewer.exe");
        configuredPath = QFile::exists(exeInFolder) ? exeInFolder : QString();
    }
    const QStringList possiblePaths = {
        QDir(projectDir).filePath("Tools/PakViewer/bin/x64/Release/net10.0-windows10.0.19041.0/win-x64/PakViewer.exe"),
        QDir(projectDir).filePath("Tools/PakViewer/PakViewer.exe"),
        QDir(projectDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/win-x64/PakViewer.exe"),
        QDir(projectDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(projectDir).filePath("Tools/PakViewer/bin/Debug/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(projectDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/publish/PakViewer.exe"),
        QDir(appDir).filePath("PakViewer/PakViewer.exe"),
        QDir(appDir).filePath("Tools/PakViewer/PakViewer.exe"),
        QDir(appDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/win-x64/PakViewer.exe"),
        QDir(appDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(appDir).filePath("Tools/PakViewer/bin/Debug/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(appDir).filePath("Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/publish/PakViewer.exe"),
        QDir(appDir).filePath("../../Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/win-x64/PakViewer.exe"),
        QDir(appDir).filePath("../../Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(appDir).filePath("../../Tools/PakViewer/bin/Debug/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(appDir).filePath("../Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/win-x64/PakViewer.exe"),
        QDir(appDir).filePath("../Tools/PakViewer/bin/Release/net10.0-windows10.0.19041.0/PakViewer.exe"),
        QDir(appDir).filePath("../Tools/PakViewer/bin/Debug/net10.0-windows10.0.19041.0/PakViewer.exe")
    };
    return resolveConfiguredOrKnownToolPath(configuredPath, possiblePaths);
}

void ModManager::autoDetectPakViewerPath() {
    const QString detectedPath = resolveExistingPakViewerPath();
    if (!detectedPath.isEmpty()) {
        setPakViewerPath(detectedPath);
    }
}

QString ModManager::getModPakPath(const QString& modName) {
    if (m_modsPathInternal.isEmpty()) {
        qWarning() << "Mods path not set";
        return QString();
    }

    auto isSupportedModFile = [](const QString& path) -> bool {
        QString lower = path.toLower();
        return lower.endsWith(".pak") || lower.endsWith(".pak-x") ||
               lower.endsWith(".ucas") || lower.endsWith(".ucas-x") ||
               lower.endsWith(".utoc") || lower.endsWith(".utoc-x");
    };

    QString cacheKey = modName;
    if (!m_modDetailsCache.contains(cacheKey)) {
        const QString normalizedSearch = normalizedDisplayModName(modName);
        for (auto it = m_modDetailsCache.cbegin(); it != m_modDetailsCache.cend(); ++it) {
            if (normalizedDisplayModName(it.key()) == normalizedSearch) {
                cacheKey = it.key();
                break;
            }
        }
    }

    if (m_modDetailsCache.contains(cacheKey)) {
        QVariantMap details = m_modDetailsCache[cacheKey];
        QString cachedPath = details["path"].toString();
        QString modType = details["type"].toString();

        if (!cachedPath.isEmpty()) {
            QFileInfo modInfo(cachedPath);

            if (modType == "loose" && modInfo.isFile()) {
                if (isSupportedModFile(cachedPath)) {
                    qDebug() << "Found cached loose file:" << cachedPath;
                    return cachedPath;
                }
            }

            if (modType == "folder" && modInfo.isDir()) {
                QDir modDir(cachedPath);

                QStringList utocFiles = modDir.entryList(QStringList() << "*.utoc", QDir::Files);
                if (utocFiles.isEmpty()) {
                    utocFiles = modDir.entryList(QStringList() << "*.utoc-x", QDir::Files);
                }
                if (!utocFiles.isEmpty()) {
                    for (const QString& utocFile : utocFiles) {
                        QString utocPath = modDir.filePath(utocFile);
                        QString base = utocFile;
                        bool disabled = false;
                        if (base.endsWith("-x", Qt::CaseInsensitive)) {
                            base.chop(2);
                            disabled = true;
                        }
                        if (base.endsWith(".utoc", Qt::CaseInsensitive)) {
                            base.chop(5);
                        }
                        const QString ucasName = base + (disabled ? ".ucas-x" : ".ucas");
                        const QString ucasPath = modDir.filePath(ucasName);
                        if (QFile::exists(ucasPath)) {
                            qDebug() << "Found IoStore container in cached folder:" << utocPath;
                            return utocPath;
                        }
                    }
                }

                QStringList pakFiles = modDir.entryList(QStringList() << "*.pak", QDir::Files);
                if (pakFiles.isEmpty()) {
                    pakFiles = modDir.entryList(QStringList() << "*.pak-x", QDir::Files);
                }
                if (!pakFiles.isEmpty()) {
                    QString pakPath = modDir.filePath(pakFiles.first());
                    qDebug() << "Found pak file in cached folder:" << pakPath;
                    return pakPath;
                }

                QDirIterator pakIt(cachedPath,
                                   QStringList() << "*.pak" << "*.pak-x",
                                   QDir::Files,
                                   QDirIterator::Subdirectories);
                if (pakIt.hasNext()) {
                    QString pakPath = pakIt.next();
                    qDebug() << "Found pak file in subfolder:" << pakPath;
                    return pakPath;
                }

                QDirIterator utocIt(cachedPath,
                                    QStringList() << "*.utoc" << "*.utoc-x",
                                    QDir::Files,
                                    QDirIterator::Subdirectories);
                while (utocIt.hasNext()) {
                    QString utocPath = utocIt.next();
                    QFileInfo utocInfo(utocPath);
                    QString base = utocInfo.fileName();
                    bool disabled = false;
                    if (base.endsWith("-x", Qt::CaseInsensitive)) {
                        base.chop(2);
                        disabled = true;
                    }
                    if (base.endsWith(".utoc", Qt::CaseInsensitive)) {
                        base.chop(5);
                    }
                    const QString ucasName = base + (disabled ? ".ucas-x" : ".ucas");
                    const QString ucasPath = QDir(utocInfo.absolutePath()).filePath(ucasName);
                    if (QFile::exists(ucasPath)) {
                        qDebug() << "Found IoStore container in subfolder:" << utocPath;
                        return utocPath;
                    }
                }
            }
        }
    }

    qDebug() << "getModPakPath: Fallback search for mod:" << modName << "in" << m_modsPathInternal;

    QString normalizedModsPath = QDir::toNativeSeparators(m_modsPathInternal);
    QDir modsDir(normalizedModsPath);

    if (!modsDir.exists()) {
        qWarning() << "getModPakPath: Mods directory does not exist:" << normalizedModsPath;
        return QString();
    }

    qDebug() << "getModPakPath: Searching in directory:" << modsDir.absolutePath();

    QStringList allFiles = modsDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    qDebug() << "getModPakPath: Total files in directory:" << allFiles.count();
    if (allFiles.count() > 0 && allFiles.count() <= 20) {
        qDebug() << "getModPakPath: Files:" << allFiles;
    }

    for (const QString& fileName : allFiles) {
        QString lowerFileName = fileName.toLower();
        QString lowerModName = modName.toLower();

        bool isMatch = false;
        QString baseName;
        QString extension;

        if (lowerFileName.endsWith(".utoc-x")) {
            baseName = fileName.left(fileName.length() - 7);
            extension = ".utoc-x";
        } else if (lowerFileName.endsWith(".utoc")) {
            baseName = fileName.left(fileName.length() - 5);
            extension = ".utoc";
        } else if (lowerFileName.endsWith(".ucas-x")) {
            baseName = fileName.left(fileName.length() - 7);
            extension = ".ucas-x";
        } else if (lowerFileName.endsWith(".ucas")) {
            baseName = fileName.left(fileName.length() - 5);
            extension = ".ucas";
        } else if (lowerFileName.endsWith(".pak-x")) {
            baseName = fileName.left(fileName.length() - 6);
            extension = ".pak-x";
        } else if (lowerFileName.endsWith(".pak")) {
            baseName = fileName.left(fileName.length() - 4);
            extension = ".pak";
        } else {
            continue;
        }

        QString lowerBaseName = baseName.toLower();
        if (lowerBaseName == lowerModName) {
            isMatch = true;
        } else if (lowerModName.endsWith("_p") && lowerBaseName == lowerModName.left(lowerModName.length() - 2)) {
            isMatch = true;
        } else if (lowerBaseName.endsWith("_p") && lowerBaseName.left(lowerBaseName.length() - 2) == lowerModName) {
            isMatch = true;
        }

        if (isMatch) {
            qDebug() << "getModPakPath: Found matching file:" << fileName << "for mod:" << modName;

            if (extension == ".utoc" || extension == ".utoc-x") {
                QString utocPath = modsDir.filePath(fileName);
                bool disabled = extension == ".utoc-x";
                QString ucasName = baseName + (disabled ? ".ucas-x" : ".ucas");
                QString ucasPath = modsDir.filePath(ucasName);

                bool ucasExists = false;
                for (const QString& f : allFiles) {
                    if (f.compare(ucasName, Qt::CaseInsensitive) == 0) {
                        ucasExists = true;
                        break;
                    }
                }

                if (ucasExists) {
                    qDebug() << "getModPakPath: Found IoStore container:" << utocPath;
                    return utocPath;
                }
            }

            else if (extension == ".pak" || extension == ".pak-x") {
                QString pakPath = modsDir.filePath(fileName);
                qDebug() << "getModPakPath: Found pak file:" << pakPath;
                return pakPath;
            }
        }
    }

    QStringList subDirs = modsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        QString lowerSubDir = subDir.toLower();
        QString lowerModName = modName.toLower();

        if (lowerSubDir == lowerModName ||
            (lowerModName.endsWith("_p") && lowerSubDir == lowerModName.left(lowerModName.length() - 2)) ||
            (lowerSubDir.endsWith("_p") && lowerSubDir.left(lowerSubDir.length() - 2) == lowerModName)) {

            QDir subModDir(modsDir.filePath(subDir));
            QStringList subFiles = subModDir.entryList(QDir::Files);

            for (const QString& f : subFiles) {
                if (f.endsWith(".utoc", Qt::CaseInsensitive) || f.endsWith(".utoc-x", Qt::CaseInsensitive)) {
                    QString utocPath = subModDir.filePath(f);
                    bool disabled = f.endsWith("-x", Qt::CaseInsensitive);
                    QString base = f;
                    if (disabled) base.chop(7); else base.chop(5);
                    QString ucasName = base + (disabled ? ".ucas-x" : ".ucas");

                    for (const QString& f2 : subFiles) {
                        if (f2.compare(ucasName, Qt::CaseInsensitive) == 0) {
                            qDebug() << "getModPakPath: Found IoStore in subfolder:" << utocPath;
                            return utocPath;
                        }
                    }
                }
            }

            for (const QString& f : subFiles) {
                if (f.endsWith(".pak", Qt::CaseInsensitive) || f.endsWith(".pak-x", Qt::CaseInsensitive)) {
                    QString pakPath = subModDir.filePath(f);
                    qDebug() << "getModPakPath: Found pak in subfolder:" << pakPath;
                    return pakPath;
                }
            }
        }
    }

    const QString normalizedSearchName = normalizedDisplayModName(modName);
    QDirIterator nestedDirIt(modsDir.absolutePath(),
                             QDir::Dirs | QDir::NoDotAndDotDot,
                             QDirIterator::Subdirectories);
    while (nestedDirIt.hasNext()) {
        const QString folderPath = nestedDirIt.next();
        if (normalizedDisplayModName(QFileInfo(folderPath).fileName()) != normalizedSearchName) {
            continue;
        }

        QDir nestedDir(folderPath);
        const QStringList utocFiles = nestedDir.entryList(QStringList() << "*.utoc" << "*.utoc-x", QDir::Files);
        for (const QString& utocFile : utocFiles) {
            const bool disabled = utocFile.endsWith("-x", Qt::CaseInsensitive);
            QString base = utocFile;
            if (disabled) base.chop(2);
            if (base.endsWith(".utoc", Qt::CaseInsensitive)) base.chop(5);
            if (QFile::exists(nestedDir.filePath(base + (disabled ? ".ucas-x" : ".ucas")))) {
                qDebug() << "getModPakPath: Found IoStore in nested folder:" << nestedDir.filePath(utocFile);
                return nestedDir.filePath(utocFile);
            }
        }

        const QStringList pakFiles = nestedDir.entryList(QStringList() << "*.pak" << "*.pak-x", QDir::Files);
        if (!pakFiles.isEmpty()) {
            qDebug() << "getModPakPath: Found pak in nested folder:" << nestedDir.filePath(pakFiles.first());
            return nestedDir.filePath(pakFiles.first());
        }
    }

    qWarning() << "No .pak or .utoc file found for mod:" << modName;
    return QString();
}

QString ModManager::pakViewerPath() const {
    return m_pakViewerPath;
}

bool ModManager::pakViewerAvailable() const {
    return !resolveExistingPakViewerPath().isEmpty();
}

QString ModManager::pakViewerStatusText() const {
    const QString resolvedPath = resolveExistingPakViewerPath();
    return resolvedPath.isEmpty()
        ? missingToolStatusText(m_pakViewerPath, "PakViewer")
        : resolvedPath;
}

void ModManager::setPakViewerPath(const QString& path) {
    if (m_pakViewerPath != path) {
        m_pakViewerPath = path;
        emit pakViewerPathChanged();
        saveSettings();
    }
}

void ModManager::launchPakViewer(const QString& pakFilePath, const QStringList& extraArgs) {
    QString viewerPath = resolveExistingPakViewerPath();

    if (viewerPath.isEmpty() || !QFile::exists(viewerPath)) {
        QString appDir = QCoreApplication::applicationDirPath();
        qWarning() << "PakViewer executable not found";
        qWarning() << "Application directory:" << appDir;
        qWarning() << "Searched path:" << (viewerPath.isEmpty() ? "No valid path found" : viewerPath);
        emit installProgress("Error: PakViewer not found. Build Tools/PakViewer.exe or set path in Settings.");
        return;
    }

    QStringList arguments;

    if (!m_gamePaksPath.isEmpty()) {
        arguments << "--paks-path" << m_gamePaksPath;
    }

    if (!m_usmapFilePath.isEmpty()) {
        arguments << "--usmap-file" << m_usmapFilePath;
    }

    if (!m_aesKey.isEmpty()) {
        arguments << "--aes-key" << m_aesKey;
    }

    if (!m_ueVersion.isEmpty()) {
        QString enumVersion = convertToEnumFormat(m_ueVersion);
        arguments << "--ue-version" << enumVersion;
    }

    QString gameName;
    const int activeIndex = findPresetIndexById(m_activePresetId);
    if (activeIndex >= 0) {
        const QVariantMap preset = m_gamePresets[activeIndex].toMap();
        gameName = preset.value("name").toString().trimmed();
    }
    if (!gameName.isEmpty()) {
        arguments << "--game-name" << gameName;
    }

    if (m_pakViewerHideConsole) {
        arguments << "--hide-console";
    }

    if (m_pakViewerHideUI) {
        arguments << "--minimal-ui";
    }

    if (!pakFilePath.isEmpty()) {
        arguments << "--open-file" << pakFilePath;
    }

    if (!extraArgs.isEmpty()) {
        arguments << extraArgs;
    }

    qDebug() << "Launching PakViewer:" << viewerPath;
    qDebug() << "Arguments:" << arguments;

    const QString workingDir = QFileInfo(viewerPath).absolutePath();

    qint64 pid = 0;
    bool success = false;
    if (isPathOnSystemDrive(viewerPath)) {
        qDebug() << "PakViewer is on the system drive - launching elevated (UAC will appear if not already admin)";
        success = launchDetachedElevated(viewerPath, arguments, workingDir);
    } else {
        success = QProcess::startDetached(viewerPath, arguments, workingDir, &pid);
    }

    if (!success) {
        qWarning() << "Failed to launch PakViewer at:" << viewerPath;
        emit installProgress("Error: Failed to launch PakViewer");
    } else {
        qDebug() << "Successfully launched PakViewer with PID:" << pid;
    }
}

void ModManager::launchPakViewerStandalone() {
    QString viewerPath = resolveExistingPakViewerPath();

    if (viewerPath.isEmpty() || !QFile::exists(viewerPath)) {
        QString appDir = QCoreApplication::applicationDirPath();
        qWarning() << "PakViewer executable not found";
        qWarning() << "Application directory:" << appDir;
        qWarning() << "Searched path:" << (viewerPath.isEmpty() ? "No valid path found" : viewerPath);
        emit installProgress("Error: PakViewer not found. Build Tools/PakViewer.exe or set path in Settings.");
        return;
    }

    QStringList arguments;

    if (!m_gamePaksPath.isEmpty()) {
        arguments << "--paks-path" << m_gamePaksPath;
    }

    if (!m_usmapFilePath.isEmpty()) {
        arguments << "--usmap-file" << m_usmapFilePath;
    }

    if (!m_aesKey.isEmpty()) {
        arguments << "--aes-key" << m_aesKey;
    }

    if (!m_ueVersion.isEmpty()) {
        QString enumVersion = convertToEnumFormat(m_ueVersion);
        arguments << "--ue-version" << enumVersion;
    }

    QString gameName;
    const int activeIndex = findPresetIndexById(m_activePresetId);
    if (activeIndex >= 0) {
        const QVariantMap preset = m_gamePresets[activeIndex].toMap();
        gameName = preset.value("name").toString().trimmed();
    }
    if (!gameName.isEmpty()) {
        arguments << "--game-name" << gameName;
    }

    if (m_pakViewerHideConsole) {
        arguments << "--hide-console";
    }

    qDebug() << "Launching PakViewer (standalone mode):" << viewerPath;
    qDebug() << "Arguments:" << arguments;

    const QString workingDir = QFileInfo(viewerPath).absolutePath();

    qint64 pid = 0;
    bool success = false;
    if (isPathOnSystemDrive(viewerPath)) {
        qDebug() << "PakViewer is on the system drive - launching elevated (UAC will appear if not already admin)";
        success = launchDetachedElevated(viewerPath, arguments, workingDir);
    } else {
        success = QProcess::startDetached(viewerPath, arguments, workingDir, &pid);
    }

    if (!success) {
        qWarning() << "Failed to launch PakViewer at:" << viewerPath;
        emit installProgress("Error: Failed to launch PakViewer");
    } else {
        qDebug() << "Successfully launched PakViewer (standalone) with PID:" << pid;
    }
}

void ModManager::exportModStructure(const QString& modName) {
    QString pakPath = getModPakPath(modName);

    if (pakPath.isEmpty()) {
        qWarning() << "Could not find pak file for mod:" << modName;
        emit modStructureExported(false, "");
        return;
    }

    qDebug() << "Exporting structure for mod:" << modName;
    qDebug() << "Pak file:" << pakPath;

    QString viewerPath = resolveExistingPakViewerPath();

    if (viewerPath.isEmpty() || !QFile::exists(viewerPath)) {
        qWarning() << "PakViewer not found - cannot export mod structure";
        emit modStructureExported(false, "");
        return;
    }

    QString outputPath;
    if (!m_downloadsPath.isEmpty()) {
        QDir downloadsDir(m_downloadsPath);
        downloadsDir.mkpath(".");
        outputPath = downloadsDir.filePath(modName + "_structure.txt");
    } else {
        QFileInfo pakInfo(pakPath);
        outputPath = pakInfo.absolutePath() + "/" + modName + "_structure.txt";
    }

    QString gameName;
    int activeIndex = findPresetIndexById(m_activePresetId);
    if (activeIndex >= 0) {
        QVariantMap preset = m_gamePresets[activeIndex].toMap();
        gameName = preset.value("name").toString();
    }
    if (gameName.isEmpty()) {
        gameName = "Unreal Engine Game";
    }

    QStringList arguments;

    if (!m_gamePaksPath.isEmpty()) {
        arguments << "--paks-path" << m_gamePaksPath;
    }

    if (!m_usmapFilePath.isEmpty()) {
        arguments << "--usmap-file" << m_usmapFilePath;
    }

    if (!m_aesKey.isEmpty()) {
        arguments << "--aes-key" << m_aesKey;
    }

    if (!m_ueVersion.isEmpty()) {
        QString enumVersion = convertToEnumFormat(m_ueVersion);
        arguments << "--ue-version" << enumVersion;
    }

    arguments << "--game-name" << gameName;

    arguments << "--open-file" << pakPath;
    arguments << "--export-structure" << outputPath;
    arguments << "--headless";
    arguments << "--hide-console";

    qDebug() << "Running PakViewer with export args:" << arguments;

    const QString workingDir = QFileInfo(viewerPath).absolutePath();

    QProcess process;
    process.setWorkingDirectory(workingDir);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(viewerPath, arguments);

    if (!process.waitForFinished(30000)) {
        qWarning() << "PakViewer export timed out";
        emit modStructureExported(false, "");
        return;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());

    if (process.exitCode() != 0) {
        qWarning() << "PakViewer export failed with exit code:" << process.exitCode();
        qWarning() << "Output:" << output;
        emit modStructureExported(false, "");
        return;
    }

    if (QFile::exists(outputPath)) {
        qDebug() << "Successfully exported mod structure to:" << outputPath;
        emit modStructureExported(true, outputPath);
    } else {
        qWarning() << "Export completed but output file not found:" << outputPath;
        emit modStructureExported(false, "");
    }
}

void ModManager::launchPakViewerForMod(const QString& modName) {
    qDebug() << "launchPakViewerForMod: Launching PakViewer for mod" << modName;

    QString viewerPath = resolveExistingPakViewerPath();

    if (viewerPath.isEmpty() || !QFile::exists(viewerPath)) {
        qWarning() << "launchPakViewerForMod: PakViewer not found";
        emit conflictScanError(tr("PakViewer not found. Please configure the PakViewer path in Settings → General → Paths & Tools."));
        emit navigateToGeneralTab();
        return;
    }

    QString pakPath = getModPakPath(modName);

    if (pakPath.isEmpty()) {
        qWarning() << "launchPakViewerForMod: Could not find Pak file for mod" << modName;
        emit conflictScanError(tr("Could not find Pak file for mod: %1").arg(modName));
        return;
    }

    QJsonObject payloadObj;
    payloadObj["modName"] = modName;
    payloadObj["pakPath"] = pakPath;

    QJsonArray conflictsArray;
    QSet<QString> conflictingFilePaths;
    int highCount = 0, mediumCount = 0, lowCount = 0;

    auto matchModName = [](const QString& nameA, const QString& nameB) -> bool {
        QString a = nameA.trimmed();
        QString b = nameB.trimmed();
        if (a.endsWith(".pak", Qt::CaseInsensitive)) a = a.left(a.length() - 4);
        if (a.endsWith(".utoc", Qt::CaseInsensitive)) a = a.left(a.length() - 5);
        if (b.endsWith(".pak", Qt::CaseInsensitive)) b = b.left(b.length() - 4);
        if (b.endsWith(".utoc", Qt::CaseInsensitive)) b = b.left(b.length() - 5);
        return a.compare(b, Qt::CaseInsensitive) == 0;
    };

    for (const QVariant& cVar : m_conflictResults) {
        QVariantMap cMap = cVar.toMap();

        QStringList modsInConflict;
        if (cMap.contains("conflictingMods")) {
            modsInConflict = cMap["conflictingMods"].toStringList();
        } else if (cMap.contains("mods")) {
            modsInConflict = cMap["mods"].toStringList();
        }

        bool isTargetModInConflict = false;
        for (const QString& m : modsInConflict) {
            if (matchModName(m, modName)) {
                isTargetModInConflict = true;
                break;
            }
        }

        if (isTargetModInConflict) {
            QString filePath = cMap["filePath"].toString();
            QString severity = cMap["severity"].toString();
            conflictingFilePaths.insert(filePath);

            if (severity.compare("high", Qt::CaseInsensitive) == 0) highCount++;
            else if (severity.compare("medium", Qt::CaseInsensitive) == 0) mediumCount++;
            else lowCount++;

            QJsonObject cObj;
            cObj["filePath"] = filePath;
            const QString fType = cMap["fileType"].toString();
            cObj["fileType"] = fType.isEmpty() ? "Asset" : fType;
            cObj["severity"] = severity.isEmpty() ? "High" : severity;

            QJsonArray otherModsArray;
            for (const QString& m : modsInConflict) {
                if (!matchModName(m, modName)) {
                    otherModsArray.append(m);
                }
            }
            cObj["conflictingMods"] = otherModsArray;
            conflictsArray.append(cObj);
        }
    }

    if (conflictsArray.isEmpty() && !m_conflictGroups.isEmpty()) {
        for (const QVariant& gVar : m_conflictGroups) {
            QVariantMap gObj = gVar.toMap();
            QVariantList gMods = gObj["mods"].toList();

            bool targetModInGroup = false;
            QStringList groupModNames;
            QStringList targetModFiles;

            for (const QVariant& gmVar : gMods) {
                QVariantMap gmObj = gmVar.toMap();
                QString gName = gmObj["name"].toString();
                groupModNames.append(gName);
                if (matchModName(gName, modName)) {
                    targetModInGroup = true;
                    targetModFiles = gmObj["conflictingFiles"].toStringList();
                }
            }

            if (targetModInGroup) {
                QString severity = gObj["highestSeverity"].toString();
                if (severity.isEmpty()) severity = "High";

                for (const QString& fPath : targetModFiles) {
                    if (!conflictingFilePaths.contains(fPath)) {
                        conflictingFilePaths.insert(fPath);

                        if (severity.compare("high", Qt::CaseInsensitive) == 0) highCount++;
                        else if (severity.compare("medium", Qt::CaseInsensitive) == 0) mediumCount++;
                        else lowCount++;

                        QJsonObject cObj;
                        cObj["filePath"] = fPath;
                        cObj["fileType"] = "Asset";
                        cObj["severity"] = severity;

                        QJsonArray otherArr;
                        for (const QString& mn : groupModNames) {
                            if (!matchModName(mn, modName)) {
                                otherArr.append(mn);
                            }
                        }
                        cObj["conflictingMods"] = otherArr;
                        conflictsArray.append(cObj);
                    }
                }
            }
        }
    }

    QJsonArray newFilesArray;
    int totalFiles = 0;
    for (const QVariant& mVar : m_conflictModsData) {
        QVariantMap mObj = mVar.toMap();
        if (matchModName(mObj["name"].toString(), modName)) {
            QStringList files = mObj["files"].toStringList();
            totalFiles = files.size();
            for (const QString& f : files) {
                if (!conflictingFilePaths.contains(f)) {
                    QJsonObject nfObj;
                    nfObj["filePath"] = f;
                    nfObj["fileType"] = "Asset";
                    newFilesArray.append(nfObj);
                }
            }
            break;
        }
    }

    payloadObj["totalFiles"] = totalFiles > 0 ? totalFiles : (conflictsArray.size() + newFilesArray.size());
    payloadObj["conflictCount"] = conflictsArray.size();
    payloadObj["highSeverityCount"] = highCount;
    payloadObj["mediumSeverityCount"] = mediumCount;
    payloadObj["lowSeverityCount"] = lowCount;
    payloadObj["conflicts"] = conflictsArray;
    payloadObj["newFiles"] = newFilesArray;

    QString tempJsonPath = QDir::tempPath() + QString("/pakviewer_conflict_%1.json").arg(modName);
    QFile jsonFile(tempJsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(payloadObj).toJson(QJsonDocument::Indented));
        jsonFile.close();
        qDebug() << "launchPakViewerForMod: Written conflict payload to" << tempJsonPath;
    }

    QStringList extraArgs;
    extraArgs << "--conflict-mode" << "--conflict-mod" << modName << "--conflict-json" << tempJsonPath;

    launchPakViewer(pakPath, extraArgs);
}
