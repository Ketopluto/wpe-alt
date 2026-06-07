#include "platform_utils.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <QDebug>

namespace {

// --------------------------------------------------------------------------
// WorkerW Native Desktop Injection for Windows 10/11.
//
// This is the proven technique used by Wallpaper Engine and Lively Wallpaper:
//   1. Send undocumented message 0x052C to Progman to spawn a WorkerW window.
//   2. Enumerate windows to find the WorkerW behind SHELLDLL_DefView.
//   3. Use SetParent to embed our wallpaper window into that WorkerW.
//
// Benefits:
//   - Wallpaper sits perfectly under desktop icons.
//   - Taskbar is NEVER hidden (DWM manages stacking correctly).
//   - Desktop icons remain fully visible and clickable.
// --------------------------------------------------------------------------

HWND g_workerW = nullptr;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM /*lParam*/) {
    // Find the WorkerW window that sits BEHIND SHELLDLL_DefView.
    // After sending message 0x052C to Progman, Windows creates two WorkerW windows.
    // The one we want is the one that does NOT contain SHELLDLL_DefView — it is the
    // blank canvas behind the desktop icons.
    HWND defView = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr) {
        // Found the WorkerW that contains SHELLDLL_DefView.
        // The NEXT WorkerW sibling (created by message 0x052C) is our target.
        g_workerW = FindWindowExW(nullptr, hwnd, L"WorkerW", nullptr);
    }
    return TRUE;
}

HWND findOrCreateWorkerW() {
    g_workerW = nullptr;

    // Find Progman
    HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) {
        qWarning() << "WorkerW injection: Could not find Progman window";
        return nullptr;
    }

    // Send the undocumented message to Progman. This makes Windows create
    // a WorkerW window behind the desktop icons. The message is:
    //   SendMessageTimeout(Progman, 0x052C, 0xD, 0)
    // 0xD tells Progman to spawn the WorkerW.
    SendMessageTimeoutW(progman, 0x052C, 0xD, 0, SMTO_NORMAL, 1000, nullptr);

    // Now enumerate all top-level windows to find the WorkerW behind SHELLDLL_DefView
    EnumWindows(EnumWindowsProc, 0);

    if (!g_workerW) {
        // Fallback: try sending 0x052C with different params
        SendMessageTimeoutW(progman, 0x052C, 0xD, 1, SMTO_NORMAL, 1000, nullptr);
        EnumWindows(EnumWindowsProc, 0);
    }

    if (g_workerW) {
        qDebug() << "WorkerW injection: Found WorkerW at" << (void*)g_workerW;
    } else {
        qWarning() << "WorkerW injection: Could not find WorkerW after Progman message";
    }

    return g_workerW;
}

} // namespace

class WindowsUtils : public PlatformUtils {
public:
    ~WindowsUtils() override {
        restoreDesktop();
    }

    void setToWallpaper(QWindow* window, QRect geometry = QRect()) override {
        restoreDesktop();

        HWND hwnd = (HWND)window->winId();

        // --- WorkerW Native Injection ---
        HWND workerW = findOrCreateWorkerW();

        if (workerW) {
            // Embed our window into WorkerW.
            // This makes DWM treat our window as part of the desktop layer,
            // which means the taskbar will NEVER auto-hide.
            m_originalParent = SetParent(hwnd, workerW);
            m_injectedHwnd = hwnd;
            m_workerW = workerW;

            // Remove decorations
            LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
            m_originalStyle = style;
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            style |= WS_CHILD | WS_VISIBLE;
            SetWindowLongPtrW(hwnd, GWL_STYLE, style);

            LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            m_originalExStyle = exStyle;
            exStyle &= ~(WS_EX_APPWINDOW);
            exStyle |= WS_EX_NOACTIVATE;
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

            // Size: use the full virtual screen (span all monitors) or specific geometry
            int x, y, w, h;
            if (geometry.isValid()) {
                x = geometry.x();
                y = geometry.y();
                w = geometry.width();
                h = geometry.height();
            } else {
                x = GetSystemMetrics(SM_XVIRTUALSCREEN);
                y = GetSystemMetrics(SM_YVIRTUALSCREEN);
                w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            }

            // No need for the -1 pixel hack anymore — we're parented inside WorkerW,
            // so DWM Fullscreen Exclusive mode is never triggered.
            SetWindowPos(hwnd, HWND_TOP, x, y, w, h,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);

            qDebug() << "WorkerW injection: Embedded wallpaper window successfully"
                     << x << y << w << h;

        } else {
            // Fallback: old HWND_BOTTOM approach if WorkerW injection fails.
            qWarning() << "WorkerW injection failed, falling back to HWND_BOTTOM mode";
            fallbackSetToWallpaper(hwnd, geometry);
        }
    }

    void setIgnoreInput(QWindow* window) override {
        HWND hwnd = (HWND)window->winId();
        if (m_workerW) {
            // When parented to WorkerW, input already goes to desktop icons.
            // Just make sure we stay behind them.
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else {
            // Fallback mode
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    void restoreDesktop() override {
        if (m_injectedHwnd && m_workerW) {
            // Un-parent from WorkerW
            SetParent(m_injectedHwnd, m_originalParent);

            // Restore original styles
            if (m_originalStyle) {
                SetWindowLongPtrW(m_injectedHwnd, GWL_STYLE, m_originalStyle);
            }
            if (m_originalExStyle) {
                SetWindowLongPtrW(m_injectedHwnd, GWL_EXSTYLE, m_originalExStyle);
            }

            m_injectedHwnd = nullptr;
            m_workerW = nullptr;
            m_originalParent = nullptr;
            m_originalStyle = 0;
            m_originalExStyle = 0;
        }
    }

private:
    void fallbackSetToWallpaper(HWND hwnd, const QRect& geometry) {
        // Old approach: keep at HWND_BOTTOM without WorkerW injection
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= WS_POPUP | WS_VISIBLE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_APPWINDOW);
        exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        int x, y, w, h;
        if (geometry.isValid()) {
            x = geometry.x();
            y = geometry.y();
            w = geometry.width();
            h = geometry.height() - 1;
        } else {
            x = GetSystemMetrics(SM_XVIRTUALSCREEN);
            y = GetSystemMetrics(SM_YVIRTUALSCREEN);
            w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            h = GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1;
        }

        SetWindowPos(hwnd, HWND_BOTTOM, x, y, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }

    HWND m_injectedHwnd = nullptr;
    HWND m_workerW = nullptr;
    HWND m_originalParent = nullptr;
    LONG_PTR m_originalStyle = 0;
    LONG_PTR m_originalExStyle = 0;
};

PlatformUtils* PlatformUtils::create() {
    return new WindowsUtils();
}
#endif
