#include "platform_utils.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <QDebug>

namespace {

BOOL CALLBACK FindWorkerWCallback(HWND topLevel, LPARAM lParam) {
    HWND shellView = FindWindowExW(topLevel, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!shellView) {
        return TRUE;
    }

    HWND worker = FindWindowExW(nullptr, topLevel, L"WorkerW", nullptr);
    if (worker) {
        *reinterpret_cast<HWND*>(lParam) = worker;
        return FALSE;
    }

    return TRUE;
}

HWND findWallpaperHost() {
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) {
        return nullptr;
    }

    DWORD_PTR result = 0;
    SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
    SendMessageTimeoutW(progman, 0x052C, 0xD, 1, SMTO_NORMAL, 1000, &result);

    HWND worker = nullptr;
    EnumWindows(FindWorkerWCallback, reinterpret_cast<LPARAM>(&worker));

    return worker ? worker : progman;
}

void applyWallpaperStyles(HWND hwnd) {
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD | WS_VISIBLE;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    exStyle &= ~WS_EX_APPWINDOW;
    exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
}

} // namespace

class WindowsUtils : public PlatformUtils {
public:
    void setToWallpaper(QWindow* window) override {
        HWND hwnd = (HWND)window->winId();
        HWND host = findWallpaperHost();

        if (host) {
            SetParent(hwnd, host);
            applyWallpaperStyles(hwnd);

            RECT rc = {};
            GetClientRect(host, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w <= 0 || h <= 0) {
                w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            }

            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        } else {
            qDebug() << "Wallpaper host not found, using desktop bottom z-order fallback.";
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    void setIgnoreInput(QWindow* window) override {
        HWND hwnd = (HWND)window->winId();
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
};

PlatformUtils* PlatformUtils::create() {
    return new WindowsUtils();
}
#endif
