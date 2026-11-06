#include "ModManagerInternal.h"

using namespace mmdetail;

void ModManager::parseCustomGameVersionsFromEGameCs() {
    m_customGameVersions.clear();
    m_customGameEnumNames.clear();
    m_customGameBaseVersions.clear();
    m_customGameEnumToDisplay.clear();
    m_customGameDetectedCache.clear();

    const QString projectRoot = findProjectRoot();
    const QStringList candidatePaths = {
        projectRoot + "/Tools/PakViewer/Libs/EGame.cs",
        projectRoot + "/Tools/PakViewer/Libs/CUE4Parse/CUE4Parse/UE4/Versions/EGame.cs"
    };
    QString eGamePath;
    for (const QString& candidate : candidatePaths) {
        if (QFile::exists(candidate)) {
            eGamePath = candidate;
            break;
        }
    }

    QFile file(eGamePath);
    if (eGamePath.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open EGame.cs for parsing custom game versions. Tried:" << candidatePaths;
        return;
    }

    QTextStream in(&file);

    QRegularExpression gamePattern(R"(^\s*GAME_([A-Za-z0-9_]+)\s*=\s*GAME_UE([456])_(\d+)\s*\+\s*\d+)");

    while (!in.atEnd()) {
        const QString line = in.readLine();
        QRegularExpressionMatch match = gamePattern.match(line);

        if (match.hasMatch()) {
            QString gameName = match.captured(1);
            const QString majorVer = match.captured(2);
            const QString minorVer = match.captured(3);
            QString enumName = "GAME_" + gameName;

            QString readableName;
            for (int i = 0; i < gameName.length(); ++i) {
                const QChar ch = gameName[i];

                if (i > 0 && ch.isUpper() && !gameName[i-1].isUpper()) {
                    readableName += ' ';
                }
                readableName += ch;
            }

            readableName = readableName.trimmed();
            const QString baseUeVersion = QStringLiteral("UE %1.%2").arg(majorVer, minorVer);

            m_customGameVersions.append(readableName);
            m_customGameEnumNames.insert(readableName, enumName);
            m_customGameEnumToDisplay.insert(enumName, readableName);
            m_customGameBaseVersions.insert(enumName, baseUeVersion);
        }
    }

    file.close();

    qDebug() << "[CustomGames] Parsed" << m_customGameVersions.size() << "custom game versions from CUE4Parse";

    emit customGameVersionsChanged();
}

static QString normalizeCustomGameName(const QString& input) {
    QString result;
    result.reserve(input.size());
    for (QChar ch : input) {
        if (ch.isLetterOrNumber()) {
            result.append(ch.toLower());
        }
    }
    return result;
}

