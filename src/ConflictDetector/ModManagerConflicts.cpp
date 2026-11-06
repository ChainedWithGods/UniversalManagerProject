#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::resolveExistingConflictDetectorPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString projectDir = findProjectRoot();
    const QStringList possiblePaths = {
        QDir(projectDir).filePath("Tools/ModConflictDetector/ModConflictDetector.exe"),
        QDir(projectDir).filePath("Tools/ModConflictDetector/bin/Release/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("ModConflictDetector.exe"),
        QDir(appDir).filePath("Tools/ModConflictDetector/ModConflictDetector.exe"),
        QDir(appDir).filePath("Tools/ModConflictDetector/bin/Release/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("Tools/ModConflictDetector/bin/Debug/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("../../Tools/ModConflictDetector/bin/Release/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("../../Tools/ModConflictDetector/bin/Debug/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("../Tools/ModConflictDetector/bin/Release/net10.0/ModConflictDetector.exe"),
        QDir(appDir).filePath("../Tools/ModConflictDetector/bin/Debug/net10.0/ModConflictDetector.exe")
    };
    return resolveConfiguredOrKnownToolPath(m_conflictDetectorPath, possiblePaths);
}

QString ModManager::resolveExistingUEDetectorPath() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString projectDir = findProjectRoot();
    const QStringList possiblePaths = {
        QDir(projectDir).filePath("Tools/UE_Detector/Idk/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/cmake-build-release/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/cmake-build-release/bin/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/out/build/release/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/out/build/debug/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/x64-release/bin/UE_Detector.exe"),
        QDir(projectDir).filePath("Tools/UE_Detector/x64-debug/bin/UE_Detector.exe"),
        QDir(appDir).filePath("UE_Detector.exe"),
        QDir(appDir).filePath("Tools/UE_Detector/UE_Detector.exe"),
        QDir(appDir).filePath("Tools/UE_Detector/Idk/UE_Detector.exe"),
        QDir(appDir).filePath("Tools/UE_Detector/cmake-build-release/UE_Detector.exe"),
        QDir(appDir).filePath("Tools/UE_Detector/cmake-build-release/bin/UE_Detector.exe"),
        QDir(appDir).filePath("../Tools/UE_Detector/UE_Detector.exe"),
        QDir(appDir).filePath("../Tools/UE_Detector/Idk/UE_Detector.exe"),
        QDir(appDir).filePath("../../Tools/UE_Detector/UE_Detector.exe"),
        QDir(appDir).filePath("../../Tools/UE_Detector/Idk/UE_Detector.exe")
    };
    return resolveConfiguredOrKnownToolPath(m_ueDetectorPath, possiblePaths);
}

bool ModManager::isConflictScanning() const {
    return m_isConflictScanning;
}

QString ModManager::conflictScanProgress() const {
    return m_conflictScanProgress;
}

QVariantList ModManager::conflictResults() const {
    return m_conflictResults;
}

QVariantList ModManager::conflictGroups() const {
    return m_conflictGroups;
}

QVariantList ModManager::conflictModsData() const {
    return m_conflictModsData;
}

QString ModManager::lastConflictScanTime() const {
    return m_lastConflictScanTime;
}

QStringList ModManager::conflictScanErrors() const {
    return m_conflictScanErrors;
}

QString ModManager::conflictDetectorPath() const {
    return m_conflictDetectorPath;
}

bool ModManager::conflictDetectorAvailable() const {
    return !resolveExistingConflictDetectorPath().isEmpty();
}

QString ModManager::conflictDetectorStatusText() const {
    const QString resolvedPath = resolveExistingConflictDetectorPath();
    return resolvedPath.isEmpty()
        ? missingToolStatusText(m_conflictDetectorPath, "ModConflictDetector")
        : resolvedPath;
}

void ModManager::setConflictDetectorPath(const QString& path) {
    if (m_conflictDetectorPath != path) {
        m_conflictDetectorPath = path;
        emit conflictDetectorPathChanged();
        saveSettings();
    }
}

void ModManager::autoDetectConflictDetectorPath() {
    const QString detectedPath = resolveExistingConflictDetectorPath();
    if (!detectedPath.isEmpty()) {
        setConflictDetectorPath(detectedPath);
    }
}

QString ModManager::ueDetectorPath() const {
    return m_ueDetectorPath;
}

bool ModManager::ueDetectorAvailable() const {
    return !resolveExistingUEDetectorPath().isEmpty();
}

QString ModManager::ueDetectorStatusText() const {
    const QString resolvedPath = resolveExistingUEDetectorPath();
    return resolvedPath.isEmpty()
        ? missingToolStatusText(m_ueDetectorPath, "Unreal Engine Version Detector")
        : resolvedPath;
}

void ModManager::setUeDetectorPath(const QString& path) {
    if (m_ueDetectorPath != path) {
        m_ueDetectorPath = path;
        emit ueDetectorPathChanged();
        saveSettings();
    }
}

void ModManager::autoDetectUEDetectorPath() {
    const QString detectedPath = resolveExistingUEDetectorPath();
    if (!detectedPath.isEmpty()) {
        setUeDetectorPath(detectedPath);
    }
}

void ModManager::scanConflicts(bool includeDisabled) {

    if (m_isConflictScanning) {
        qDebug() << "scanConflicts: Scan already in progress";
        return;
    }

    if (m_modsPath.isEmpty()) {
        emit conflictScanError(tr("Mods path is not configured. Please set it in Settings → General → Paths & Tools."));
        return;
    }

    QDir modsDir(m_modsPath);
    if (!modsDir.exists()) {
        emit conflictScanError(tr("Mods directory does not exist: %1").arg(m_modsPath));
        return;
    }

    QString cliPath = m_conflictDetectorPath;
    if (cliPath.isEmpty()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList possiblePaths = {
            QDir(appDir).filePath("Tools/ModConflictDetector/bin/Release/net1.0/ModConflictDetector.exe"),
            QDir(appDir).filePath("Tools/ModConflictDetector/bin/Debug/net1.0/ModConflictDetector.exe"),
            QDir(appDir).filePath("Tools/ModConflictDetector/ModConflictDetector.exe"),
            QDir(appDir).filePath("../../Tools/ModConflictDetector/bin/Release/net1.0/ModConflictDetector.exe"),
            QDir(appDir).filePath("../../Tools/ModConflictDetector/bin/Debug/net1.0/ModConflictDetector.exe"),
            QDir(appDir).filePath("../Tools/ModConflictDetector/bin/Release/net1.0/ModConflictDetector.exe"),
            QDir(appDir).filePath("../Tools/ModConflictDetector/bin/Debug/net1.0/ModConflictDetector.exe"),

        };

        for (const QString& path : possiblePaths) {
            QString cleanPath = QDir::cleanPath(path);
            if (QFile::exists(cleanPath)) {
                cliPath = cleanPath;
                break;
            }
        }

        if (!QFile::exists(cliPath)) {
            cliPath = appDir + "/ModConflictDetector.exe";
        }
    }

    if (!QFile::exists(cliPath)) {
        emit conflictScanError(tr("Conflict detector tool not found. Ensure ModConflictDetector.exe is in the application directory or configure its path in settings."));
        return;
    }

    m_conflictResults.clear();
    m_conflictGroups.clear();
    m_conflictModsData.clear();
    m_conflictScanErrors.clear();
    emit conflictResultsChanged();
    emit conflictGroupsChanged();
    emit conflictModsDataChanged();
    emit conflictScanErrorsChanged();

    m_isConflictScanning = true;
    m_conflictScanProgress = tr("Starting scan...");
    emit isConflictScanningChanged();
    emit conflictScanProgressChanged();

    QStringList args;
    args << "--mods-path" << m_modsPath;

    if (!m_gamePaksPath.isEmpty()) {
        args << "--game-path" << m_gamePaksPath;
    }

    if (!m_usmapFilePath.isEmpty() && QFile::exists(m_usmapFilePath)) {
        args << "--usmap" << m_usmapFilePath;
    }

    if (!m_aesKey.isEmpty()) {
        args << "--aes-key" << m_aesKey;
    }

    if (!m_ueVersion.isEmpty()) {

        QString version = convertToEnumFormat(m_ueVersion);

        if (version.startsWith("UE ")) {
            version = version.mid(3).trimmed();
        }
        args << "--ue-version" << version;
    }

    if (includeDisabled) {
        args << "--include-disabled";
    }

    args << "--verbose";

    qDebug() << "scanConflicts: Starting CLI with args:" << args;

    if (m_conflictScanProcess) {
        m_conflictScanProcess->deleteLater();
    }
    m_conflictScanProcess = new QProcess(this);

    connect(m_conflictScanProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray output = m_conflictScanProcess->readAllStandardOutput();

        m_conflictScanProcess->setProperty("jsonOutput",
            m_conflictScanProcess->property("jsonOutput").toByteArray() + output);
    });

    connect(m_conflictScanProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray errorOutput = m_conflictScanProcess->readAllStandardError();
        QString errorStr = QString::fromUtf8(errorOutput).trimmed();

        QStringList lines = errorStr.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            QString trimmedLine = line.trimmed();

            if (trimmedLine.contains("Processing:")) {

                int idx = trimmedLine.indexOf("Processing:");
                if (idx != -1) {
                    QString modName = trimmedLine.mid(idx + 11).trimmed();

                    int parenIdx = modName.indexOf('(');
                    if (parenIdx != -1) {
                        modName = modName.left(parenIdx).trimmed();
                    }
                    m_conflictScanProgress = tr("Scanning: %1").arg(modName);
                    emit conflictScanProgressChanged();
                }
            } else if (trimmedLine.contains("Found") && trimmedLine.contains("mod files")) {

                QRegularExpression rx("Found (\\d+) mod files");
                QRegularExpressionMatch match = rx.match(trimmedLine);
                if (match.hasMatch()) {
                    QString count = match.captured(1);
                    m_conflictScanProgress = tr("Found %1 mods to scan...").arg(count);
                    emit conflictScanProgressChanged();
                }
            }
        }

        qDebug() << "scanConflicts stderr:" << errorStr;
    });

    connect(m_conflictScanProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        qDebug() << "scanConflicts: Process finished with exit code" << exitCode << "status" << exitStatus;

        bool success = false;
        int conflictCount = 0;

        if (exitStatus == QProcess::NormalExit) {

            QByteArray jsonOutput = m_conflictScanProcess->property("jsonOutput").toByteArray();

            if (!jsonOutput.isEmpty()) {
                parseConflictResults(jsonOutput);
                success = m_conflictScanErrors.isEmpty() || !m_conflictResults.isEmpty();
                conflictCount = m_conflictResults.count();
            } else {
                m_conflictScanErrors.append(tr("No output received from conflict detector"));
                emit conflictScanErrorsChanged();
            }
        } else {
            m_conflictScanErrors.append(tr("Conflict detector process crashed"));
            emit conflictScanErrorsChanged();
        }

        if (m_conflictScanTimeoutTimer && m_conflictScanTimeoutTimer->isActive()) {
            m_conflictScanTimeoutTimer->stop();
        }

        m_lastConflictScanTime = QDateTime::currentDateTime().toString(Qt::ISODate);
        emit lastConflictScanTimeChanged();

        m_isConflictScanning = false;
        m_conflictScanProgress.clear();
        emit isConflictScanningChanged();
        emit conflictScanProgressChanged();

        emit conflictScanComplete(success, conflictCount);

        saveSettings();

        m_conflictScanProcess->deleteLater();
        m_conflictScanProcess = nullptr;
    });

    connect(m_conflictScanProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        QString errorMsg;
        switch (error) {
            case QProcess::FailedToStart:
                errorMsg = tr("Failed to start conflict detector. Please check if the executable exists and has proper permissions.");
                break;
            case QProcess::Crashed:
                errorMsg = tr("Conflict detector crashed unexpectedly.");
                break;
            case QProcess::Timedout:
                errorMsg = tr("Conflict detector timed out.");
                break;
            case QProcess::WriteError:
                errorMsg = tr("Error writing to conflict detector process.");
                break;
            case QProcess::ReadError:
                errorMsg = tr("Error reading from conflict detector process.");
                break;
            default:
                errorMsg = tr("Unknown error occurred with conflict detector.");
                break;
        }

        qDebug() << "scanConflicts: Process error:" << errorMsg;

        m_conflictScanErrors.append(errorMsg);
        emit conflictScanErrorsChanged();

        m_isConflictScanning = false;
        m_conflictScanProgress.clear();
        emit isConflictScanningChanged();
        emit conflictScanProgressChanged();

        emit conflictScanError(errorMsg);
    });

    m_conflictScanProcess->setProperty("jsonOutput", QByteArray());

    if (!m_conflictScanTimeoutTimer) {
        m_conflictScanTimeoutTimer = new QTimer(this);
        m_conflictScanTimeoutTimer->setSingleShot(true);
        connect(m_conflictScanTimeoutTimer, &QTimer::timeout, this, &ModManager::onConflictScanTimeout);
    }
    m_conflictScanTimeoutWarningShown = false;
    m_conflictScanTimeoutTimer->start(60000);

    m_conflictScanProcess->start(cliPath, args);
}

