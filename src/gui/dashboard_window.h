#pragma once

#include <QMainWindow>
#include <QSettings>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QPushButton>

class DashboardWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit DashboardWindow(QSettings* settings, QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void applySettings();

private:
    void setupUi();
    void loadSettings();
    void applyTheme();

    QWidget* createGalleryPage();
    QWidget* createDisplaysPage();
    QWidget* createPlaybackPage();
    QWidget* createSettingsPage();
    QWidget* createAboutPage();

    QSettings* m_settings;

    QListWidget* m_sidebar;
    QStackedWidget* m_stack;

    // UI Controls
    QSpinBox* m_fpsSpinBox;
    QComboBox* m_fillModeCombo;
    QComboBox* m_multiMonitorCombo;
    QCheckBox* m_autoStartCheck;

    QComboBox* m_playlistModeCombo;
    QSpinBox* m_rotationSpinBox;
    QSlider* m_volumeSlider;
    QLabel* m_volumeLabel;
    QCheckBox* m_muteCheck;

    QCheckBox* m_pauseFullscreenCheck;
    QCheckBox* m_pauseBatteryCheck;
};
