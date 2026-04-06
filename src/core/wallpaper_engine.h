#pragma once

#include <QObject>
#include <QWindow>
#include <QTimer>
#include <memory>
#include "../renderers/wallpaper_renderer.h"
#include "../platform/platform_utils.h"

class WallpaperEngine : public QObject {
    Q_OBJECT
public:
    explicit WallpaperEngine(QObject* parent = nullptr);
    ~WallpaperEngine();

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    void setRenderer(WallpaperRenderer* renderer);
    void setTargetFps(int fps);
    int targetFps() const { return m_targetFps; }
    void setPaused(bool paused);
    bool paused() const { return m_paused; }

private:
    void applyTimerInterval();

    QWindow* m_window;
    std::unique_ptr<WallpaperRenderer> m_renderer;
    PlatformUtils* m_platform;
    QTimer* m_timer;
    int m_targetFps;
    bool m_paused;
    bool m_running = false;
};
