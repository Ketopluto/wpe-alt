#include "video_renderer.h"
#include <QDebug>
#include <algorithm>

VideoRenderer::VideoRenderer(const QString& path, QObject* parent)
    : WallpaperRenderer(parent), m_path(path) {
}

bool VideoRenderer::canOpen(const QString& path) {
    AVFormatContext* ctx = nullptr;
    if (avformat_open_input(&ctx, path.toUtf8().constData(), nullptr, nullptr) != 0) {
        return false;
    }
    if (avformat_find_stream_info(ctx, nullptr) < 0) {
        avformat_close_input(&ctx);
        return false;
    }
    int stream = -1;
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            stream = static_cast<int>(i);
            break;
        }
    }
    avformat_close_input(&ctx);
    return stream >= 0;
}

VideoRenderer::~VideoRenderer() {
    m_stop = true;
    if (m_decodeThread) {
        m_decodeThread->wait();
        delete m_decodeThread;
        m_decodeThread = nullptr;
    }
    closeVideo();
    delete m_backingStore;
    m_backingStore = nullptr;
}

void VideoRenderer::init(QWindow* window) {
    m_window = window;
    delete m_backingStore;
    m_backingStore = new QBackingStore(window);
    m_backingStore->resize(window->size());
    m_stop = false;
    m_frameReady = false;

    if (openVideo()) {
        m_decodeThread = QThread::create([this] { decodeLoop(); });
        m_decodeThread->start();
    } else {
        qDebug() << "VideoRenderer: failed to initialize video for" << m_path;
    }
}

bool VideoRenderer::openVideo() {
    closeVideo();

    if (avformat_open_input(&m_formatCtx, m_path.toUtf8().constData(), nullptr, nullptr) != 0) {
        qDebug() << "Could not open video file" << m_path;
        return false;
    }

    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        return false;
    }

    m_videoStream = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStream < 0) {
        qDebug() << "Could not find a video stream in" << m_path;
        return false;
    }

    const AVCodec* codec = avcodec_find_decoder(m_formatCtx->streams[m_videoStream]->codecpar->codec_id);
    if (!codec) {
        qDebug() << "Could not find decoder for stream in" << m_path;
        return false;
    }

    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx) {
        return false;
    }

    if (avcodec_parameters_to_context(m_codecCtx, m_formatCtx->streams[m_videoStream]->codecpar) < 0) {
        return false;
    }

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0) {
        return false;
    }

    m_frame = av_frame_alloc();
    if (!m_frame) {
        return false;
    }

    const int outputW = std::max(1, m_window ? m_window->width() : m_codecCtx->width);
    const int outputH = std::max(1, m_window ? m_window->height() : m_codecCtx->height);
    {
        QMutexLocker locker(&m_frameMutex);
        if (!rebuildScaleContextLocked(outputW, outputH)) {
            return false;
        }
    }

    m_frameDelayMs = detectFrameDelayMs();
    if (m_frameDelayMs < 5.0 || m_frameDelayMs > 1000.0) {
        m_frameDelayMs = 33.0;
    }

    return true;
}

void VideoRenderer::closeVideo() {
    QMutexLocker locker(&m_frameMutex);

    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_rgbData[0]) {
        av_freep(&m_rgbData[0]);
        for (int i = 0; i < 4; ++i) {
            m_rgbData[i] = nullptr;
            m_rgbLinesize[i] = 0;
        }
    }

    if (m_frame) {
        av_frame_free(&m_frame);
    }

    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
    }

    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
    }

    m_videoStream = -1;
    m_outWidth = 0;
    m_outHeight = 0;
    m_frameReady = false;
}

bool VideoRenderer::rebuildScaleContextLocked(int outputW, int outputH) {
    if (!m_codecCtx) {
        return false;
    }

    outputW = std::max(1, outputW);
    outputH = std::max(1, outputH);

    if (m_swsCtx && m_outWidth == outputW && m_outHeight == outputH) {
        return true;
    }

    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_rgbData[0]) {
        av_freep(&m_rgbData[0]);
        for (int i = 0; i < 4; ++i) {
            m_rgbData[i] = nullptr;
            m_rgbLinesize[i] = 0;
        }
    }

    if (av_image_alloc(m_rgbData, m_rgbLinesize, outputW, outputH, AV_PIX_FMT_BGRA, 1) < 0) {
        return false;
    }

    m_swsCtx = sws_getContext(m_codecCtx->width, m_codecCtx->height, m_codecCtx->pix_fmt,
                              outputW, outputH, AV_PIX_FMT_BGRA,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsCtx) {
        av_freep(&m_rgbData[0]);
        for (int i = 0; i < 4; ++i) {
            m_rgbData[i] = nullptr;
            m_rgbLinesize[i] = 0;
        }
        return false;
    }

    m_outWidth = outputW;
    m_outHeight = outputH;
    m_frameReady = false;
    return true;
}

