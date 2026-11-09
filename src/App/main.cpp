#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
#include <QVariant>
#include <QTime>
#include <QByteArray>
#include <QSurfaceFormat>
#include <QtWebEngineQuick>
#include <QLoggingCategory>
#include <QIcon>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QPointer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QLibrary>
#include <QLibraryInfo>
#include <QQmlNetworkAccessManagerFactory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QQuickWebEngineProfile>
#include <QWebEngineUrlRequestInterceptor>
#include <cstdio>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "modmanager.h"
#include "luaprofilemanager.h"
#include "windowstitlebarcontroller.h"
#include "NexusOAuth.h"
#include "NexusWebSession.h"
#include "AggressiveRam.h"
#include "TrayHelper.h"
#include "NotificationManager.h"
#include "AnimatedImageStore.h"
#include "DownloadManager.h"
#include "UpdateChecker.h"
#include "TutorialDownloader.h"
#include "resourcepaths.h"

namespace {

class QmlNetworkAccessManager : public QNetworkAccessManager
{
public:
    explicit QmlNetworkAccessManager(QObject* parent = nullptr)
        : QNetworkAccessManager(parent)
    {
    }

protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoingData = nullptr) override
    {
        if (!request.hasRawHeader("User-Agent")) {
            QNetworkRequest enriched(request);
            enriched.setRawHeader("User-Agent",
                                  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) UniversalModManager/1.0");
            return QNetworkAccessManager::createRequest(operation, enriched, outgoingData);
        }
        return QNetworkAccessManager::createRequest(operation, request, outgoingData);
    }
};

class QmlNetworkAccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager* create(QObject* parent) override
    {
        auto* manager = new QmlNetworkAccessManager(parent);
        manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* diskCache = new QNetworkDiskCache(manager);
        diskCache->setCacheDirectory(
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/qml-http"));
        diskCache->setMaximumCacheSize(512 * 1024 * 1024);
        manager->setCache(diskCache);

        return manager;
    }
};

class YoutubeRefererInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

    void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        const QString host = info.requestUrl().host();
        if (host.endsWith(QLatin1String("youtube.com"))
            || host.endsWith(QLatin1String("youtube-nocookie.com"))
            || host.endsWith(QLatin1String("ytimg.com"))) {
            info.setHttpHeader("Referer", "https://www.youtube.com/");
        }
    }
};

QtMessageHandler g_previousMessageHandler = nullptr;

void filterWebEngineConsoleNoise(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const bool jsConsole = (context.category && qstrcmp(context.category, "js") == 0)
        || message.startsWith(QLatin1String("js: "));
    if (message.contains(QLatin1String("Failed to create WebGPU Context Provider"))
        || (jsConsole
            && (message.contains(QLatin1String("preloaded using link preload"))
                || message.contains(QLatin1String("aria-hidden"))
                || message.contains(QLatin1String("doubleclick"))
                || message.contains(QLatin1String("googlevideo"))
                || message.contains(QLatin1String("ytimg"))
                || message.contains(QLatin1String("youtube"))))) {
        return;
    }

    if (g_previousMessageHandler)
        g_previousMessageHandler(type, context, message);
    else
        fprintf(stderr, "%s\n", qPrintable(message));
}

void suppressFfmpegLogs()
{
#ifdef Q_OS_WIN
    const QStringList searchDirectories = {
        QCoreApplication::applicationDirPath(),
        QLibraryInfo::path(QLibraryInfo::BinariesPath)
    };

    static QLibrary avutil;
    for (const QString& directory : searchDirectories) {
        const QStringList candidates = QDir(directory).entryList(
            {QStringLiteral("avutil-*.dll")}, QDir::Files, QDir::Name);
        for (const QString& name : candidates) {
            avutil.setFileName(QDir(directory).filePath(name));
            if (!avutil.load()) {
                continue;
            }

            using SetLogLevel = void (*)(int);
            auto setLogLevel = reinterpret_cast<SetLogLevel>(
                avutil.resolve("av_log_set_level"));
            if (setLogLevel) {
                setLogLevel(-8);
                return;
            }
            avutil.unload();
        }
    }
#endif
}

class LibraryEscapeFilter : public QObject
{
public:
    explicit LibraryEscapeFilter(QQuickWindow* window)
        : QObject(window)
        , m_window(window)
    {
        if (auto* app = QCoreApplication::instance())
            app->installEventFilter(this);
        logEscape(QStringLiteral("filter installed"));
    }

    ~LibraryEscapeFilter() override
    {
        if (auto* app = QCoreApplication::instance())
            app->removeEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        if (event->type() != QEvent::KeyPress || !m_window
                || QGuiApplication::focusWindow() != m_window.data()) {
            return QObject::eventFilter(watched, event);
        }

        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() != Qt::Key_Escape || keyEvent->isAutoRepeat())
            return QObject::eventFilter(watched, event);

