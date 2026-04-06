#pragma once

#include <QWindow>

class PlatformUtils {
public:
    virtual ~PlatformUtils() = default;
    virtual void setToWallpaper(QWindow* window) = 0;
    virtual void setIgnoreInput(QWindow* window) = 0;
    
    static PlatformUtils* create();
};
