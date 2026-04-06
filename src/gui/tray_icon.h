#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QWidgetAction>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>

class TrayIcon : public QSystemTrayIcon {
    Q_OBJECT
public:
    explicit TrayIcon(QObject* parent = nullptr) : QSystemTrayIcon(parent) {
        m_menu = new QMenu();

        // Enable / Disable wallpaper toggle (top of menu)
        m_enableAction = m_menu->addAction("Disable Wallpaper");
        m_enableAction->setCheckable(true);
        m_enableAction->setChecked(true); // starts enabled
        connect(m_enableAction, &QAction::toggled, this, [this](bool checked) {
            updateEnableLabel(checked);
            emit enableToggled(checked);
        });

        m_menu->addSeparator();

        // Wallpaper selection
        QAction* chooseMediaAction = m_menu->addAction("Choose Media...");
        connect(chooseMediaAction, &QAction::triggered, this, &TrayIcon::mediaSelectionRequested);

        QAction* galleryAction = m_menu->addAction("Gallery...");
        connect(galleryAction, &QAction::triggered, this, &TrayIcon::galleryRequested);

        m_menu->addSeparator();

        // Playback controls
        QAction* prevAction = m_menu->addAction("\u23EE Previous");
        connect(prevAction, &QAction::triggered, this, &TrayIcon::previousRequested);

        QAction* nextAction = m_menu->addAction("Next \u23ED");
        connect(nextAction, &QAction::triggered, this, &TrayIcon::nextRequested);

        m_pauseAction = m_menu->addAction("Pause");
        m_pauseAction->setCheckable(true);
        connect(m_pauseAction, &QAction::toggled, this, [this](bool paused) {
            updatePauseLabel(paused);
            emit pauseToggled(paused);
        });

        m_menu->addSeparator();

        // Audio
        m_muteAction = m_menu->addAction("Mute Audio");
        m_muteAction->setCheckable(true);
        connect(m_muteAction, &QAction::toggled, this, &TrayIcon::muteToggled);

        // Volume slider in menu
        auto* volumeWidget = new QWidget();
        auto* volLayout = new QHBoxLayout(volumeWidget);
        volLayout->setContentsMargins(12, 4, 12, 4);
        auto* volLabel = new QLabel("\xF0\x9F\x94\x8A"); // 🔊
        m_volumeSlider = new QSlider(Qt::Horizontal);
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(100);
        m_volumeSlider->setFixedWidth(120);
        m_volumeValueLabel = new QLabel("100%");
        volLayout->addWidget(volLabel);
        volLayout->addWidget(m_volumeSlider);
        volLayout->addWidget(m_volumeValueLabel);

        connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
            m_volumeValueLabel->setText(QString("%1%").arg(value));
            emit volumeChanged(value / 100.0f);
        });

        auto* volumeAction = new QWidgetAction(m_menu);
        volumeAction->setDefaultWidget(volumeWidget);
        m_menu->addAction(volumeAction);

        m_menu->addSeparator();

        // Settings & Quit
        QAction* settingsAction = m_menu->addAction("Settings...");
        connect(settingsAction, &QAction::triggered, this, &TrayIcon::settingsRequested);

        m_menu->addSeparator();

        QAction* quitAction = m_menu->addAction("Quit");
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

        setContextMenu(m_menu);
        setToolTip("WPE-Alt");

        QIcon icon = QIcon::fromTheme("video-display");
        if (icon.isNull()) {
            icon = qApp->style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        setIcon(icon);
    }

    ~TrayIcon() override {
        delete m_menu;
    }

    void setPausedState(bool paused) {
        if (!m_pauseAction) return;
        if (m_pauseAction->isChecked() != paused) {
            m_pauseAction->setChecked(paused);
            return;
        }
        updatePauseLabel(paused);
    }

    void setMutedState(bool muted) {
        if (m_muteAction && m_muteAction->isChecked() != muted) {
            m_muteAction->setChecked(muted);
        }
    }

    void setVolumeState(int percent) {
        if (m_volumeSlider) {
            m_volumeSlider->blockSignals(true);
            m_volumeSlider->setValue(percent);
            m_volumeSlider->blockSignals(false);
            m_volumeValueLabel->setText(QString("%1%").arg(percent));
        }
    }

    void setEnabledState(bool enabled) {
        if (m_enableAction && m_enableAction->isChecked() != enabled) {
            m_enableAction->setChecked(enabled);
        }
    }

signals:
    void enableToggled(bool enabled);
    void mediaSelectionRequested();
    void galleryRequested();
    void settingsRequested();
    void pauseToggled(bool paused);
    void muteToggled(bool muted);
    void volumeChanged(float volume);
    void nextRequested();
    void previousRequested();

private:
    void updateEnableLabel(bool enabled) {
        if (m_enableAction) {
            m_enableAction->setText(enabled ? "Disable Wallpaper" : "Enable Wallpaper");
        }
    }

    void updatePauseLabel(bool paused) {
        if (m_pauseAction) {
            m_pauseAction->setText(paused ? "Resume" : "Pause");
        }
    }

    QMenu* m_menu;
    QAction* m_enableAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_muteAction = nullptr;
    QSlider* m_volumeSlider = nullptr;
    QLabel* m_volumeValueLabel = nullptr;
};