double VideoRenderer::detectFrameDelayMs() const {
    if (!m_formatCtx || m_videoStream < 0) {
        return 33.0;
    }

    const AVStream* stream = m_formatCtx->streams[m_videoStream];
    double fps = 0.0;

    if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
        fps = av_q2d(stream->avg_frame_rate);
    }
    if (fps <= 0.0 && stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
        fps = av_q2d(stream->r_frame_rate);
    }

    if (fps <= 1.0 || fps > 240.0) {
        fps = 30.0;
    }

    return 1000.0 / fps;
}

void VideoRenderer::decodeLoop() {
    if (!m_formatCtx || !m_codecCtx) {
        return;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return;
    }

    while (!m_stop) {
        if (m_paused) {
            QThread::msleep(20);
            continue;
        }

        const int readResult = av_read_frame(m_formatCtx, packet);
        if (readResult == AVERROR_EOF) {
            av_seek_frame(m_formatCtx, m_videoStream, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(m_codecCtx);
            continue;
        }
        if (readResult < 0) {
            QThread::msleep(5);
            continue;
        }

        if (packet->stream_index != m_videoStream) {
            av_packet_unref(packet);
            continue;
        }

        const int sendResult = avcodec_send_packet(m_codecCtx, packet);
        av_packet_unref(packet);
        if (sendResult < 0 && sendResult != AVERROR(EAGAIN)) {
            continue;
        }

        while (!m_stop) {
            const int recvResult = avcodec_receive_frame(m_codecCtx, m_frame);
            if (recvResult == AVERROR(EAGAIN) || recvResult == AVERROR_EOF) {
                break;
            }
            if (recvResult < 0) {
                break;
            }

            {
                QMutexLocker locker(&m_frameMutex);
                if (m_swsCtx && m_rgbData[0]) {
                    sws_scale(m_swsCtx,
                              (uint8_t const* const*)m_frame->data,
                              m_frame->linesize,
                              0,
                              m_codecCtx->height,
                              m_rgbData,
                              m_rgbLinesize);
                    m_frameReady = true;
                }
            }

            QThread::msleep(static_cast<unsigned long>(std::clamp(m_frameDelayMs, 5.0, 1000.0)));
        }
    }

    av_packet_free(&packet);
}

void VideoRenderer::renderFillMode(QPainter& painter, const QImage& image, const QRect& target) {
    QSize imgSize = image.size();
    switch (m_fillMode) {
    case FillMode::Stretch:
        painter.drawImage(target, image);
        break;
    case FillMode::Fit: {
        QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatio);
        QRect dest(QPoint((target.width() - scaled.width()) / 2,
                          (target.height() - scaled.height()) / 2), scaled);
        painter.fillRect(target, Qt::black);
        painter.drawImage(dest, image);
        break;
    }
    case FillMode::Fill: {
        QSize scaled = imgSize.scaled(target.size(), Qt::KeepAspectRatioByExpanding);
        QRect dest(QPoint((target.width() - scaled.width()) / 2,
                          (target.height() - scaled.height()) / 2), scaled);
        painter.drawImage(dest, image);
        break;
    }
    case FillMode::Center:
        painter.fillRect(target, Qt::black);
        painter.drawImage(QPoint((target.width() - imgSize.width()) / 2,
                                 (target.height() - imgSize.height()) / 2), image);
        break;
    }
}

void VideoRenderer::render() {
    if (!m_window || !m_codecCtx || !m_backingStore) {
        return;
    }

    QMutexLocker locker(&m_frameMutex);
    if (!m_frameReady || !m_rgbData[0]) {
        return;
    }

    m_backingStore->beginPaint(QRect(QPoint(0,0), m_window->size()));
    QPaintDevice* device = m_backingStore->paintDevice();
    QPainter painter(device);
    
    QImage image(m_rgbData[0], m_outWidth, m_outHeight, m_rgbLinesize[0], QImage::Format_ARGB32);
    renderFillMode(painter, image, QRect(0, 0, m_window->width(), m_window->height()));
    
    painter.end();
    m_backingStore->endPaint();
    m_backingStore->flush(QRect(QPoint(0,0), m_window->size()));
    
    m_frameReady = false;
}

void VideoRenderer::resize(int w, int h) {
    if (m_backingStore) {
        m_backingStore->resize(QSize(w, h));
    }

    if (m_codecCtx) {
        QMutexLocker locker(&m_frameMutex);
        rebuildScaleContextLocked(w, h);
    }
}

void VideoRenderer::setPaused(bool paused) {
    m_paused = paused;
}

void VideoRenderer::setVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void VideoRenderer::setMuted(bool muted) {
    m_muted = muted;
}

void VideoRenderer::setFillMode(FillMode mode) {
    m_fillMode = mode;
}
