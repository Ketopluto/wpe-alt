#include "platform_utils.h"
#ifndef Q_OS_WIN

class StubUtils : public PlatformUtils {
public:
    void setToWallpaper(QWindow* window) override {
        // Platform specific implementation needed
        window->setLower();
    }

    void setIgnoreInput(QWindow* window) override {
        // Platform specific implementation needed
    }
};

PlatformUtils* PlatformUtils::create() {
    return new StubUtils();
}
#endif