        logEscape(QStringLiteral("escape received"));
        auto* page = m_window->findChild<QObject*>(QStringLiteral("modLibraryPage"));
        const bool pageVisible = page && page->property("visible").toBool();
        logEscape(QStringLiteral("escape: page=%1 visible=%2")
                      .arg(page ? 1 : 0).arg(pageVisible ? 1 : 0));
        if (page && pageVisible && !itemInsidePopup(m_window->activeFocusItem())) {
            const bool invoked = QMetaObject::invokeMethod(page, "handleEscape");
            logEscape(QStringLiteral("handleEscape invoked=%1").arg(invoked));
            if (invoked) {
                event->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static void logEscape(const QString& line)
    {
        QFile file(QDir::tempPath() + QStringLiteral("/um_escape.log"));
        if (file.open(QIODevice::Append | QIODevice::Text))
            file.write((QTime::currentTime().toString(Qt::ISODate) + " " + line + "\n").toUtf8());
    }

    static bool itemInsidePopup(QQuickItem* item)
    {
        for (QQuickItem* current = item; current; current = current->parentItem()) {
            if (QByteArray(current->metaObject()->className()).contains("QQuickPopupItem"))
                return true;
        }
        return false;
    }

    QPointer<QQuickWindow> m_window;
};

}

static QString getStartupSettingsFilePath()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        return QDir::currentPath() + "/settings.json";
    }

    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return appDataDir + "/settings.json";
}

static bool shouldShowManagerConsoleOnStartup()
{
    QFile file(getStartupSettingsFilePath());
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject obj = doc.object();
    return !obj.value("managerHideConsole").toBool(true);
}

#ifdef Q_OS_WIN
static void ensureManagerConsoleVisibleOnStartup()
{
    if (HWND consoleWindow = GetConsoleWindow()) {
        ShowWindow(consoleWindow, SW_SHOWNORMAL);
        return;
    }

    if (!AttachConsole(ATTACH_PARENT_PROCESS) && !AllocConsole()) {
        return;
    }

    FILE* redirectedStream = nullptr;
    _wfreopen_s(&redirectedStream, L"CONIN$", L"r", stdin);
    _wfreopen_s(&redirectedStream, L"CONOUT$", L"w", stdout);
    _wfreopen_s(&redirectedStream, L"CONOUT$", L"w", stderr);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleW(L"Universal Manager");

    if (HWND consoleWindow = GetConsoleWindow()) {
        ShowWindow(consoleWindow, SW_SHOWNORMAL);
    }
}
#endif

int main(int argc, char* argv[])
{
    QtWebEngineQuick::initialize();
    g_previousMessageHandler = qInstallMessageHandler(filterWebEngineConsoleNoise);

    QCoreApplication::setOrganizationName("");
    QCoreApplication::setApplicationName("UniversalManager");

    QString pendingNxmUrl;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromUtf8(argv[i]).trimmed();
        if (argument.startsWith(QStringLiteral("nxm://"), Qt::CaseInsensitive)
            && argument.contains(QStringLiteral("/mods/"), Qt::CaseInsensitive)
            && argument.contains(QStringLiteral("/files/"), Qt::CaseInsensitive)) {
            pendingNxmUrl = argument;
            break;
        }
    }

    TrayHelper trayHelper("UniversalManagerSingleInstance");
    const QByteArray instanceMessage = pendingNxmUrl.isEmpty()
        ? QByteArrayLiteral("SHOW\n")
        : QByteArray("NXM " + pendingNxmUrl.toUtf8() + "\n");
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    if (trayHelper.isAnotherInstanceRunning(instanceMessage)) {
        return 0;
    }

#ifdef Q_OS_WIN
    if (shouldShowManagerConsoleOnStartup()) {
        ensureManagerConsoleVisibleOnStartup();
    }
#endif

    QLoggingCategory::setFilterRules(
        "qt.multimedia.ffmpeg*=false\n"
        "qt.multimedia*=false\n"
        "qt.webengine*=false\n"
        "qt.webenginecontext*=false"
    );

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QSurfaceFormat format;
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

#ifdef Q_OS_WIN
    qputenv("QT_D3D_NO_FLIP", "1");
