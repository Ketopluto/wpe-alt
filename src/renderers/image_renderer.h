#pragma once

#include "wallpaper_renderer.h"
#include <QBackingStore>
#include <QImageReader>
#include <QPainter>

class ImageRenderer : public WallpaperRenderer {
    Q_OBJECT
public:
    explicit ImageRenderer(const QString& path, QObject* parent = nullptr)
        : WallpaperRenderer(parent), m_path(path) {}
    ~ImageRenderer() override {
        delete m_backingStore;
    }

    void init(QWindow* window) override {
        m_window = window;
        delete m_backingStore;
        m_backingStore = new QBackingStore(window);
        m_backingStore->resize(window->size());

        QImageReader reader(m_path);
        reader.setAutoTransform(true);
        m_image = reader.read();
        m_dirty = true;
    }

    void render() override {
        if (!m_window || !m_backingStore || !m_dirty) {
            return;
        }

        m_backingStore->beginPaint(QRect(QPoint(0, 0), m_window->size()));
        QPaintDevice* device = m_backingStore->paintDevice();
        QPainter painter(device);

        QRect target(0, 0, m_window->width(), m_window->height());

        if (!m_image.isNull()) {
            QSize imgSize = m_image.size();

            switch (m_fillMode) {
            case FillMode::Stretch:
                painter.drawImage(target, m_image);
                break;
            case FillMode::Fit: {
                QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatio);
                QRect dest(QPoint((target.width() - scaled.width()) / 2,
                                  (target.height() - scaled.height()) / 2), scaled);
                painter.fillRect(target, Qt::black);
                painter.drawImage(dest, m_image);
                break;
            }
            case FillMode::Fill: {
                QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatioByExpanding);
                QRect dest(QPoint((target.width() - scaled.width()) / 2,
                                  (target.height() - scaled.height()) / 2), scaled);
                painter.drawImage(dest, m_image);
                break;
            }
            case FillMode::Center:
                painter.fillRect(target, Qt::black);
                painter.drawImage(QPoint((target.width() - imgSize.width()) / 2,
                                         (target.height() - imgSize.height()) / 2), m_image);
                break;
            }
        } else {
            painter.fillRect(target, Qt::black);
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
        Q_UNUSED(paused);
    }

    void setFillMode(FillMode mode) override {
        m_fillMode = mode;
        m_dirty = true;
    }

private:
    QString m_path;
    QWindow* m_window = nullptr;
    QBackingStore* m_backingStore = nullptr;
    QImage m_image;
    bool m_dirty = false;
};
