#pragma once

#include "wallpaper_renderer.h"
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QBackingStore>
#include <QPainter>
#include <QString>
#include <QAudioOutput>
#include <atomic>

class GpuVideoRenderer : public WallpaperRenderer {
    Q_OBJECT
public:
    explicit GpuVideoRenderer(const QString& path, QObject* parent = nullptr);
    ~GpuVideoRenderer();

    static bool canOpen(const QString& path);

    void init(QWindow* window) override;
    void render() override;
    void resize(int w, int h) override;
    void setPaused(bool paused) override;
    void setVolume(float volume) override;
    void setMuted(bool muted) override;
    void setFillMode(FillMode mode) override;

    float volume() const { return m_volume; }
    bool muted() const { return m_muted; }

private slots:
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    void renderFillMode(QPainter& painter, const QImage& image, const QRect& target);

    QString m_path;
    QWindow* m_window = nullptr;
    QBackingStore* m_backingStore = nullptr;

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QVideoSink* m_videoSink = nullptr;

    QImage m_currentFrame;
    QMutex m_frameMutex;
    bool m_frameReady = false;

    int m_outWidth = 0;
    int m_outHeight = 0;

    float m_volume = 1.0f;
    bool m_muted = false;
    std::atomic_bool m_paused = false;
};
