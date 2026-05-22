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

        // Find desktop icon list view and make its background transparent
        HWND progman = FindWindowW(L"Progman", nullptr);
        if (progman) {
            HWND defView = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
            if (!defView) {
                // DefView might be in a top-level WorkerW on some setups
                for (HWND w = FindWindowExW(nullptr, nullptr, L"WorkerW", nullptr);
                     w; w = FindWindowExW(nullptr, w, L"WorkerW", nullptr)) {
                    defView = FindWindowExW(w, nullptr, L"SHELLDLL_DefView", nullptr);
                    if (defView) break;
                }
            }
            if (defView) {
                HWND listView = FindWindowExW(defView, nullptr, L"SysListView32", nullptr);
                if (listView) {
                    SendMessageW(listView, LVM_SETBKCOLOR, 0, (LPARAM)CLR_NONE);
                    SendMessageW(listView, LVM_SETTEXTBKCOLOR, 0, (LPARAM)CLR_NONE);
                    InvalidateRect(listView, nullptr, TRUE);
                    m_listView = listView;
                }
            }
        }

        // Keep as top-level window — do NOT parent to any desktop window.
        // Remove window decorations.
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= WS_POPUP | WS_VISIBLE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_APPWINDOW);
        exStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

        // Full screen size
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);

        // Place at BOTTOM of all windows — behind desktop icons,
        // on top of the now-black system wallpaper.
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, w, h,
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

        // Restore list view background
        if (m_listView && IsWindow(m_listView)) {
            SendMessageW(m_listView, LVM_SETBKCOLOR, 0, (LPARAM)GetSysColor(COLOR_DESKTOP));
            SendMessageW(m_listView, LVM_SETTEXTBKCOLOR, 0, (LPARAM)GetSysColor(COLOR_DESKTOP));
            InvalidateRect(m_listView, nullptr, TRUE);
        }
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
