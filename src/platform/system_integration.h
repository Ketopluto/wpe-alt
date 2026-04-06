#pragma once

#include <QObject>
#include <QTimer>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class SystemIntegration : public QObject {
    Q_OBJECT
public:
    explicit SystemIntegration(QObject* parent = nullptr) : QObject(parent) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setTimerType(Qt::CoarseTimer);
        m_pollTimer->setInterval(2000);
        connect(m_pollTimer, &QTimer::timeout, this, &SystemIntegration::pollSystemState);
    }

    ~SystemIntegration() override {
        m_pollTimer->stop();
    }

    void startMonitoring() {
        m_pollTimer->start();
    }

    void stopMonitoring() {
        m_pollTimer->stop();
    }

    // --- Auto-start ---
    bool isAutoStartEnabled() const {
#ifdef Q_OS_WIN
        HKEY key = nullptr;
        LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_READ, &key);
        if (result != ERROR_SUCCESS) return false;

        DWORD type = 0;
        result = RegQueryValueExW(key, L"WPE-Alt", nullptr, &type, nullptr, nullptr);
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
#else
        return false;
#endif
    }

    void setAutoStart(bool enable) {
#ifdef Q_OS_WIN
        HKEY key = nullptr;
        LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_WRITE, &key);
        if (result != ERROR_SUCCESS) return;

        if (enable) {
            wchar_t path[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            QString exePath = QString::fromWCharArray(path);
            std::wstring value = exePath.toStdWString();
            RegSetValueExW(key, L"WPE-Alt", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(value.c_str()),
                static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(key, L"WPE-Alt");
        }
        RegCloseKey(key);
#else
        Q_UNUSED(enable);
#endif
    }

    // --- Single instance ---
    static bool tryAcquireSingleInstance() {
#ifdef Q_OS_WIN
        HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\WPE-Alt-SingleInstance");
        if (mutex == nullptr) return false;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(mutex);
            return false;
        }
        // Intentionally leak mutex handle — held for process lifetime
        return true;
#else
        return true;
#endif
    }

    // --- Feature toggles ---
    bool pauseOnFullscreen() const { return m_pauseOnFullscreen; }
    void setPauseOnFullscreen(bool v) { m_pauseOnFullscreen = v; }

    bool pauseOnBattery() const { return m_pauseOnBattery; }
    void setPauseOnBattery(bool v) { m_pauseOnBattery = v; }

    bool isPausedBySystem() const { return m_systemPaused; }

signals:
    void systemPauseChanged(bool paused);

private slots:
    void pollSystemState() {
        bool shouldPause = false;

        if (m_pauseOnFullscreen) {
            shouldPause = shouldPause || isFullscreenAppRunning();
        }
        if (m_pauseOnBattery) {
            shouldPause = shouldPause || isOnBattery();
        }

        if (shouldPause != m_systemPaused) {
            m_systemPaused = shouldPause;
            emit systemPauseChanged(m_systemPaused);
        }
    }

private:
    bool isFullscreenAppRunning() const {
#ifdef Q_OS_WIN
        HWND fg = GetForegroundWindow();
        if (!fg) return false;

        // Ignore desktop / shell windows
        HWND desktop = GetDesktopWindow();
        HWND shell = GetShellWindow();
        if (fg == desktop || fg == shell) return false;

        // Check if foreground window covers the entire monitor
        HMONITOR monitor = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {};
        mi.cbSize = sizeof(MONITORINFO);
        if (!GetMonitorInfoW(monitor, &mi)) return false;

        RECT wr = {};
        GetWindowRect(fg, &wr);

        return (wr.left <= mi.rcMonitor.left &&
                wr.top <= mi.rcMonitor.top &&
                wr.right >= mi.rcMonitor.right &&
                wr.bottom >= mi.rcMonitor.bottom);
#else
        return false;
#endif
    }

    bool isOnBattery() const {
#ifdef Q_OS_WIN
        SYSTEM_POWER_STATUS sps = {};
        if (!GetSystemPowerStatus(&sps)) return false;
        // ACLineStatus: 0 = offline (battery), 1 = online (plugged in)
        return sps.ACLineStatus == 0;
#else
        return false;
#endif
    }

    QTimer* m_pollTimer = nullptr;
    bool m_pauseOnFullscreen = true;
    bool m_pauseOnBattery = false;
    bool m_systemPaused = false;
};
