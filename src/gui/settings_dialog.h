#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSettings>
#include <QFont>
#include <QApplication>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QSettings* settings, QWidget* parent = nullptr)
        : QDialog(parent), m_settings(settings) {
        setWindowTitle("WPE-Alt Settings");
        setMinimumSize(480, 420);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto* mainLayout = new QVBoxLayout(this);
        auto* tabs = new QTabWidget(this);

        tabs->addTab(createGeneralTab(), "General");
        tabs->addTab(createPlaybackTab(), "Playback");
        tabs->addTab(createPerformanceTab(), "Performance");
        tabs->addTab(createAboutTab(), "About");

        mainLayout->addWidget(tabs);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::applyAndAccept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        mainLayout->addWidget(buttons);

        loadSettings();
    }

signals:
    void settingsChanged();

private:
    QWidget* createGeneralTab() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);

        auto* engineGroup = new QGroupBox("Engine", widget);
        auto* engineLayout = new QFormLayout(engineGroup);

        m_fpsSpinBox = new QSpinBox(engineGroup);
        m_fpsSpinBox->setRange(5, 120);
        m_fpsSpinBox->setSuffix(" FPS");
        engineLayout->addRow("Target FPS:", m_fpsSpinBox);

        m_fillModeCombo = new QComboBox(engineGroup);
        m_fillModeCombo->addItem("Fill (crop to fit)", "fill");
        m_fillModeCombo->addItem("Fit (letterbox)", "fit");
        m_fillModeCombo->addItem("Stretch", "stretch");
        m_fillModeCombo->addItem("Center (no scaling)", "center");
        engineLayout->addRow("Fill Mode:", m_fillModeCombo);

        layout->addWidget(engineGroup);

        auto* startupGroup = new QGroupBox("Startup", widget);
        auto* startupLayout = new QVBoxLayout(startupGroup);

        m_autoStartCheck = new QCheckBox("Start with Windows", startupGroup);
        startupLayout->addWidget(m_autoStartCheck);

        layout->addWidget(startupGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createPlaybackTab() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);

        auto* playlistGroup = new QGroupBox("Playlist", widget);
        auto* playlistLayout = new QFormLayout(playlistGroup);

        m_playlistModeCombo = new QComboBox(playlistGroup);
        m_playlistModeCombo->addItem("Sequential", "sequential");
        m_playlistModeCombo->addItem("Shuffle", "shuffle");
        m_playlistModeCombo->addItem("Single (loop one)", "single");
        playlistLayout->addRow("Mode:", m_playlistModeCombo);

        m_rotationSpinBox = new QSpinBox(playlistGroup);
        m_rotationSpinBox->setRange(0, 86400);
        m_rotationSpinBox->setSuffix(" sec");
        m_rotationSpinBox->setSpecialValueText("Off");
        playlistLayout->addRow("Auto-rotate every:", m_rotationSpinBox);

        layout->addWidget(playlistGroup);

        auto* audioGroup = new QGroupBox("Audio", widget);
        auto* audioLayout = new QFormLayout(audioGroup);

        m_volumeSlider = new QSlider(Qt::Horizontal, audioGroup);
        m_volumeSlider->setRange(0, 100);
        m_volumeLabel = new QLabel("100%", audioGroup);
        connect(m_volumeSlider, &QSlider::valueChanged, [this](int v) {
            m_volumeLabel->setText(QString("%1%").arg(v));
        });

        auto* volRow = new QHBoxLayout();
        volRow->addWidget(m_volumeSlider);
        volRow->addWidget(m_volumeLabel);
        audioLayout->addRow("Volume:", volRow);

        m_muteCheck = new QCheckBox("Mute audio", audioGroup);
        audioLayout->addRow("", m_muteCheck);

        layout->addWidget(audioGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createPerformanceTab() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);

        auto* behaviorGroup = new QGroupBox("Power Saving", widget);
        auto* behaviorLayout = new QVBoxLayout(behaviorGroup);

        m_pauseFullscreenCheck = new QCheckBox("Pause when a fullscreen app is running", behaviorGroup);
        behaviorLayout->addWidget(m_pauseFullscreenCheck);

        m_pauseBatteryCheck = new QCheckBox("Pause when on battery power", behaviorGroup);
        behaviorLayout->addWidget(m_pauseBatteryCheck);

        layout->addWidget(behaviorGroup);
        layout->addStretch();

        return widget;
    }

    QWidget* createAboutTab() {
        auto* widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setAlignment(Qt::AlignCenter);

        auto* title = new QLabel("WPE-Alt", widget);
        QFont titleFont = title->font();
        titleFont.setPointSize(20);
        titleFont.setBold(true);
        title->setFont(titleFont);
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);

        auto* subtitle = new QLabel("Open-source Wallpaper Engine alternative", widget);
        subtitle->setAlignment(Qt::AlignCenter);
        layout->addWidget(subtitle);

        layout->addSpacing(16);

        auto* version = new QLabel("Version 1.0.0", widget);
        version->setAlignment(Qt::AlignCenter);
        layout->addWidget(version);

        auto* stack = new QLabel("Built with Qt 6 + FFmpeg", widget);
        stack->setAlignment(Qt::AlignCenter);
        layout->addWidget(stack);

        layout->addSpacing(16);

        auto* license = new QLabel("Licensed under MIT", widget);
        license->setAlignment(Qt::AlignCenter);
        layout->addWidget(license);

        layout->addStretch();
        return widget;
    }

    void loadSettings() {
        m_fpsSpinBox->setValue(m_settings->value("engine/fps", 30).toInt());

        QString fillMode = m_settings->value("engine/fill_mode", "fill").toString().toLower();
        int fillIdx = m_fillModeCombo->findData(fillMode);
        m_fillModeCombo->setCurrentIndex(fillIdx >= 0 ? fillIdx : 0);

        m_autoStartCheck->setChecked(m_settings->value("engine/auto_start", false).toBool());

        QString plMode = m_settings->value("playlist/mode", "sequential").toString().toLower();
        int plIdx = m_playlistModeCombo->findData(plMode);
        m_playlistModeCombo->setCurrentIndex(plIdx >= 0 ? plIdx : 0);

        m_rotationSpinBox->setValue(m_settings->value("playlist/interval_sec", 0).toInt());

        m_volumeSlider->setValue(m_settings->value("audio/volume", 100).toInt());
        m_muteCheck->setChecked(m_settings->value("audio/mute", false).toBool());

        m_pauseFullscreenCheck->setChecked(m_settings->value("behavior/pause_fullscreen", true).toBool());
        m_pauseBatteryCheck->setChecked(m_settings->value("behavior/pause_battery", false).toBool());
    }

    void applyAndAccept() {
        m_settings->setValue("engine/fps", m_fpsSpinBox->value());
        m_settings->setValue("engine/fill_mode", m_fillModeCombo->currentData().toString());
        m_settings->setValue("engine/auto_start", m_autoStartCheck->isChecked());

        m_settings->setValue("playlist/mode", m_playlistModeCombo->currentData().toString());
        m_settings->setValue("playlist/interval_sec", m_rotationSpinBox->value());

        m_settings->setValue("audio/volume", m_volumeSlider->value());
        m_settings->setValue("audio/mute", m_muteCheck->isChecked());

        m_settings->setValue("behavior/pause_fullscreen", m_pauseFullscreenCheck->isChecked());
        m_settings->setValue("behavior/pause_battery", m_pauseBatteryCheck->isChecked());

        m_settings->sync();
        emit settingsChanged();
        accept();
    }

    QSettings* m_settings;

    // General
    QSpinBox* m_fpsSpinBox = nullptr;
    QComboBox* m_fillModeCombo = nullptr;
    QCheckBox* m_autoStartCheck = nullptr;

    // Playback
    QComboBox* m_playlistModeCombo = nullptr;
    QSpinBox* m_rotationSpinBox = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_volumeLabel = nullptr;
    QCheckBox* m_muteCheck = nullptr;

    // Performance
    QCheckBox* m_pauseFullscreenCheck = nullptr;
    QCheckBox* m_pauseBatteryCheck = nullptr;
};