void ModManager::onConflictScanTimeout() {

    if (m_conflictScanTimeoutWarningShown || !m_isConflictScanning) {
        return;
    }

    m_conflictScanTimeoutWarningShown = true;
    qDebug() << "onConflictScanTimeout: Scan has been running for 60 seconds";

    emit conflictScanTimeoutWarning();
}

void ModManager::cancelConflictScan() {
    if (!m_isConflictScanning || !m_conflictScanProcess) {
        qDebug() << "cancelConflictScan: No scan in progress";
        return;
    }

    qDebug() << "cancelConflictScan: Cancelling scan...";

    if (m_conflictScanTimeoutTimer && m_conflictScanTimeoutTimer->isActive()) {
        m_conflictScanTimeoutTimer->stop();
    }

    QByteArray partialOutput = m_conflictScanProcess->property("jsonOutput").toByteArray();

    m_conflictScanProcess->terminate();

    if (!m_conflictScanProcess->waitForFinished(3000)) {

        m_conflictScanProcess->kill();
        m_conflictScanProcess->waitForFinished(1000);
    }

    if (!partialOutput.isEmpty()) {
        qDebug() << "cancelConflictScan: Attempting to parse partial output";
        parseConflictResults(partialOutput);
    }

    m_isConflictScanning = false;
    m_conflictScanProgress = tr("Scan cancelled");
    emit isConflictScanningChanged();
    emit conflictScanProgressChanged();

    m_conflictScanErrors.append(tr("Scan was cancelled by user. Partial results may be shown."));
    emit conflictScanErrorsChanged();

    emit conflictScanComplete(false, m_conflictResults.count());

    m_conflictScanProcess->deleteLater();
    m_conflictScanProcess = nullptr;
}

