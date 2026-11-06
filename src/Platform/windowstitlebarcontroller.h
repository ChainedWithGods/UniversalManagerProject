#pragma once

#include <QObject>

class QQmlApplicationEngine;
class QQuickWindow;
class ModManager;

class WindowsTitleBarController : public QObject
{
    Q_OBJECT
public:
    explicit WindowsTitleBarController(QQmlApplicationEngine* engine, ModManager* modManager, QObject* parent = nullptr);

    Q_INVOKABLE void attachToWindow(QQuickWindow* window);

private:
    void ensureThemeConfigLoaded();
    void applyToWindow(QQuickWindow* window);

    QQmlApplicationEngine* m_engine;
    ModManager* m_modManager;
    QObject* m_themeConfigObject = nullptr;
};
