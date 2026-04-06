#pragma once

#include "wallpaper_renderer.h"
#include <QBackingStore>
#include <QPainter>
#include <QString>
#include <QThread>
#include <QMutex>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoRenderer : public WallpaperRenderer {
    Q_OBJECT
public:
    explicit VideoRenderer(const QString& path, QObject* parent = nullptr);
    ~VideoRenderer();

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

private:
    QString m_path;
    QWindow* m_window = nullptr;
    QBackingStore* m_backingStore = nullptr;

    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_codecCtx = nullptr;
    int m_videoStream = -1;
    AVFrame* m_frame = nullptr;
    SwsContext* m_swsCtx = nullptr;
    uint8_t* m_rgbData[4] = { nullptr, nullptr, nullptr, nullptr };
    int m_rgbLinesize[4] = { 0, 0, 0, 0 };
    int m_outWidth = 0;
    int m_outHeight = 0;
    double m_frameDelayMs = 33.0;

    QMutex m_frameMutex;
    bool m_frameReady = false;
    std::atomic_bool m_stop = false;
    QThread* m_decodeThread = nullptr;

    std::atomic_bool m_paused = false;
    float m_volume = 1.0f;
    bool m_muted = false;

    bool openVideo();
    void closeVideo();
    bool rebuildScaleContextLocked(int outputW, int outputH);
    double detectFrameDelayMs() const;
    void decodeLoop();
    void renderFillMode(QPainter& painter, const QImage& image, const QRect& target);
};
