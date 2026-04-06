#pragma once

#include "wallpaper_renderer.h"
#include <QBackingStore>
#include <QPainter>
#include <QElapsedTimer>

class ColorRenderer : public WallpaperRenderer {
    Q_OBJECT
public:
    explicit ColorRenderer(QObject* parent = nullptr) : WallpaperRenderer(parent) {}

    ~ColorRenderer() override {
        delete m_backingStore;
    }

    void init(QWindow* window) override {
        m_window = window;
        delete m_backingStore;
        m_backingStore = new QBackingStore(window);
        m_backingStore->resize(window->size());
    }

    void render() override {
        if (!m_window->isVisible()) return;

        m_backingStore->beginPaint(QRect(QPoint(0,0), m_window->size()));
        
        QPaintDevice* device = m_backingStore->paintDevice();
        QPainter painter(device);
        
        int w = m_window->width();
        int h = m_window->height();
        
        double t = m_timer.elapsed() / 1000.0;
        
        QLinearGradient gradient(0, 0, w, h);
        gradient.setColorAt(0, QColor::fromHsvF(fmod(t * 0.1, 1.0), 0.5, 0.2));
        gradient.setColorAt(1, QColor::fromHsvF(fmod(t * 0.1 + 0.5, 1.0), 0.5, 0.2));
        
        painter.fillRect(0, 0, w, h, gradient);
        
        painter.end();
        m_backingStore->endPaint();
        m_backingStore->flush(QRect(QPoint(0,0), m_window->size()));
    }

    void resize(int w, int h) override {
        m_backingStore->resize(QSize(w, h));
    }

    void setPaused(bool paused) override {
        m_paused = paused;
    }

private:
    QWindow* m_window = nullptr;
    QBackingStore* m_backingStore = nullptr;
    QElapsedTimer m_timer { []{ QElapsedTimer t; t.start(); return t; }() };
    bool m_paused = false;
};
