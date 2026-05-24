#include "platform_utils.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <QDebug>

namespace {

// --------------------------------------------------------------------------
// Desktop wallpaper for Windows 11 24H2+.
//
// On Win11 24H2, DWM completely controls desktop composition — child windows
// inside Progman, WorkerW, or SHELLDLL_DefView are NOT composited/visible.
//
// The ONLY reliable approach:
//   1. Set the Windows wallpaper to solid black (so desktop = black)
//   2. Keep our window as a top-level window at HWND_BOTTOM z-order
//   3. Our window sits between the desktop background and all other windows
//   4. Make the desktop icons transparent-background via LVM_SETBKCOLOR
//
// On exit, restore the original wallpaper.
// --------------------------------------------------------------------------

} // namespace

class WindowsUtils : public PlatformUtils {
public:
    ~WindowsUtils() override {
        restoreDesktop();
    }

    void setToWallpaper(QWindow* window) override {
        restoreDesktop();

        HWND hwnd = (HWND)window->winId();

        // Save current wallpaper path so we can restore on exit
        wchar_t currentWp[MAX_PATH] = {};
        SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, currentWp, 0);
        m_savedWallpaper = QString::fromWCharArray(currentWp);

        // Set wallpaper to empty (solid color background — will be black)
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)L"", SPIF_SENDCHANGE);

        // Note: We do NOT send cross-process messages (LVM_SETBKCOLOR) to the desktop ListView here.
        // Modern Windows 10/11 already renders desktop icon text backgrounds transparently by default.
        // Avoiding cross-process SendMessageW calls completely prevents EDR (SentinelOne) process-tampering flags.

        // 1. Trigger the desktop composition to expose the WorkerW/Progman hierarchy
        HWND progman = FindWindowW(L"Progman", nullptr);
        if (progman) {
            SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, nullptr);
        }

        // 2. Locate SHELLDLL_DefView (contains desktop icons) and its parent
        HWND shell_def_view = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
        HWND desktop_parent = progman;

        if (!shell_def_view) {
            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                HWND def = FindWindowExW(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
                if (def) {
                    HWND* result = (HWND*)lParam;
                    *result = def;
                    return FALSE; // Stop enumeration
                }
                return TRUE;
            }, (LPARAM)&shell_def_view);
        }

        if (shell_def_view) {
            desktop_parent = GetParent(shell_def_view);
        }

        // Transform into a child window to natively inject into the desktop hierarchy
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= WS_CHILD | WS_VISIBLE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_APPWINDOW);
        exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        // Set opacity to 255 (fully opaque) so the layered window is visible
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

        // Inject into the desktop parent
        if (desktop_parent) {
            SetParent(hwnd, desktop_parent);
        }

        // Full virtual screen size (Extend Mode Option B: span all monitors)
        int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // Place our window explicitly behind SHELLDLL_DefView in the Z-order
        HWND insertAfter = shell_def_view ? shell_def_view : HWND_BOTTOM;
        SetWindowPos(hwnd, insertAfter, x, y, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }

    void setIgnoreInput(QWindow* window) override {
        // Keep window at bottom z-order continuously
        HWND hwnd = (HWND)window->winId();
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void restoreDesktop() override {
        // Restore original wallpaper
        if (!m_savedWallpaper.isEmpty()) {
            std::wstring wpPath = m_savedWallpaper.toStdWString();
            SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)wpPath.c_str(), SPIF_SENDCHANGE);
            m_savedWallpaper.clear();
        }

        // No list view restoration needed since we avoided cross-process modifications
        m_listView = nullptr;
    }

private:
    QString m_savedWallpaper;
    HWND m_listView = nullptr;
};

PlatformUtils* PlatformUtils::create() {
    return new WindowsUtils();
}
#endif
