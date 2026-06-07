#include "gpu_video_renderer.h"
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

GpuVideoRenderer::GpuVideoRenderer(const QString& path, QObject* parent)
    : WallpaperRenderer(parent), m_path(path) {
}

GpuVideoRenderer::~GpuVideoRenderer() {
    if (m_player) {
        m_player->stop();
    }
    delete m_backingStore;
    m_backingStore = nullptr;
}

bool GpuVideoRenderer::canOpen(const QString& path) {
    // Rely on QMediaPlayer's format support — it uses native OS decoders.
    // This includes H.264, H.265, VP9, AV1 (if codecs installed), etc.
    static const QStringList supportedExtensions = {
        "mp4", "mkv", "webm", "avi", "mov", "wmv", "flv", "m4v", "ts", "mpeg", "mpg"
    };
    QString ext = QFileInfo(path).suffix().toLower();
    return supportedExtensions.contains(ext);
}

void GpuVideoRenderer::init(QWindow* window) {
    m_window = window;

    // Recreate backing store
    delete m_backingStore;
    m_backingStore = new QBackingStore(window);
    m_backingStore->resize(window->size());

    m_outWidth = window->width();
    m_outHeight = window->height();

    // Create QMediaPlayer with QVideoSink for frame capture
    if (!m_player) {
        m_player = new QMediaPlayer(this);

        m_audioOutput = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOutput);

        m_videoSink = new QVideoSink(this);
        m_player->setVideoSink(m_videoSink);

        // Connect video frame signal — this fires for every decoded frame.
        // On Windows, QMediaPlayer uses Windows Media Foundation which
        // does hardware-accelerated decoding (DXVA2 / D3D11VA) automatically.
        connect(m_videoSink, &QVideoSink::videoFrameChanged,
                this, &GpuVideoRenderer::onVideoFrameChanged);

        // Loop video when it reaches the end
        connect(m_player, &QMediaPlayer::mediaStatusChanged,
                this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) {
                m_player->setPosition(0);
                m_player->play();
            }
        });

        // Log errors
        connect(m_player, &QMediaPlayer::errorOccurred,
                this, [this](QMediaPlayer::Error error, const QString& errorString) {
            qWarning() << "GpuVideoRenderer: Media error" << error << errorString;
        });
    }

    // Set source and start playing
    m_player->setSource(QUrl::fromLocalFile(m_path));

    // Apply audio settings
    m_audioOutput->setVolume(m_volume);
    m_audioOutput->setMuted(m_muted);

    if (!m_paused) {
        m_player->play();
    }

    qDebug() << "GpuVideoRenderer: Initialized with GPU-accelerated playback for" << m_path;
}

void GpuVideoRenderer::onVideoFrameChanged(const QVideoFrame& frame) {
    if (!frame.isValid()) return;

    // Map the frame to CPU memory for painting.
    // On Windows with WMF backend, the actual decode is GPU-accelerated;
    // we just need the final RGB for our QBackingStore blit.
    QVideoFrame f = frame;
    if (f.map(QVideoFrame::ReadOnly)) {
        QImage img = f.toImage();
        if (!img.isNull()) {
            QMutexLocker locker(&m_frameMutex);
            // Scale to output size if needed
            if (img.width() != m_outWidth || img.height() != m_outHeight) {
                m_currentFrame = img.scaled(m_outWidth, m_outHeight,
                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            } else {
                m_currentFrame = img;
            }
            m_frameReady = true;
        }
        f.unmap();
    }
}

void GpuVideoRenderer::render() {
    if (!m_window || !m_backingStore) return;

    QMutexLocker locker(&m_frameMutex);
    if (!m_frameReady || m_currentFrame.isNull()) return;

    const QSize paintSize(m_outWidth, m_outHeight);
    m_backingStore->beginPaint(QRect(QPoint(0, 0), paintSize));
    QPaintDevice* device = m_backingStore->paintDevice();
    QPainter painter(device);

    renderFillMode(painter, m_currentFrame, QRect(0, 0, m_outWidth, m_outHeight));

    painter.end();
    m_backingStore->endPaint();
    m_backingStore->flush(QRect(QPoint(0, 0), paintSize));

    m_frameReady = false;
}

void GpuVideoRenderer::renderFillMode(QPainter& painter, const QImage& image, const QRect& target) {
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

void GpuVideoRenderer::resize(int w, int h) {
    m_outWidth = std::max(1, w);
    m_outHeight = std::max(1, h);
    if (m_backingStore) {
        m_backingStore->resize(QSize(m_outWidth, m_outHeight));
    }
}

void GpuVideoRenderer::setPaused(bool paused) {
    m_paused = paused;
    if (m_player) {
        if (paused) {
            m_player->pause();
        } else {
            m_player->play();
        }
    }
}

void GpuVideoRenderer::setVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    if (m_audioOutput) {
        m_audioOutput->setVolume(m_volume);
    }
}

void GpuVideoRenderer::setMuted(bool muted) {
    m_muted = muted;
    if (m_audioOutput) {
        m_audioOutput->setMuted(muted);
    }
}

void GpuVideoRenderer::setFillMode(FillMode mode) {
    m_fillMode = mode;
}
