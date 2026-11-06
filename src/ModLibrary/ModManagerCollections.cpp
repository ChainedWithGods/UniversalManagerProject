#include "ModManagerInternal.h"

using namespace mmdetail;

QString ModManager::getCollectionsFilePath() const {
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        return QCoreApplication::applicationDirPath() + "/collections.json";
    }
    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return appDataDir + "/collections.json";
}

QVariantList ModManager::collections() const {
    if (m_activePresetId.isEmpty()) {
        return m_collections;
    }

    QVariantList filteredCollections;
    const QString activeFolderKey = normalizePathForCompare(m_modsPath);
    for (const QVariant& var : m_collections) {
        const QVariantMap collection = var.toMap();
        if (collection.value("presetId").toString() != m_activePresetId) {
            continue;
        }

        const QString collectionFolderKey = normalizePathForCompare(collection.value("modsPath").toString());
        if (!activeFolderKey.isEmpty() && !collectionFolderKey.isEmpty() && collectionFolderKey != activeFolderKey) {
            continue;
        }

        filteredCollections.append(collection);
    }

    return filteredCollections;
}

void ModManager::loadCollections() {
    QString filePath = getCollectionsFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        m_collections.clear();
        emit collectionsChanged();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open collections file:" << filePath;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "Collections file is not a valid JSON array";
        return;
    }

    QHash<QString, QSet<QString>> presetModNamesCache;
    auto getPresetModNames = [this, &presetModNamesCache](const QString& presetId) -> QSet<QString> {
        if (presetModNamesCache.contains(presetId)) {
            return presetModNamesCache.value(presetId);
        }

        const int presetIndex = findPresetIndexById(presetId);
        if (presetIndex < 0) {
            return {};
        }

        const QString modsPath = m_gamePresets[presetIndex].toMap().value("modsPath").toString();
        const QSet<QString> modNames = collectDisplayModNamesInPath(modsPath);
        presetModNamesCache.insert(presetId, modNames);
        return modNames;
    };

    auto inferPresetIdForCollection = [this, &getPresetModNames](const QStringList& mods) -> QString {
        if (m_gamePresets.size() == 1) {
            return m_gamePresets.first().toMap().value("id").toString();
        }

        QString bestPresetId;
        int bestScore = 0;
        bool scoreTie = false;

        for (const QVariant& presetVar : m_gamePresets) {
            const QVariantMap preset = presetVar.toMap();
            const QString presetId = preset.value("id").toString();
            const QSet<QString> presetModNames = getPresetModNames(presetId);

            int score = 0;
            for (const QString& modName : mods) {
                if (presetModNames.contains(modName.trimmed().toLower())) {
                    score++;
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestPresetId = presetId;
                scoreTie = false;
            } else if (score > 0 && score == bestScore) {
                scoreTie = true;
            }
        }

        if (bestScore > 0 && !scoreTie) {
            return bestPresetId;
        }

        return m_activePresetId;
    };

    QHash<QString, QSet<QString>> modsByFolderCache;
    auto getFolderModNames = [&modsByFolderCache](const QString& folderPath) -> QSet<QString> {
        const QString key = normalizePathForCompare(folderPath);
        if (key.isEmpty()) {
            return {};
        }
        if (modsByFolderCache.contains(key)) {
            return modsByFolderCache.value(key);
        }

        const QSet<QString> modNames = collectDisplayModNamesInPath(folderPath);
        modsByFolderCache.insert(key, modNames);
        return modNames;
    };

    auto inferModsFolderPathForCollection = [this, &getFolderModNames](const QString& presetId, const QStringList& mods) -> QString {
        const int presetIndex = findPresetIndexById(presetId);
        if (presetIndex < 0) {
            return QString();
        }

        const QVariantMap preset = m_gamePresets[presetIndex].toMap();
        const QString presetModsPath = QDir::cleanPath(preset.value("modsPath").toString());

        QVariantMap game;
        game["path"] = preset.value("gamePath").toString();
        game["paksPath"] = preset.value("gamePaksPath").toString();
        game["modsFolderPath"] = presetModsPath;

        QVariantList folderChoices = getModsFoldersForGame(game);
        const QString normalizedPresetModsPath = normalizePathForCompare(presetModsPath);
        bool foundPresetPath = normalizedPresetModsPath.isEmpty();
        for (const QVariant& folderVar : folderChoices) {
            if (normalizePathForCompare(folderVar.toMap().value("path").toString()) == normalizedPresetModsPath) {
                foundPresetPath = true;
                break;
            }
        }
        if (!foundPresetPath && !presetModsPath.isEmpty()) {
            QVariantMap folder;
            folder["path"] = presetModsPath;
            folderChoices.append(folder);
        }

        QString bestFolderPath;
        int bestScore = 0;
        bool scoreTie = false;

        for (const QVariant& folderVar : folderChoices) {
            const QString folderPath = QDir::cleanPath(folderVar.toMap().value("path").toString());
            if (folderPath.isEmpty()) {
                continue;
            }

            const QSet<QString> folderModNames = getFolderModNames(folderPath);
            int score = 0;
            for (const QString& modName : mods) {
                if (folderModNames.contains(modName.trimmed().toLower())) {
                    score++;
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestFolderPath = folderPath;
                scoreTie = false;
            } else if (score > 0 && score == bestScore) {
                scoreTie = true;
            }
        }

        if (bestScore > 0 && !scoreTie) {
            return bestFolderPath;
        }

        return presetModsPath;
    };

    bool migratedLegacyCollections = false;
    m_collections.clear();
    QJsonArray arr = doc.array();
    for (const QJsonValue& val : arr) {
        if (val.isObject()) {
            QJsonObject obj = val.toObject();
            QVariantMap collection;
            collection["id"] = obj["id"].toString();
            collection["name"] = obj["name"].toString();
            collection["description"] = obj["description"].toString();
            collection["preview"] = obj["preview"].toString();
            collection["modCount"] = obj["mods"].toArray().size();

            QStringList mods;
            for (const QJsonValue& modVal : obj["mods"].toArray()) {
                mods.append(modVal.toString());
            }
            collection["mods"] = mods;
            QString presetId = obj["presetId"].toString();
            if (presetId.isEmpty()) {
                presetId = inferPresetIdForCollection(mods);
                if (!presetId.isEmpty()) {
                    migratedLegacyCollections = true;
                }
            }
            collection["presetId"] = presetId;
            QString modsPath = QDir::cleanPath(obj["modsPath"].toString());
            if (modsPath.isEmpty() && !presetId.isEmpty()) {
                modsPath = inferModsFolderPathForCollection(presetId, mods);
                if (!modsPath.isEmpty()) {
                    migratedLegacyCollections = true;
                }
            }
            collection["modsPath"] = modsPath;

            m_collections.append(collection);
        }
    }

    if (migratedLegacyCollections) {
        saveCollections();
    }

    emit collectionsChanged();
}

void ModManager::saveCollections() {
    QString filePath = getCollectionsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save collections file:" << filePath;
        return;
    }

    QJsonArray arr;
    for (const QVariant& var : m_collections) {
        QVariantMap collection = var.toMap();
        QJsonObject obj;
        obj["id"] = collection["id"].toString();
        obj["name"] = collection["name"].toString();
        obj["description"] = collection["description"].toString();
        obj["preview"] = collection["preview"].toString();
        obj["presetId"] = collection["presetId"].toString();
        obj["modsPath"] = collection["modsPath"].toString();

        QJsonArray modsArr;
        QStringList mods = collection["mods"].toStringList();
        for (const QString& mod : mods) {
            modsArr.append(mod);
        }
        obj["mods"] = modsArr;

        arr.append(obj);
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void ModManager::createCollection(const QString& name, const QString& description) {
    if (name.trimmed().isEmpty()) {
        return;
    }

    QVariantMap collection;
    collection["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    collection["name"] = name.trimmed();
    collection["description"] = description.trimmed();
    collection["modCount"] = 0;
    collection["mods"] = QStringList();
    collection["presetId"] = m_activePresetId;
    collection["modsPath"] = QDir::cleanPath(m_modsPath);

    m_collections.append(collection);

    (void)QtConcurrent::run([this]() {
        saveCollections();
        QMetaObject::invokeMethod(this, [this]() {
            emit collectionsChanged();
        }, Qt::QueuedConnection);
    });
}

void ModManager::deleteCollection(const QString& collectionId) {
    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection["id"].toString() == collectionId) {
            m_collections.removeAt(i);

            (void)QtConcurrent::run([this]() {
                saveCollections();
                QMetaObject::invokeMethod(this, [this]() {
                    emit collectionsChanged();
                }, Qt::QueuedConnection);
            });

            return;
        }
    }
}

void ModManager::addModToCollection(const QString& collectionId, const QString& modName) {
    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection["id"].toString() == collectionId) {
            QStringList mods = collection["mods"].toStringList();
            if (!mods.contains(modName)) {
                mods.append(modName);
                collection["mods"] = mods;
                collection["modCount"] = mods.size();
                m_collections[i] = collection;

                (void)QtConcurrent::run([this]() {
                    saveCollections();
                    QMetaObject::invokeMethod(this, [this]() {
                        emit collectionsChanged();
                    }, Qt::QueuedConnection);
                });
            }
            return;
        }
    }
}

