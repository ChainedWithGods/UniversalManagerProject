#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

class AnimatedImageStore : public QObject
{
    Q_OBJECT

public:
    explicit AnimatedImageStore(QObject* parent = nullptr);
    ~AnimatedImageStore() override;

    Q_INVOKABLE QString cachedPath(const QString& url) const;

    Q_INVOKABLE QVariantMap infoFor(const QString& url) const;
    Q_INVOKABLE void request(const QString& url);

signals:
    void converted(const QString& url, const QString& localUrl);

private:
    QString pathForUrl(const QString& url) const;
    bool decodeToPng(const QByteArray& data, const QString& outPath) const;

    QNetworkAccessManager m_network;
    QSet<QString> m_inFlight;
    bool m_comInitialized = false;
};
