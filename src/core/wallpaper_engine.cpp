#include "wallpaper_engine.h"
#include <QScreen>
#include <QGuiApplication>
#include <algorithm>

WallpaperEngine::WallpaperEngine(QObject* parent)
    : QObject(parent), m_targetFps(30), m_paused(false) {
    m_window = new QWindow();
    m_window->setSurfaceType(QSurface::RasterSurface);
    m_window->setFlags(Qt::FramelessWindowHint | Qt::BypassWindowManagerHint | Qt::WindowStaysOnBottomHint);

    m_platform = PlatformUtils::create();

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, [this]() {
        if (m_renderer && !m_paused) {
            m_renderer->render();
        }
    });

    connect(m_window, &QWindow::widthChanged, [this]() {
        if (m_renderer) {
            m_renderer->resize(m_window->width(), m_window->height());
        }
    });
    connect(m_window, &QWindow::heightChanged, [this]() {
        if (m_renderer) {
            m_renderer->resize(m_window->width(), m_window->height());
        }
    });
}

WallpaperEngine::~WallpaperEngine() {
    m_timer->stop();
    m_renderer.reset();
    delete m_window;
    delete m_platform;
}

void WallpaperEngine::start() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    QRect geom = screen->virtualGeometry();
    m_window->setGeometry(geom);
    m_window->show();

    m_platform->setToWallpaper(m_window);
    m_platform->setIgnoreInput(m_window);
    m_window->setGeometry(0, 0, geom.width(), geom.height());

    if (m_renderer) {
        m_renderer->init(m_window);
        m_renderer->resize(m_window->width(), m_window->height());
        m_renderer->setPaused(m_paused);
    }

    applyTimerInterval();
    m_timer->start();
    m_running = true;
}

void WallpaperEngine::stop() {
    m_timer->stop();
    if (m_renderer) {
        m_renderer->setPaused(true);
    }
    m_window->hide();
    m_running = false;
}

void WallpaperEngine::setRenderer(WallpaperRenderer* renderer) {
    if (!renderer) {
        return;
    }

    if (m_renderer) {
        m_renderer->setPaused(true);
        m_renderer.reset();
    }

    m_renderer.reset(renderer);
    if (m_window->isVisible() && m_renderer) {
        m_renderer->init(m_window);
        m_renderer->resize(m_window->width(), m_window->height());
        m_renderer->setPaused(m_paused);
    }
}

void WallpaperEngine::setTargetFps(int fps) {
    m_targetFps = std::clamp(fps, 5, 120);
    applyTimerInterval();
}

void WallpaperEngine::setPaused(bool paused) {
    m_paused = paused;
    if (m_renderer) {
        m_renderer->setPaused(paused);
    }
}

void WallpaperEngine::applyTimerInterval() {
    const int interval = std::max(1000 / std::max(m_targetFps, 5), 5);
    m_timer->setInterval(interval);
}
