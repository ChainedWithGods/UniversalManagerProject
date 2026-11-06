#include "AnimatedImageStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objidl.h>
#include <combaseapi.h>
#include <shlwapi.h>
#include <wincodec.h>
#endif

AnimatedImageStore::AnimatedImageStore(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    m_comInitialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
#endif
}

AnimatedImageStore::~AnimatedImageStore()
{
#ifdef Q_OS_WIN
    if (m_comInitialized)
        CoUninitialize();
#endif
}

QString AnimatedImageStore::pathForUrl(const QString& url) const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/avif");
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".png");
}

QString AnimatedImageStore::cachedPath(const QString& url) const
{
    const QString path = pathForUrl(url);
    return QFileInfo::exists(path) ? QUrl::fromLocalFile(path).toString() : QString();
}

QVariantMap AnimatedImageStore::infoFor(const QString& url) const
{
    QVariantMap info;
    const QString path = pathForUrl(url);
    if (!QFileInfo::exists(path))
        return info;

    QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid())
        return info;

    info.insert(QStringLiteral("width"), size.width());
    info.insert(QStringLiteral("height"), size.height());
    info.insert(QStringLiteral("png"), QUrl::fromLocalFile(path).toString());
    return info;
}

void AnimatedImageStore::request(const QString& url)
{
    if (url.isEmpty() || m_inFlight.contains(url) || !cachedPath(url).isEmpty())
        return;
    m_inFlight.insert(url);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) UniversalModManager/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        m_inFlight.remove(url);
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QByteArray data = reply->readAll();
        const QString path = pathForUrl(url);
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (decodeToPng(data, path))
            emit converted(url, QUrl::fromLocalFile(path).toString());
    });
}

bool AnimatedImageStore::decodeToPng(const QByteArray& data, const QString& outPath) const
{
#ifdef Q_OS_WIN
    if (data.isEmpty())
        return false;

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory)))
        || !factory) {
        return false;
    }

    IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(data.constData()),
                                        static_cast<UINT>(data.size()));
    if (!stream) {
        factory->Release();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromStream(stream, nullptr,
                                                  WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr) || !decoder) {
        factory->Release();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) {
        factory->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr) && converter) {
        hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
    }
    frame->Release();
    if (FAILED(hr) || !converter) {
        factory->Release();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        converter->Release();
        factory->Release();
        return false;
    }

    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32);
    if (image.isNull()) {
        converter->Release();
        factory->Release();
        return false;
    }

    hr = converter->CopyPixels(nullptr, static_cast<UINT>(image.bytesPerLine()),
                               static_cast<UINT>(image.sizeInBytes()), image.bits());
    converter->Release();
    factory->Release();
    if (FAILED(hr))
        return false;

    return image.save(outPath, "PNG");
#else
    Q_UNUSED(data)
    Q_UNUSED(outPath)
    return false;
#endif
}