QString ModManager::detectCustomGameForPath(const QString& gamePath) const {
    if (gamePath.isEmpty() || m_customGameVersions.isEmpty()) {
        return QString();
    }

    const QString cleanPath = QDir::cleanPath(QUrl::fromPercentEncoding(gamePath.toUtf8()));
    if (cleanPath.isEmpty()) {
        return QString();
    }

    if (m_customGameDetectedCache.contains(cleanPath)) {
        return m_customGameDetectedCache.value(cleanPath);
    }

    QFileInfo fi(cleanPath);
    if (!fi.exists()) {
        m_customGameDetectedCache.insert(cleanPath, QString());
        return QString();
    }

    QString folderName;
    QString exeBaseName;
    QStringList folderCandidates;
    QStringList exeCandidates;

    auto addFolderCandidateFromInfo = [&folderCandidates](const QFileInfo& info) {
        if (info.exists()) {
            folderCandidates << normalizeCustomGameName(info.fileName());
        }
    };

    auto collectFolderCandidates = [&](const QString& rootPath) {
        QFileInfo info(rootPath);
        if (!info.exists() || !info.isDir()) return;

        addFolderCandidateFromInfo(info);

        QDir cursor(info.absoluteFilePath());
        for (int level = 0; level < 3; ++level) {
            if (!cursor.cdUp()) break;
            const QString upPath = cursor.absolutePath();
            if (upPath.isEmpty()) break;
            QFileInfo upInfo(upPath);
            if (!upInfo.exists() || !upInfo.isDir()) break;
            addFolderCandidateFromInfo(upInfo);
        }
    };

    if (fi.isDir()) {
        folderName = fi.fileName();
        collectFolderCandidates(cleanPath);

        const QString bestExe = findGameExecutableFromFolder(cleanPath, folderName);
        if (!bestExe.isEmpty()) {
            QFileInfo exeInfo(bestExe);
            QString stem = exeInfo.completeBaseName();

            const int dashIdx = stem.lastIndexOf('-');
            if (dashIdx > 0) {
                exeCandidates << normalizeCustomGameName(stem.left(dashIdx));
            }
            exeCandidates << normalizeCustomGameName(stem);
            exeCandidates << normalizeCustomGameName(exeInfo.fileName());
        }
    } else {

        exeBaseName = fi.completeBaseName();
        const int dashIdx = exeBaseName.lastIndexOf('-');
        if (dashIdx > 0) {
            exeCandidates << normalizeCustomGameName(exeBaseName.left(dashIdx));
        }
        exeCandidates << normalizeCustomGameName(exeBaseName);
        exeCandidates << normalizeCustomGameName(fi.fileName());

        const QString containerDir = QFileInfo(cleanPath).absolutePath();
        if (!containerDir.isEmpty()) {
            QDir cursor(containerDir);
            for (int level = 0; level < 3; ++level) {
                QFileInfo curInfo(cursor.absolutePath());
                if (!curInfo.exists() || !curInfo.isDir()) break;
                folderCandidates << normalizeCustomGameName(curInfo.fileName());
                if (!cursor.cdUp()) break;
            }
        }
    }

    if (folderCandidates.isEmpty() && exeCandidates.isEmpty()) {
        return QString();
    }

    for (const auto& it : m_customGameEnumToDisplay.toStdMap()) {
        const QString& enumName = it.first;
        QString normalizedEnum = normalizeCustomGameName(m_customGameEnumNames.key(enumName, QString()));
        if (normalizedEnum.isEmpty()) {
            normalizedEnum = normalizeCustomGameName(enumName.mid(5));
        }
        for (const QString& candidate : folderCandidates) {
            if (!candidate.isEmpty() && candidate == normalizedEnum) {
                m_customGameDetectedCache.insert(cleanPath, enumName);
                const QString displayName = m_customGameEnumToDisplay.value(enumName);
                const QString baseVersion = m_customGameBaseVersions.value(enumName);
                qDebug().noquote()
                    << QString::fromUtf8("[CustomGameDetect] path=\"%1\" -> %2 [%3, base %4] (strict: folder)")
                           .arg(cleanPath,
                                displayName.isEmpty() ? enumName : displayName,
                                enumName,
                                baseVersion.isEmpty() ? QStringLiteral("?") : baseVersion);
                return enumName;
            }
        }
        for (const QString& candidate : exeCandidates) {
            if (!candidate.isEmpty() && candidate == normalizedEnum) {
                m_customGameDetectedCache.insert(cleanPath, enumName);
                const QString displayName = m_customGameEnumToDisplay.value(enumName);
                const QString baseVersion = m_customGameBaseVersions.value(enumName);
                qDebug().noquote()
                    << QString::fromUtf8("[CustomGameDetect] path=\"%1\" -> %2 [%3, base %4] (strict: exe)")
                           .arg(cleanPath,
                                displayName.isEmpty() ? enumName : displayName,
                                enumName,
                                baseVersion.isEmpty() ? QStringLiteral("?") : baseVersion);
                return enumName;
            }
        }
    }

    QList<QPair<QString, QString>> fuzzyCandidates;
    for (const auto& it : m_customGameEnumToDisplay.toStdMap()) {
        const QString& enumName = it.first;
        QString normalizedEnum = normalizeCustomGameName(m_customGameEnumNames.key(enumName, QString()));
        if (normalizedEnum.isEmpty()) {
            normalizedEnum = normalizeCustomGameName(enumName.mid(5));
        }
        if (!normalizedEnum.isEmpty()) {
            fuzzyCandidates.append({enumName, normalizedEnum});
        }
    }
    std::sort(fuzzyCandidates.begin(), fuzzyCandidates.end(),
              [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                  return a.second.size() > b.second.size();
              });

    int bestScore = 0;
    QString bestEnum;
    for (const auto& pair : fuzzyCandidates) {
        const QString& normalizedEnum = pair.second;
        if (normalizedEnum.size() < 4) {
            continue;
        }
        for (const QString& candidate : folderCandidates) {
            if (candidate.size() < 4) continue;
            if (candidate.contains(normalizedEnum) || normalizedEnum.contains(candidate)) {
                const int score = qMin(candidate.size(), normalizedEnum.size());
                if (score > bestScore) {
                    bestScore = score;
                    bestEnum = pair.first;
                }
            }
        }
        for (const QString& candidate : exeCandidates) {
            if (candidate.size() < 4) continue;
            if (candidate.contains(normalizedEnum) || normalizedEnum.contains(candidate)) {
                const int score = qMin(candidate.size(), normalizedEnum.size());
                if (score > bestScore) {
                    bestScore = score;
                    bestEnum = pair.first;
                }
            }
        }
    }

    if (!bestEnum.isEmpty()) {
        m_customGameDetectedCache.insert(cleanPath, bestEnum);
        const QString displayName = m_customGameEnumToDisplay.value(bestEnum);
        const QString baseVersion = m_customGameBaseVersions.value(bestEnum);
        qDebug().noquote()
            << QString::fromUtf8("[CustomGameDetect] path=\"%1\" -> %2 [%3, base %4] (fuzzy)")
                   .arg(cleanPath,
                        displayName.isEmpty() ? bestEnum : displayName,
                        bestEnum,
                        baseVersion.isEmpty() ? QStringLiteral("?") : baseVersion);
        return bestEnum;
    }

    m_customGameDetectedCache.insert(cleanPath, QString());
    return QString();
}

