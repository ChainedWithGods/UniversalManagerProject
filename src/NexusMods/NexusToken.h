#pragma once

#include <QString>

class NexusToken
{
public:
    bool saveRefreshToken(const QString& refreshToken);
    QString loadRefreshToken();
    void clear();

private:
#ifdef Q_OS_WIN
    QString migrateLegacySession();
#endif
};
