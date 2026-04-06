#pragma once

#include <QObject>
#include <QWindow>

enum class FillMode { Stretch, Fill, Fit, Center };

class WallpaperRenderer : public QObject {
    Q_OBJECT
public:
    explicit WallpaperRenderer(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~WallpaperRenderer() = default;

    virtual void init(QWindow* window) = 0;
    virtual void render() = 0;
    virtual void resize(int w, int h) = 0;
    virtual void setPaused(bool paused) = 0;

    virtual void setVolume(float /*volume*/) {}
    virtual void setMuted(bool /*muted*/) {}
    virtual void setFillMode(FillMode mode) { m_fillMode = mode; }

protected:
    FillMode m_fillMode = FillMode::Fill;
};
