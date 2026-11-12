#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ModManager;

class LuaProfileManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString activeProfileId READ activeProfileId WRITE setActiveProfileId NOTIFY activeProfileIdChanged)
    Q_PROPERTY(QStringList profileIds READ profileIds NOTIFY profilesChanged)

public:
    explicit LuaProfileManager(ModManager* modManager, QObject* parent = nullptr);

    QString activeProfileId() const;
    void setActiveProfileId(const QString& profileId);

    QStringList profileIds() const;

    Q_INVOKABLE QString createProfileFolder(const QString& presetId, const QString& presetName);

    Q_INVOKABLE QString deleteProfileFolder(const QString& presetId);

    Q_INVOKABLE QString renameProfileFolder(const QString& presetId, const QString& oldPresetName);

    Q_INVOKABLE QString profileFolderPath(const QString& presetId, const QString& explicitName = QString()) const;

    Q_INVOKABLE QString luaModsFolderForProfile(const QString& presetId) const;

    Q_INVOKABLE QString cppModsFolderForProfile(const QString& presetId) const;

    Q_INVOKABLE bool profileFolderExists(const QString& presetId) const;

    static bool seedProfileFolderAt(const QString& folderPath, const QString& defaultModsDir,
                                    QString* errorOut);

    void notifyProfileFoldersChanged() { emit profileFoldersChanged(); }

signals:
    void activeProfileIdChanged();
    void profilesChanged();

    void profileFoldersChanged();

private:
    friend class ModManager;

    QString folderNameForPresetId(const QString& presetId, const QString& explicitName = QString()) const;

    QString folderPathForName(const QString& presetId, const QString& displayName) const;
    QString presetsFolderRoot() const;
    bool seedProfileFolder(const QString& folderPath, QString* errorOut);

    ModManager* m_modManager = nullptr;
    QString m_activeProfileId;
};