void ModManager::removeModFromCollection(const QString& collectionId, const QString& modName) {
    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection["id"].toString() == collectionId) {
            QStringList mods = collection["mods"].toStringList();
            mods.removeAll(modName);
            collection["mods"] = mods;
            collection["modCount"] = mods.size();
            m_collections[i] = collection;

            (void)QtConcurrent::run([this]() {
                saveCollections();
                QMetaObject::invokeMethod(this, [this]() {
                    emit collectionsChanged();
                }, Qt::QueuedConnection);
            });

            return;
        }
    }
}

void ModManager::viewCollection(const QString& collectionId) {

    for (const QVariant& var : m_collections) {
        QVariantMap collection = var.toMap();
        if (collection["id"].toString() == collectionId) {

            qDebug() << "Viewing collection:" << collection["name"].toString();

            return;
        }
    }
}

void ModManager::renameCollection(const QString& collectionId, const QString& newName) {
    if (newName.trimmed().isEmpty()) {
        return;
    }

    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection["id"].toString() == collectionId) {
            collection["name"] = newName.trimmed();
            m_collections[i] = collection;

            (void)QtConcurrent::run([this]() {
                saveCollections();
                QMetaObject::invokeMethod(this, [this]() {
                    emit collectionsChanged();
                }, Qt::QueuedConnection);
            });

            return;
        }
    }
}

