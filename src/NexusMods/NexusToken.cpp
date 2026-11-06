#include "NexusToken.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
#endif

namespace {

#ifdef Q_OS_WIN
constexpr wchar_t kCredentialTarget[] = L"UniversalManager/NexusModsOAuth";
constexpr wchar_t kCredentialUserName[] = L"NexusMods";

constexpr char kLegacyEntropy[] = "UniversalManager/NexusOAuth/DPAPI/v1";
constexpr char kLegacyFileName[] = "nexus_oauth_session.bin";

QString legacyFilePath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QCoreApplication::applicationDirPath();
    }
    return QDir(directory).filePath(QString::fromLatin1(kLegacyFileName));
}

QString readCredential()
{
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(kCredentialTarget, CRED_TYPE_GENERIC, 0, &credential) || !credential) {
        return {};
    }

    const QString token = QString::fromUtf8(
        reinterpret_cast<const char*>(credential->CredentialBlob),
        static_cast<qsizetype>(credential->CredentialBlobSize));
    SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
    CredFree(credential);
    return token;
}
#endif

}

bool NexusToken::saveRefreshToken(const QString& refreshToken)
{
#ifdef Q_OS_WIN
    if (refreshToken.isEmpty()) {
        return false;
    }

    QByteArray token = refreshToken.toUtf8();
    if (token.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        return false;
    }

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(kCredentialTarget);
    credential.UserName = const_cast<LPWSTR>(kCredentialUserName);
    credential.Comment = const_cast<LPWSTR>(L"Nexus Mods OAuth refresh token");
    credential.CredentialBlobSize = static_cast<DWORD>(token.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(token.data());
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;

    const bool saved = CredWriteW(&credential, 0) == TRUE;
    SecureZeroMemory(token.data(), static_cast<size_t>(token.size()));
    return saved;
#else
    Q_UNUSED(refreshToken);
    return false;
#endif
}

QString NexusToken::loadRefreshToken()
{
#ifdef Q_OS_WIN
    const QString stored = readCredential();
    if (!stored.isEmpty()) {
        QFile::remove(legacyFilePath());
        return stored;
    }

    const QString legacy = migrateLegacySession();
    if (legacy.isEmpty()) {
        return {};
    }
    if (saveRefreshToken(legacy)) {
        QFile::remove(legacyFilePath());
    }
    return legacy;
#else
    return {};
#endif
}

void NexusToken::clear()
{
#ifdef Q_OS_WIN
    CredDeleteW(kCredentialTarget, CRED_TYPE_GENERIC, 0);
    QFile::remove(legacyFilePath());
#endif
}

#ifdef Q_OS_WIN
QString NexusToken::migrateLegacySession()
{
    QFile file(legacyFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray cipher = file.readAll();
    file.close();

    DATA_BLOB cipherBlob;
    cipherBlob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(cipher.constData()));
    cipherBlob.cbData = static_cast<DWORD>(cipher.size());

    QByteArray entropyBytes(kLegacyEntropy);
    DATA_BLOB entropyBlob;
    entropyBlob.pbData = reinterpret_cast<BYTE*>(entropyBytes.data());
    entropyBlob.cbData = static_cast<DWORD>(entropyBytes.size());

    DATA_BLOB plainBlob{};
    if (!CryptUnprotectData(&cipherBlob, nullptr, &entropyBlob, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &plainBlob)) {
        return {};
    }

    QByteArray plain(reinterpret_cast<const char*>(plainBlob.pbData),
                     static_cast<qsizetype>(plainBlob.cbData));
    SecureZeroMemory(plainBlob.pbData, plainBlob.cbData);
    LocalFree(plainBlob.pbData);

    const QString token = QJsonDocument::fromJson(plain)
                              .object()
                              .value(QStringLiteral("refresh_token"))
                              .toString();
    SecureZeroMemory(plain.data(), static_cast<size_t>(plain.size()));
    return token;
}
#endif