#endif

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(ResourcePaths::appIcon));

    suppressFfmpegLogs();

    trayHelper.listen();

    QQuickStyle::setStyle("Fusion");

    if (auto* webProfile = QQuickWebEngineProfile::defaultProfile())
        webProfile->setUrlRequestInterceptor(new YoutubeRefererInterceptor(webProfile));

    QmlNetworkAccessManagerFactory qmlNetworkAccessManagerFactory;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&qmlNetworkAccessManagerFactory);

    auto modManager = new ModManager(&app);
    auto downloadManager = new DownloadManager(&app);
    auto notificationManager = new NotificationManager(&app);
    auto updateChecker = new UpdateChecker(&app);
    auto tutorialDownloader = new TutorialDownloader(&app);
    modManager->setDownloadManager(downloadManager);
    auto titleBarController = new WindowsTitleBarController(&engine, modManager, &app);
    auto aggressiveRam = new AggressiveRam(&app, modManager);
    auto animatedImageStore = new AnimatedImageStore(&app);

    engine.rootContext()->setContextProperty("modManager", modManager);
    engine.rootContext()->setContextProperty("nexusOAuth", modManager->nexusOAuth());
    engine.rootContext()->setContextProperty("nexusWebSession", modManager->nexusWebSession());
    engine.rootContext()->setContextProperty("luaProfileManager", modManager->luaProfileManager());
    engine.rootContext()->setContextProperty("qmlEngine", &engine);
    engine.rootContext()->setContextProperty("applicationDirPath", QCoreApplication::applicationDirPath());
    engine.rootContext()->setContextProperty("aggressiveRam", aggressiveRam);
    engine.rootContext()->setContextProperty("animatedImageStore", animatedImageStore);
    engine.rootContext()->setContextProperty("downloadManager", downloadManager);
    engine.rootContext()->setContextProperty("notificationManager", notificationManager);
    engine.rootContext()->setContextProperty("updateChecker", updateChecker);
    engine.rootContext()->setContextProperty("tutorialDownloader", tutorialDownloader);
    engine.rootContext()->setContextProperty("trayHelper", &trayHelper);
    engine.rootContext()->setContextProperty("titleBarController", titleBarController);

    auto mainWindowRef = std::make_shared<QPointer<QQuickWindow>>();
    const auto activateMainWindow = [mainWindowRef, &engine]() {
        QQuickWindow* window = mainWindowRef->data();
        if (!window) {
            const auto roots = engine.rootObjects();
            if (!roots.isEmpty())
                window = qobject_cast<QQuickWindow*>(roots.first());
        }
        if (!window)
            return;
        if (window->visibility() == QWindow::Minimized)
            window->showNormal();
        else if (!window->isVisible())
            window->show();
        window->raise();
        window->requestActivate();
#ifdef Q_OS_WIN
        if (const HWND hwnd = reinterpret_cast<HWND>(window->winId())) {
            if (IsIconic(hwnd))
                ShowWindow(hwnd, SW_RESTORE);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetForegroundWindow(hwnd);
            SetActiveWindow(hwnd);
        }
#endif
    };

    QObject::connect(downloadManager, &DownloadManager::downloadCompleted, notificationManager,
                     [notificationManager](const QString& filePath) {
                         notificationManager->notify(QStringLiteral("Download complete"),
                                                     QFileInfo(filePath).fileName(),
                                                     QStringLiteral("download.svg"), 4200);
                     });

    QObject::connect(&trayHelper, &TrayHelper::anotherInstanceStarted, &app,
                     [activateMainWindow]() { activateMainWindow(); });
    QObject::connect(&trayHelper, &TrayHelper::nxmUrlReceived, &app,
                     [modManager, activateMainWindow](const QString& url) {
                         activateMainWindow();
                         modManager->handleNxmUrl(url);
                     });

    QUrl url;
    if (modManager->showSplashScreen()) {
        url = QUrl(ResourcePaths::splashScreen);
    } else {
        url = QUrl(ResourcePaths::mainQml);
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [modManager, titleBarController, mainWindowRef](QObject* obj, const QUrl& objUrl) {
                         auto window = qobject_cast<QQuickWindow*>(obj);
                         if (!window)
                             return;

                         const bool isMainWindow = objUrl == QUrl(ResourcePaths::mainQml);
                         if (isMainWindow) {
                             *mainWindowRef = window;
                             titleBarController->attachToWindow(window);
                             new LibraryEscapeFilter(window);
                         }

                         auto limiterTimer = std::make_shared<QElapsedTimer>();
                         limiterTimer->start();

                         QObject::connect(window, &QQuickWindow::beforeRendering, window,
                                          [limiterTimer, modManager]() {
                                              if (!modManager->limitFpsOnDefocusForRender()) {
                                                  limiterTimer->restart();
                                                  return;
                                              }
                                              if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
                                                  limiterTimer->restart();
                                                  return;
                                              }
                                              const qint64 targetMs = 66;
                                              const qint64 elapsed = limiterTimer->elapsed();
                                              if (elapsed < targetMs)
                                                  QThread::msleep(static_cast<unsigned long>(targetMs - elapsed));
                                              limiterTimer->restart();
                                          }, Qt::DirectConnection);
                     }, Qt::QueuedConnection);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [updateChecker](QObject* obj, const QUrl& objUrl) {
                         if (!obj || objUrl != QUrl(ResourcePaths::mainQml))
                             return;
                         QTimer::singleShot(1500, updateChecker, &UpdateChecker::checkForUpdates);
                     }, Qt::QueuedConnection);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [modManager](QObject* obj, const QUrl& objUrl) {
                         if (!obj || objUrl != QUrl(ResourcePaths::mainQml))
                             return;
                         QTimer::singleShot(1200, modManager, [modManager]() {
                             modManager->ensureLuaProfileFolders();
                         });
                     }, Qt::QueuedConnection);
    engine.load(url);

    if (!pendingNxmUrl.isEmpty()) {
        QTimer::singleShot(1200, modManager, [modManager, pendingNxmUrl]() {
            modManager->handleNxmUrl(pendingNxmUrl);
        });
    }

    return app.exec();
}