void ModManager::disableConflictingMod(const QString& modName) {
    qDebug() << "disableConflictingMod: Toggling mod status for" << modName;

    bool modFound = false;
    bool wasDisabled = false;
    QString actualModName = modName;

    auto checkMatch = [&](const QVariantMap& mod, const QString& search) -> bool {
        QString listName = mod["name"].toString();
        QString fileName = mod["fileName"].toString();
        QString uniqueId = mod["uniqueId"].toString();

        return listName.compare(search, Qt::CaseInsensitive) == 0 ||
               fileName.compare(search, Qt::CaseInsensitive) == 0 ||
               uniqueId.compare(search, Qt::CaseInsensitive) == 0;
    };

    for (const QVariant& modVar : m_modList) {
        QVariantMap mod = modVar.toMap();
        if (checkMatch(mod, modName)) {
            modFound = true;
            wasDisabled = mod["status"].toString() == "Disabled";
            actualModName = mod["name"].toString();
            break;
        }
    }

    if (!modFound) {
        QString modNameWithPak = modName + ".pak";
        for (const QVariant& modVar : m_modList) {
            QVariantMap mod = modVar.toMap();
            if (checkMatch(mod, modNameWithPak)) {
                modFound = true;
                wasDisabled = mod["status"].toString() == "Disabled";
                actualModName = mod["name"].toString();
                break;
            }
        }
    }

    if (!modFound && modName.endsWith("_P", Qt::CaseInsensitive)) {
        QString modNameWithoutP = modName.left(modName.length() - 2);
        for (const QVariant& modVar : m_modList) {
            QVariantMap mod = modVar.toMap();
            if (checkMatch(mod, modNameWithoutP) || checkMatch(mod, modNameWithoutP + ".pak")) {
                modFound = true;
                wasDisabled = mod["status"].toString() == "Disabled";
                actualModName = mod["name"].toString();
                break;
            }
        }
    }

    if (!modFound) {
        QString searchName = modName;
        if (searchName.endsWith("_P", Qt::CaseInsensitive)) {
            searchName = searchName.left(searchName.length() - 2);
        }

        int bestMatchLengthDiff = 99999;
        QVariantMap bestMatch;

        for (const QVariant& modVar : m_modList) {
            QVariantMap mod = modVar.toMap();
            QString modListName = mod["name"].toString();
            if (modListName.endsWith(".pak", Qt::CaseInsensitive)) {
                modListName = modListName.left(modListName.length() - 4);
            }

            if (modListName.contains(searchName, Qt::CaseInsensitive) ||
                searchName.contains(modListName, Qt::CaseInsensitive)) {

                int lengthDiff = qAbs(modListName.length() - searchName.length());
                if (lengthDiff < bestMatchLengthDiff) {
                    bestMatchLengthDiff = lengthDiff;
                    bestMatch = mod;
                }
            }
        }

        if (!bestMatch.isEmpty()) {
            modFound = true;
            wasDisabled = bestMatch["status"].toString() == "Disabled";
            actualModName = bestMatch["name"].toString();
            qDebug() << "disableConflictingMod: Found partial match:" << actualModName << "for" << modName;
        }
    }

    if (!modFound) {
        qWarning() << "disableConflictingMod: Mod not found:" << modName;
        emit conflictScanError(tr("Mod not found: %1").arg(modName));
        return;
    }

    toggleModStatus(actualModName);

    for (int i = 0; i < m_conflictGroups.size(); i++) {
        QVariantMap group = m_conflictGroups[i].toMap();
        QVariantList mods = group["mods"].toList();

        for (int j = 0; j < mods.size(); j++) {
            QVariantMap modData = mods[j].toMap();
            if (modData["name"].toString() == modName) {

                modData["isDisabled"] = !wasDisabled;
                mods[j] = modData;
            }
        }

        group["mods"] = mods;
        m_conflictGroups[i] = group;
    }

    emit conflictGroupsChanged();

    QString action = wasDisabled ? tr("enabled") : tr("disabled");
    emit installProgress(tr("Mod %1 has been %2").arg(modName, action));
}