QString ModManager::getCustomGameForGame(const QString& gamePath) const {
    return detectCustomGameForPath(gamePath);
}

QString ModManager::getCustomGameDisplayNameForGame(const QString& gamePath) const {
    const QString enumName = detectCustomGameForPath(gamePath);
    if (enumName.isEmpty()) {
        return QString();
    }
    return m_customGameEnumToDisplay.value(enumName);
}

QString ModManager::getCustomGameBaseVersionForGame(const QString& gamePath) const {
    const QString enumName = detectCustomGameForPath(gamePath);
    if (enumName.isEmpty()) {
        return QString();
    }
    return m_customGameBaseVersions.value(enumName);
}

bool ModManager::isCustomGameDetectedForGame(const QString& gamePath) const {
    return !detectCustomGameForPath(gamePath).isEmpty();
}

QString ModManager::getCustomGameSummaryForGame(const QString& gamePath) const {
    const QString enumName = detectCustomGameForPath(gamePath);
    if (enumName.isEmpty()) {
        return QString();
    }
    const QString displayName = m_customGameEnumToDisplay.value(enumName);
    const QString baseVersion = m_customGameBaseVersions.value(enumName);
    return QStringLiteral("%1 [%2, base %3]")
        .arg(displayName.isEmpty() ? enumName : displayName,
             enumName,
             baseVersion.isEmpty() ? QStringLiteral("?") : baseVersion);
}

QString ModManager::convertToEnumFormat(const QString& versionOrGameName) const {

    if (m_customGameEnumNames.contains(versionOrGameName)) {
        return m_customGameEnumNames.value(versionOrGameName);
    }

    return versionOrGameName;
}