void ModManager::updateCollectionDescription(const QString& collectionId, const QString& newDescription) {
    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection["id"].toString() == collectionId) {
            collection["description"] = newDescription.trimmed();
            m_collections[i] = collection;

            (void)QtConcurrent::run([this]() {
                saveCollections();
                QMetaObject::invokeMethod(this, [this]() {
                    emit collectionsChanged();
                }, Qt::QueuedConnection);
            });

            return;
        }
    }
}

void ModManager::setCollectionPreview(const QString& collectionId, const QString& imagePath) {

    (void)QtConcurrent::run([this, collectionId, imagePath]() {
        QString destPath = imagePath;

        if (!imagePath.isEmpty()) {
            QFileInfo fileInfo(imagePath);
            QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/collection_previews";
            QDir().mkpath(dataDir);

            QDir dir(dataDir);
            const QStringList oldFiles = dir.entryList(QStringList() << collectionId + "_*", QDir::Files);
            for (const QString& oldFile : oldFiles) {
                QFile::remove(dataDir + "/" + oldFile);
            }

            QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
            destPath = dataDir + "/" + collectionId + "_" + timestamp + "." + fileInfo.suffix();

            QFile::copy(imagePath, destPath);
        }

        QMetaObject::invokeMethod(this, [this, collectionId, destPath]() {
            for (int i = 0; i < m_collections.size(); ++i) {
                QVariantMap collection = m_collections[i].toMap();
                if (collection["id"].toString() == collectionId) {
                    collection["preview"] = destPath;
                    m_collections[i] = collection;

                    (void)QtConcurrent::run([this]() {
                        saveCollections();

                        QMetaObject::invokeMethod(this, [this]() {
                            emit collectionsChanged();
                        }, Qt::QueuedConnection);
                    });

                    return;
                }
            }
        }, Qt::QueuedConnection);
    });
}

QString ModManager::getCollectionPreview(const QString& collectionId) {
    for (const QVariant& var : m_collections) {
        QVariantMap collection = var.toMap();
        if (collection["id"].toString() == collectionId) {
            return collection["preview"].toString();
        }
    }
    return QString();
}

void ModManager::toggleCollectionStatus(const QString& collectionId, bool enable) {
    for (const QVariant& var : m_collections) {
        QVariantMap collection = var.toMap();
        if (collection["id"].toString() == collectionId) {
            QStringList mods = collection["mods"].toStringList();
            if (mods.isEmpty()) return;

            toggleModStatusBatch(mods, enable);
            return;
        }
    }
}

void ModManager::cleanupDeletedModsFromCollections(const QStringList& deletedModNames) {
    if (deletedModNames.isEmpty()) {
        return;
    }

    bool collectionsModified = false;

    for (int i = 0; i < m_collections.size(); ++i) {
        QVariantMap collection = m_collections[i].toMap();
        if (collection.value("presetId").toString() != m_activePresetId) {
            continue;
        }
        const QString activeFolderKey = normalizePathForCompare(m_modsPath);
        const QString collectionFolderKey = normalizePathForCompare(collection.value("modsPath").toString());
        if (!activeFolderKey.isEmpty() && !collectionFolderKey.isEmpty() && collectionFolderKey != activeFolderKey) {
            continue;
        }
        QStringList mods = collection["mods"].toStringList();
        int originalSize = mods.size();

        for (const QString& deletedMod : deletedModNames) {
            mods.removeAll(deletedMod);
        }

        if (mods.size() != originalSize) {
            collection["mods"] = mods;
            collection["modCount"] = mods.size();
            m_collections[i] = collection;
            collectionsModified = true;
            qDebug() << "Removed" << (originalSize - mods.size()) << "deleted mod(s) from collection:" << collection["name"].toString();
        }
    }

    if (collectionsModified) {

        (void)QtConcurrent::run([this]() {
            saveCollections();
            QMetaObject::invokeMethod(this, [this]() {
                emit collectionsChanged();
            }, Qt::QueuedConnection);
        });
    }
}