void ModManager::exportConflictReport(const QString& filePath) {
    qDebug() << "exportConflictReport: Exporting to" << filePath;

    if (m_conflictGroups.isEmpty() && m_conflictResults.isEmpty()) {
        emit conflictScanError(tr("No conflict data to export. Please run a scan first."));
        return;
    }

    QJsonObject exportObj;

    exportObj["scanTimestamp"] = m_lastConflictScanTime.isEmpty()
        ? QDateTime::currentDateTime().toString(Qt::ISODate)
        : m_lastConflictScanTime;
    exportObj["modsPath"] = m_modsPath;
    exportObj["exportedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray modsArray;
    for (const QVariant& modVar : m_conflictModsData) {
        QVariantMap mod = modVar.toMap();
        QJsonObject modObj;
        modObj["name"] = mod["name"].toString();
        modObj["path"] = mod["path"].toString();
        modObj["type"] = mod["type"].toString();
        modObj["fileCount"] = mod["fileCount"].toInt();
        modObj["isDisabled"] = mod["isDisabled"].toBool();
        modsArray.append(modObj);
    }
    exportObj["modsScanned"] = modsArray;
    exportObj["totalModsScanned"] = modsArray.count();

    QJsonArray conflictsArray;
    for (const QVariant& conflictVar : m_conflictResults) {
        QVariantMap conflict = conflictVar.toMap();
        QJsonObject conflictObj;
        conflictObj["filePath"] = conflict["filePath"].toString();
        conflictObj["fileType"] = conflict["fileType"].toString();
        conflictObj["severity"] = conflict["severity"].toString();
        conflictObj["description"] = conflict["description"].toString();

        QJsonArray conflictingModsArray;
        QStringList conflictingMods = conflict["conflictingMods"].toStringList();
        for (const QString& modName : conflictingMods) {
            conflictingModsArray.append(modName);
        }
        conflictObj["conflictingMods"] = conflictingModsArray;

        conflictsArray.append(conflictObj);
    }
    exportObj["conflicts"] = conflictsArray;
    exportObj["totalConflicts"] = conflictsArray.count();

    QJsonArray groupsArray;
    for (const QVariant& groupVar : m_conflictGroups) {
        QVariantMap group = groupVar.toMap();
        QJsonObject groupObj;
        groupObj["groupName"] = group["groupName"].toString();
        groupObj["groupType"] = group["groupType"].toString();
        groupObj["conflictCount"] = group["conflictCount"].toInt();
        groupObj["highestSeverity"] = group["highestSeverity"].toString();
        groupObj["confidence"] = group["confidence"].toString();

        QJsonArray groupModsArray;
        QVariantList mods = group["mods"].toList();
        for (const QVariant& modVar : mods) {
            QVariantMap mod = modVar.toMap();
            QJsonObject modObj;
            modObj["name"] = mod["name"].toString();
            modObj["fileCount"] = mod["fileCount"].toInt();
            modObj["isDisabled"] = mod["isDisabled"].toBool();

            QJsonArray filesArray;
            QStringList files = mod["conflictingFiles"].toStringList();
            for (const QString& file : files) {
                filesArray.append(file);
            }
            modObj["conflictingFiles"] = filesArray;

            groupModsArray.append(modObj);
        }
        groupObj["mods"] = groupModsArray;

        if (group.contains("suggestion")) {
            QVariantMap suggestion = group["suggestion"].toMap();
            QJsonObject suggestionObj;
            suggestionObj["modToDisable"] = suggestion["modToDisable"].toString();
            suggestionObj["reason"] = suggestion["reason"].toString();
            groupObj["suggestion"] = suggestionObj;
        }

        groupsArray.append(groupObj);
    }
    exportObj["conflictGroups"] = groupsArray;

    QJsonArray errorsArray;
    for (const QString& error : m_conflictScanErrors) {
        errorsArray.append(error);
    }
    exportObj["errors"] = errorsArray;

    QJsonDocument doc(exportObj);
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        emit conflictScanError(tr("Failed to write export file: %1").arg(file.errorString()));
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "exportConflictReport: Successfully exported to" << filePath;
    emit installProgress(tr("Conflict report exported successfully"));
}

QString ModManager::copyConflictSummary() {
    qDebug() << "copyConflictSummary: Generating summary";

    if (m_conflictGroups.isEmpty() && m_conflictResults.isEmpty()) {
        emit conflictScanError(tr("No conflict data to copy. Please run a scan first."));
        return QString();
    }

    QStringList summaryLines;

    summaryLines << "=== MOD CONFLICT REPORT ===";
    summaryLines << "";

    if (!m_lastConflictScanTime.isEmpty()) {
        QDateTime scanTime = QDateTime::fromString(m_lastConflictScanTime, Qt::ISODate);
        summaryLines << QString("Scan Date: %1").arg(scanTime.toString("yyyy-MM-dd hh:mm:ss"));
    }
    summaryLines << QString("Mods Path: %1").arg(m_modsPath);
    summaryLines << QString("Total Mods Scanned: %1").arg(m_conflictModsData.count());
    summaryLines << QString("Total Conflicts Found: %1").arg(m_conflictResults.count());
    summaryLines << QString("Conflict Groups: %1").arg(m_conflictGroups.count());
    summaryLines << "";

    int highCount = 0, mediumCount = 0, lowCount = 0;
    for (const QVariant& conflictVar : m_conflictResults) {
        QVariantMap conflict = conflictVar.toMap();
        QString severity = conflict["severity"].toString().toLower();
        if (severity == "high") highCount++;
        else if (severity == "medium") mediumCount++;
        else lowCount++;
    }

    summaryLines << "--- SEVERITY BREAKDOWN ---";
    summaryLines << QString("High: %1").arg(highCount);
    summaryLines << QString("Medium: %1").arg(mediumCount);
    summaryLines << QString("Low: %1").arg(lowCount);
    summaryLines << "";

    summaryLines << "--- CONFLICT GROUPS ---";
    summaryLines << "";

    for (const QVariant& groupVar : m_conflictGroups) {
        QVariantMap group = groupVar.toMap();
        QString groupName = group["groupName"].toString();
        QString severity = group["highestSeverity"].toString().toUpper();
        int conflictCount = group["conflictCount"].toInt();

        summaryLines << QString("[%1] %2 (%3 conflicts)").arg(severity, groupName).arg(conflictCount);

        QVariantList mods = group["mods"].toList();
        for (const QVariant& modVar : mods) {
            QVariantMap mod = modVar.toMap();
            QString modName = mod["name"].toString();
            bool isDisabled = mod["isDisabled"].toBool();
            QStringList files = mod["conflictingFiles"].toStringList();

            QString status = isDisabled ? " (DISABLED)" : "";
            summaryLines << QString("  * %1%2 - %3 conflicting file(s)").arg(modName, status).arg(files.count());
        }

        if (group.contains("suggestion")) {
            QVariantMap suggestion = group["suggestion"].toMap();
            QString modToDisable = suggestion["modToDisable"].toString();
            QString reason = suggestion["reason"].toString();
            summaryLines << QString("Suggestion: Consider disabling \"%1\"").arg(modToDisable);
            if (!reason.isEmpty()) {
                summaryLines << QString("     Reason: %1").arg(reason);
            }
        }

        summaryLines << "";
    }

    summaryLines << "=== END OF REPORT ===";

    QString summary = summaryLines.join("\n");

    QGuiApplication::clipboard()->setText(summary);

    qDebug() << "copyConflictSummary: Summary copied to clipboard";
    emit installProgress(tr("Conflict summary copied to clipboard"));

    return summary;
}

void ModManager::parseConflictResults(const QByteArray& jsonData) {
    qDebug() << "parseConflictResults: Parsing JSON data of size" << jsonData.size();

    m_conflictResults.clear();
    m_conflictModsData.clear();
    m_conflictScanErrors.clear();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QString errorMsg = tr("Failed to parse conflict detector output: %1").arg(parseError.errorString());
        qDebug() << "parseConflictResults:" << errorMsg;
        m_conflictScanErrors.append(errorMsg);
        emit conflictScanErrorsChanged();
        return;
    }

    if (!doc.isObject()) {
        QString errorMsg = tr("Invalid conflict detector output format: expected JSON object");
        qDebug() << "parseConflictResults:" << errorMsg;
        m_conflictScanErrors.append(errorMsg);
        emit conflictScanErrorsChanged();
        return;
    }

    QJsonObject root = doc.object();

    QString scanTime = root.value("ScanTime").toString();
    int totalModsScanned = root.value("TotalModsScanned").toInt();
    int totalFilesScanned = root.value("TotalFilesScanned").toInt();
    int totalConflicts = root.value("TotalConflicts").toInt();

    qDebug() << "parseConflictResults: Scanned" << totalModsScanned << "mods,"
             << totalFilesScanned << "files," << totalConflicts << "conflicts";

    m_conflictModsData.clear();
    QJsonArray modsArray = root.value("Mods").toArray();
    for (const QJsonValue& modVal : modsArray) {
        QJsonObject modObj = modVal.toObject();
        QVariantMap mod;
        mod["name"] = modObj.value("Name").toString();
        mod["path"] = modObj.value("Path").toString();
        mod["type"] = modObj.value("Type").toString();
        mod["fileCount"] = modObj.value("FileCount").toInt();
        mod["isDisabled"] = modObj.value("IsDisabled").toBool();

        QStringList files;
        QJsonArray filesArray = modObj.value("Files").toArray();
        for (const QJsonValue& fileVal : filesArray) {
            files.append(fileVal.toString());
        }
        mod["files"] = files;

        m_conflictModsData.append(mod);
    }

    emit conflictModsDataChanged();

    QJsonArray conflictsArray = root.value("Conflicts").toArray();
    for (const QJsonValue& conflictVal : conflictsArray) {
        QJsonObject conflictObj = conflictVal.toObject();
        QVariantMap conflict;
        conflict["filePath"] = conflictObj.value("FilePath").toString();
        conflict["fileType"] = conflictObj.value("FileType").toString();
        conflict["severity"] = conflictObj.value("Severity").toString();
        conflict["description"] = conflictObj.value("Description").toString();

        QStringList conflictingMods;
        QJsonArray conflictingModsArray = conflictObj.value("ConflictingMods").toArray();
        for (const QJsonValue& modVal : conflictingModsArray) {
            conflictingMods.append(modVal.toString());
        }
        conflict["conflictingMods"] = conflictingMods;

        m_conflictResults.append(conflict);
    }

    QJsonArray errorsArray = root.value("Errors").toArray();
    for (const QJsonValue& errorVal : errorsArray) {
        m_conflictScanErrors.append(errorVal.toString());
    }

    qDebug() << "parseConflictResults: Parsed" << m_conflictResults.count() << "conflicts,"
             << m_conflictScanErrors.count() << "errors";

    emit conflictResultsChanged();
    emit conflictScanErrorsChanged();

    groupConflictsByCharacter();
}

