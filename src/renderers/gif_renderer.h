#pragma once

#include "wallpaper_renderer.h"
#include <QBackingStore>
#include <QPainter>
#include <QMovie>
#include <QBuffer>
#include <QFile>

class GifRenderer : public WallpaperRenderer {
    Q_OBJECT
public:
    explicit GifRenderer(const QString& path, QObject* parent = nullptr)
        : WallpaperRenderer(parent), m_path(path) {}

    ~GifRenderer() override {
        delete m_movie;
        delete m_backingStore;
    }

    static bool canOpen(const QString& path) {
        QString lower = path.toLower();
        return lower.endsWith(".gif") || lower.endsWith(".apng");
    }

    void init(QWindow* window) override {
        m_window = window;
        delete m_backingStore;
        m_backingStore = new QBackingStore(window);
        m_backingStore->resize(window->size());

        delete m_movie;
        m_movie = new QMovie(m_path);
        if (!m_movie->isValid()) {
            qWarning("GifRenderer: cannot open %s", qPrintable(m_path));
            return;
        }
        m_movie->setCacheMode(QMovie::CacheAll);

        connect(m_movie, &QMovie::frameChanged, this, [this]() {
            m_dirty = true;
        });

        m_movie->start();
    }

    void render() override {
        if (!m_window || !m_backingStore || !m_movie || !m_dirty) {
            return;
        }

        QImage frame = m_movie->currentImage();
        if (frame.isNull()) return;

        m_backingStore->beginPaint(QRect(QPoint(0, 0), m_window->size()));
        QPaintDevice* device = m_backingStore->paintDevice();
        QPainter painter(device);

        QRect target(0, 0, m_window->width(), m_window->height());
        QSize imgSize = frame.size();

        switch (m_fillMode) {
        case FillMode::Stretch:
            painter.drawImage(target, frame);
            break;
        case FillMode::Fit: {
            QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatio);
            QRect dest(QPoint((target.width() - scaled.width()) / 2,
                              (target.height() - scaled.height()) / 2), scaled);
            painter.fillRect(target, Qt::black);
            painter.drawImage(dest, frame);
            break;
        }
        case FillMode::Fill: {
            QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatioByExpanding);
            QRect dest(QPoint((target.width() - scaled.width()) / 2,
                              (target.height() - scaled.height()) / 2), scaled);
            painter.drawImage(dest, frame);
            break;
        }
        case FillMode::Center:
            painter.fillRect(target, Qt::black);
            painter.drawImage(QPoint((target.width() - imgSize.width()) / 2,
                                     (target.height() - imgSize.height()) / 2), frame);
            break;
        }

        painter.end();
        m_backingStore->endPaint();
        m_backingStore->flush(QRect(QPoint(0, 0), m_window->size()));
        m_dirty = false;
    }

    void resize(int w, int h) override {
        if (m_backingStore) {
            m_backingStore->resize(QSize(w, h));
        }
        m_dirty = true;
    }

    void setPaused(bool paused) override {
        m_paused = paused;
        if (m_movie) {
            m_movie->setPaused(paused);
        }
    }

    void setFillMode(FillMode mode) override {
        m_fillMode = mode;
        m_dirty = true;
    }

private:
    QString m_path;
    QWindow* m_window = nullptr;
    QBackingStore* m_backingStore = nullptr;
    QMovie* m_movie = nullptr;
    bool m_dirty = false;
    bool m_paused = false;
};
