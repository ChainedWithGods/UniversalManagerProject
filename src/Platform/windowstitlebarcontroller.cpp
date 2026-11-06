#include "windowstitlebarcontroller.h"
#include <QColor>
#include <QDebug>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantMap>
#include "modmanager.h"
#include "resourcepaths.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

namespace {
QColor colorFromVariant(const QVariant& value)
{
    if (!value.isValid()) {
        return {};
    }

    if (value.canConvert<QColor>()) {
        const QColor color = value.value<QColor>();
        if (color.isValid()) {
            return color;
        }
    }

    const QColor color(value.toString());
    if (color.isValid()) {
        return color;
    }

    return {};
}

QColor flattenColor(const QColor& color, const QColor& backgroundColor)
{
    if (!color.isValid()) {
        return backgroundColor;
    }

    if (color.alpha() >= 255) {
        return QColor(color.red(), color.green(), color.blue());
    }

    const qreal alpha = color.alphaF();
    const qreal inverseAlpha = 1.0 - alpha;
    return QColor(
        qRound(color.red() * alpha + backgroundColor.red() * inverseAlpha),
        qRound(color.green() * alpha + backgroundColor.green() * inverseAlpha),
        qRound(color.blue() * alpha + backgroundColor.blue() * inverseAlpha)
    );
}

QColor blendColors(const QColor& baseColor, const QColor& tintColor, qreal tintAmount)
{
    if (!baseColor.isValid()) {
        return tintColor;
    }
    if (!tintColor.isValid()) {
        return baseColor;
    }

    const qreal clampedTintAmount = qBound(0.0, tintAmount, 1.0);
    const qreal baseAmount = 1.0 - clampedTintAmount;

    return QColor(
        qRound(baseColor.red() * baseAmount + tintColor.red() * clampedTintAmount),
        qRound(baseColor.green() * baseAmount + tintColor.green() * clampedTintAmount),
        qRound(baseColor.blue() * baseAmount + tintColor.blue() * clampedTintAmount)
    );
}

bool shouldUseDarkTitleText(const QColor& color)
{
    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    return luminance >= 160;
}

#ifdef Q_OS_WIN
COLORREF toColorRef(const QColor& color)
{
    return RGB(color.red(), color.green(), color.blue());
}
#endif
}

WindowsTitleBarController::WindowsTitleBarController(QQmlApplicationEngine* engine, ModManager* modManager, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_modManager(modManager)
{
    ensureThemeConfigLoaded();
}

void WindowsTitleBarController::attachToWindow(QQuickWindow* window)
{
    if (!window || !m_modManager) {
        return;
    }

    QPointer<QQuickWindow> trackedWindow(window);
    const auto refresh = [this, trackedWindow]() {
        if (trackedWindow) {
            applyToWindow(trackedWindow);
        }
    };

    QTimer::singleShot(0, window, refresh);
    QObject::connect(window, &QQuickWindow::visibleChanged, window, refresh);
    QObject::connect(m_modManager, &ModManager::themeChanged, window, refresh);
    QObject::connect(m_modManager, &ModManager::accentColorChanged, window, refresh);
    QObject::connect(m_modManager, &ModManager::navBarColorChanged, window, refresh);
}

void WindowsTitleBarController::ensureThemeConfigLoaded()
{
    if (m_themeConfigObject || !m_engine) {
        return;
    }

    QQmlComponent component(m_engine, QUrl(ResourcePaths::themeConfig), this);
    if (component.isError()) {
        qWarning() << "Failed to load ThemeConfig.qml:" << component.errors();
        return;
    }

    QObject* themeConfig = component.create();
    if (!themeConfig) {
        qWarning() << "Failed to create ThemeConfig.qml object:" << component.errors();
        return;
    }

    themeConfig->setParent(this);
    m_themeConfigObject = themeConfig;
}

void WindowsTitleBarController::applyToWindow(QQuickWindow* window)
{
#ifndef Q_OS_WIN
    Q_UNUSED(window);
    return;
#else
    if (!window || !m_modManager) {
        return;
    }

    ensureThemeConfigLoaded();

    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const QString themeName = m_modManager->theme();
    QString palettePropertyName = "cozy";
    if (themeName.compare("Glassmorphic", Qt::CaseInsensitive) == 0) {
        palettePropertyName = "glassmorphic";
    } else if (themeName.compare("Crimson", Qt::CaseInsensitive) == 0) {
        palettePropertyName = "crimson";
    } else if (themeName.compare("Wavey", Qt::CaseInsensitive) == 0) {
        palettePropertyName = "wavey";
    }

    QVariantMap palette;
    if (m_themeConfigObject) {
        palette = m_themeConfigObject->property(palettePropertyName.toUtf8().constData()).toMap();
    }

    const QColor fallbackDark("#16181d");
    const QColor bgColor = flattenColor(colorFromVariant(palette.value("bgColor")), fallbackDark);
    const QColor surfaceColor = flattenColor(colorFromVariant(palette.value("surfaceColor")), bgColor);
    const QColor primaryColor = flattenColor(colorFromVariant(palette.value("primaryColor")), surfaceColor);
    const QColor titleBarColor = flattenColor(colorFromVariant(palette.value("titleBarColor")), surfaceColor);

    QColor captionColor;

    if (themeName.compare("Glassmorphic", Qt::CaseInsensitive) == 0) {
        const QColor customAccent(m_modManager->accentColor());
        const QColor accentBase = customAccent.isValid() ? customAccent : primaryColor;
        captionColor = blendColors(QColor("#1a1d22"), accentBase, 0.28);
    } else {

        captionColor = titleBarColor;
    }

    const QColor textColor = shouldUseDarkTitleText(captionColor) ? QColor(Qt::black) : QColor(Qt::white);
    const QColor borderColor = captionColor;
    const BOOL immersiveDarkMode = textColor == QColor(Qt::white);

    const COLORREF nativeCaptionColor = toColorRef(captionColor);
    const COLORREF nativeTextColor = toColorRef(textColor);
    const COLORREF nativeBorderColor = toColorRef(borderColor);

    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &immersiveDarkMode, sizeof(immersiveDarkMode));
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &nativeCaptionColor, sizeof(nativeCaptionColor));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &nativeTextColor, sizeof(nativeTextColor));
    DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &nativeBorderColor, sizeof(nativeBorderColor));
#endif
}