void ModManager::groupConflictsByCharacter() {
    qDebug() << "groupConflictsByCharacter: Grouping" << m_conflictResults.count() << "conflicts";

    m_conflictGroups.clear();

    if (m_conflictResults.isEmpty()) {
        emit conflictGroupsChanged();
        return;
    }

    QMap<QString, QVariantList> groupedConflicts;

    QMap<QString, QString> groupTypes;

    QMap<QString, QString> groupConfidence;

    for (const QVariant& conflictVar : m_conflictResults) {
        QVariantMap conflict = conflictVar.toMap();
        QString filePath = conflict.value("filePath").toString();
        QString groupName = extractCharacterFromPath(filePath);

        QString groupType = "misc";
        QString confidence = "high";

        if (groupName.startsWith("Map: ")) {
            groupType = "map";
            confidence = "high";
        } else if (groupName == "User Interface") {
            groupType = "ui";
            confidence = "high";
        } else if (groupName == "Miscellaneous") {
            groupType = "misc";
            confidence = "low";
        } else {

            groupType = "character";
            confidence = "high";
        }

        groupedConflicts[groupName].append(conflict);
        groupTypes[groupName] = groupType;

        if (!groupConfidence.contains(groupName) || confidence == "low") {
            groupConfidence[groupName] = confidence;
        }
    }

    auto severityPriority = [](const QString& severity) -> int {
        if (severity.toLower() == "high") return 3;
        if (severity.toLower() == "medium") return 2;
        if (severity.toLower() == "low") return 1;
        return 0;
    };

    for (auto it = groupedConflicts.begin(); it != groupedConflicts.end(); ++it) {
        QString groupName = it.key();
        QVariantList conflicts = it.value();

        int conflictCount = conflicts.count();

        QString highestSeverity = "low";
        int highestPriority = 0;

        QMap<QString, QVariantMap> modsInGroup;

        for (const QVariant& conflictVar : conflicts) {
            QVariantMap conflict = conflictVar.toMap();
            QString severity = conflict.value("severity").toString();
            int priority = severityPriority(severity);
            if (priority > highestPriority) {
                highestPriority = priority;
                highestSeverity = severity;
            }

            QStringList conflictingMods = conflict.value("conflictingMods").toStringList();
            QString filePath = conflict.value("filePath").toString();
            QString fileType = conflict.value("fileType").toString();

            for (const QString& modName : conflictingMods) {
                if (!modsInGroup.contains(modName)) {
                    QVariantMap modInfo;
                    modInfo["name"] = modName;
                    modInfo["conflictingFiles"] = QStringList();
                    modInfo["fileCount"] = 0;
                    modInfo["isDisabled"] = false;

                    for (const QVariant& modDataVar : m_conflictModsData) {
                        QVariantMap modData = modDataVar.toMap();
                        if (modData.value("name").toString() == modName) {
                            modInfo["fileCount"] = modData.value("fileCount").toInt();
                            modInfo["isDisabled"] = modData.value("isDisabled").toBool();
                            break;
                        }
                    }

                    modsInGroup[modName] = modInfo;
                }

                QVariantMap modInfo = modsInGroup[modName];
                QStringList files = modInfo["conflictingFiles"].toStringList();
                if (!files.contains(filePath)) {
                    files.append(filePath);
                    modInfo["conflictingFiles"] = files;
                    modsInGroup[modName] = modInfo;
                }
            }
        }

        QVariantList modsList;
        for (auto modIt = modsInGroup.begin(); modIt != modsInGroup.end(); ++modIt) {
            modsList.append(modIt.value());
        }

        QVariantMap group;
        group["groupName"] = groupName;
        group["groupType"] = groupTypes[groupName];
        group["conflictCount"] = conflictCount;
        group["highestSeverity"] = highestSeverity;
        group["confidence"] = groupConfidence[groupName];
        group["conflicts"] = conflicts;
        group["mods"] = modsList;

        QVariantMap suggestion = generateSuggestion(modsList);
        if (!suggestion.isEmpty()) {
            group["suggestion"] = suggestion;
        }

        m_conflictGroups.append(group);
    }

    std::sort(m_conflictGroups.begin(), m_conflictGroups.end(),
        [&severityPriority](const QVariant& a, const QVariant& b) {
            QVariantMap groupA = a.toMap();
            QVariantMap groupB = b.toMap();

            int severityA = severityPriority(groupA["highestSeverity"].toString());
            int severityB = severityPriority(groupB["highestSeverity"].toString());

            if (severityA != severityB) {
                return severityA > severityB;
            }

            return groupA["conflictCount"].toInt() > groupB["conflictCount"].toInt();
        });

    qDebug() << "groupConflictsByCharacter: Created" << m_conflictGroups.count() << "groups";

    emit conflictGroupsChanged();
}

QString ModManager::extractCharacterFromPath(const QString& path) {

    QString normalizedPath = path;
    normalizedPath.replace("\\", "/");

    QRegularExpression charPattern("/Characters/([^/]+)/", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = charPattern.match(normalizedPath);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    QRegularExpression custPattern("/Customization/([^/]+)/", QRegularExpression::CaseInsensitiveOption);
    match = custPattern.match(normalizedPath);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    QRegularExpression mapPattern("/Maps/([^/]+)/", QRegularExpression::CaseInsensitiveOption);
    match = mapPattern.match(normalizedPath);
    if (match.hasMatch()) {
        return "Map: " + match.captured(1);
    }

    if (normalizedPath.contains("/UI/", Qt::CaseInsensitive)) {
        return "User Interface";
    }

    return "Miscellaneous";
}

QString ModManager::determineSeverity(const QString& fileType, int modCount, const QString& path) {

    QString baseSeverity = "medium";
    QString normalizedPath = path;
    normalizedPath.replace("\\", "/");
    QString normalizedType = fileType.toLower();

    QString fileName = normalizedPath.mid(normalizedPath.lastIndexOf('/') + 1);
    QString fileNameUpper = fileName.toUpper();

    if (fileNameUpper.startsWith("BP_") ||
        normalizedPath.contains("/Blueprints/", Qt::CaseInsensitive)) {
        baseSeverity = "high";
    }

    else if (fileNameUpper.startsWith("DT_")) {
        baseSeverity = "high";
    }

    else if (normalizedType == "map" ||
             fileName.endsWith(".umap", Qt::CaseInsensitive)) {
        baseSeverity = "high";
    }

    else if (normalizedType == "animation" ||
             normalizedType == "animmontage" ||
             normalizedType == "animsequence" ||
             normalizedPath.contains("/Animations/", Qt::CaseInsensitive) ||
             normalizedPath.contains("/AnimMontage/", Qt::CaseInsensitive) ||
             fileNameUpper.startsWith("AM_") ||
             fileNameUpper.startsWith("AS_")) {
        baseSeverity = "high";
    }

    else if (normalizedType == "physicsasset" ||
             fileNameUpper.startsWith("PA_") ||
             fileNameUpper.startsWith("PHYS_")) {
        baseSeverity = "high";
    }

    else if ((normalizedType == "skeletalmesh" || fileNameUpper.startsWith("SK_")) &&
             (normalizedPath.contains("/Characters/", Qt::CaseInsensitive) &&
              !normalizedPath.contains("/Accessories/", Qt::CaseInsensitive) &&
              !normalizedPath.contains("/Attachments/", Qt::CaseInsensitive))) {
        baseSeverity = "high";
    }

    else if (normalizedType == "material" ||
             normalizedType == "materialinstance" ||
             normalizedType == "materialinstanceconstant" ||
             fileNameUpper.startsWith("M_") ||
             fileNameUpper.startsWith("MI_")) {
        baseSeverity = "medium";
    }

    else if ((normalizedType == "texture" ||
              normalizedType == "texture2d" ||
              fileNameUpper.startsWith("T_")) &&
             (normalizedPath.contains("/Characters/", Qt::CaseInsensitive) ||
              normalizedPath.contains("/Customization/", Qt::CaseInsensitive))) {
        baseSeverity = "medium";
    }

    else if (normalizedType == "staticmesh" ||
             fileNameUpper.startsWith("SM_")) {
        baseSeverity = "medium";
    }

    else if ((normalizedType == "skeletalmesh" || fileNameUpper.startsWith("SK_")) &&
             (normalizedPath.contains("/Accessories/", Qt::CaseInsensitive) ||
              normalizedPath.contains("/Attachments/", Qt::CaseInsensitive))) {
        baseSeverity = "medium";
    }

    else if (normalizedType == "localization" ||
             fileName.endsWith(".locres", Qt::CaseInsensitive) ||
             normalizedPath.contains("/Localization/", Qt::CaseInsensitive)) {
        baseSeverity = "low";
    }

    else if (normalizedType == "config" ||
             fileName.endsWith(".ini", Qt::CaseInsensitive) ||
             normalizedPath.contains("/Config/", Qt::CaseInsensitive)) {
        baseSeverity = "low";
    }

    else if ((normalizedType == "texture" ||
              normalizedType == "texture2d" ||
              fileNameUpper.startsWith("T_")) &&
             (normalizedPath.contains("/UI/", Qt::CaseInsensitive) ||
              normalizedPath.contains("/Icons/", Qt::CaseInsensitive) ||
              normalizedPath.contains("/HUD/", Qt::CaseInsensitive))) {
        baseSeverity = "low";
    }

    else if (normalizedType == "audio" ||
             normalizedType == "soundwave" ||
             normalizedType == "soundcue" ||
             normalizedPath.contains("/Audio/", Qt::CaseInsensitive) ||
             normalizedPath.contains("/Sounds/", Qt::CaseInsensitive)) {
        baseSeverity = "low";
    }

    if (modCount >= 3) {
        if (baseSeverity == "low") {
            baseSeverity = "medium";
        } else if (baseSeverity == "medium") {
            baseSeverity = "high";
        }

    }

    if (normalizedPath.contains("/Core/", Qt::CaseInsensitive) ||
        normalizedPath.contains("/Engine/", Qt::CaseInsensitive)) {
        if (baseSeverity == "low") {
            baseSeverity = "medium";
        } else if (baseSeverity == "medium") {
            baseSeverity = "high";
        }
    }

    return baseSeverity;
}

int ModManager::countModConflictFrequency(const QString& modName) const {

    int frequency = 0;

    for (const QVariant& groupVar : m_conflictGroups) {
        QVariantMap group = groupVar.toMap();
        QVariantList mods = group.value("mods").toList();

        for (const QVariant& modVar : mods) {
            QVariantMap mod = modVar.toMap();
            if (mod.value("name").toString() == modName) {
                frequency++;
                break;
            }
        }
    }

    QSet<QString> conflictPaths;
    for (const QVariant& conflictVar : m_conflictResults) {
        QVariantMap conflict = conflictVar.toMap();
        QStringList conflictingMods = conflict.value("conflictingMods").toStringList();
        if (conflictingMods.contains(modName)) {
            conflictPaths.insert(conflict.value("filePath").toString());
        }
    }

    return conflictPaths.count();
}

QVariantMap ModManager::generateSuggestion(const QVariantList& conflictingMods) {

    if (conflictingMods.count() < 2) {
        return QVariantMap();
    }

    struct ModScore {
        QString name;
        int score;
        int fileCount;
        bool isDisabled;
        int conflictFrequency;
        int conflictingFilesCount;
    };

    QList<ModScore> scores;

    for (const QVariant& modVar : conflictingMods) {
        QVariantMap mod = modVar.toMap();
        QString modName = mod.value("name").toString();
        int fileCount = mod.value("fileCount").toInt();
        bool isDisabled = mod.value("isDisabled").toBool();
        QStringList conflictingFiles = mod.value("conflictingFiles").toStringList();
        int conflictingFilesCount = conflictingFiles.count();

        int conflictFrequency = countModConflictFrequency(modName);

        int score = 0;

        if (fileCount <= 10) {
            score += 0;
        } else if (fileCount <= 50) {
            score += 10;
        } else if (fileCount <= 100) {
            score += 20;
        } else {
            score += 30;
        }

        if (isDisabled) {
            score -= 15;
        }

        if (conflictFrequency >= 4) {
            score += 0;
        } else if (conflictFrequency >= 2) {
            score += 5;
        } else {
            score += 10;
        }

        if (conflictingFilesCount >= 4) {
            score += 0;
        } else if (conflictingFilesCount >= 2) {
            score += 3;
        } else {
            score += 5;
        }

        ModScore modScore;
        modScore.name = modName;
        modScore.score = score;
        modScore.fileCount = fileCount;
        modScore.isDisabled = isDisabled;
        modScore.conflictFrequency = conflictFrequency;
        modScore.conflictingFilesCount = conflictingFilesCount;

        scores.append(modScore);
    }

    std::sort(scores.begin(), scores.end(), [](const ModScore& a, const ModScore& b) {
        return a.score < b.score;
    });

    if (scores.isEmpty()) {
        return QVariantMap();
    }

    const ModScore& suggested = scores.first();

    if (scores.count() >= 2) {
        int scoreDiff = scores.last().score - scores.first().score;
        if (scoreDiff < 5) {

            return QVariantMap();
        }
    }

    QString reason;

    if (suggested.isDisabled) {
        reason = tr("This mod is already disabled");
    } else if (suggested.fileCount <= 10) {
        reason = tr("This mod has fewer files (%1) and may be an addon or patch").arg(suggested.fileCount);
    } else if (suggested.conflictFrequency >= 3) {
        reason = tr("This mod appears in multiple conflict groups (%1) - disabling it may resolve several conflicts").arg(suggested.conflictFrequency);
    } else if (suggested.conflictingFilesCount >= 3) {
        reason = tr("This mod has many conflicting files in this group (%1)").arg(suggested.conflictingFilesCount);
    } else {
        reason = tr("This mod has fewer files and may be an addon");
    }

    QVariantMap suggestion;
    suggestion["modToDisable"] = suggested.name;
    suggestion["reason"] = reason;
    suggestion["score"] = suggested.score;

    qDebug() << "generateSuggestion: Suggesting to disable" << suggested.name
             << "with score" << suggested.score << "-" << reason;

    return suggestion;
}
