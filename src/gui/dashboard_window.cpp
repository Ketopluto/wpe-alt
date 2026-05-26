#include "dashboard_window.h"
#include <QApplication>
#include <QStyle>
#include <QGroupBox>
#include <QScrollArea>
#include <QPainter>
#include <QLinearGradient>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QFileInfo>

DashboardWindow::DashboardWindow(QSettings* settings, QWidget* parent)
    : QMainWindow(parent), m_settings(settings) {
    setWindowTitle("Wallpaper Studio");
    setMinimumSize(800, 500);

    setupUi();
    generatePrebuiltWallpapers();
    loadSettings();
    loadGalleryItems();
    applyTheme();
}

void DashboardWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(200);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    
    QListWidgetItem* itemGallery = new QListWidgetItem(" Gallery");
    QListWidgetItem* itemDisplays = new QListWidgetItem(" Displays");
    QListWidgetItem* itemPlayback = new QListWidgetItem(" Playback");
    QListWidgetItem* itemSettings = new QListWidgetItem(" Settings");
    QListWidgetItem* itemAbout = new QListWidgetItem(" About");

    m_sidebar->addItem(itemGallery);
    m_sidebar->addItem(itemDisplays);
    m_sidebar->addItem(itemPlayback);
    m_sidebar->addItem(itemSettings);
    m_sidebar->addItem(itemAbout);

    // Stack
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createGalleryPage());
    m_stack->addWidget(createDisplaysPage());
    m_stack->addWidget(createPlaybackPage());
    m_stack->addWidget(createSettingsPage());
    m_stack->addWidget(createAboutPage());

    connect(m_sidebar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_sidebar->setCurrentRow(1); // Default to Displays for now

    mainLayout->addWidget(m_sidebar);
    mainLayout->addWidget(m_stack, 1);

    // Bottom Action Bar
    auto* bottomBar = new QWidget(this);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->addStretch();
    auto* applyBtn = new QPushButton("Apply Settings", this);
    applyBtn->setMinimumSize(120, 36);
    bottomLayout->addWidget(applyBtn);

    auto* wrapperLayout = new QVBoxLayout();
    wrapperLayout->setContentsMargins(0,0,0,0);
    wrapperLayout->addWidget(centralWidget, 1);
    wrapperLayout->addWidget(bottomBar);
    
    auto* rootWidget = new QWidget(this);
    rootWidget->setLayout(wrapperLayout);
    setCentralWidget(rootWidget);

    connect(applyBtn, &QPushButton::clicked, this, &DashboardWindow::applySettings);
}

QWidget* DashboardWindow::createGalleryPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    
    auto* titleRow = new QHBoxLayout();
    auto* title = new QLabel("Wallpaper Gallery", page);
    title->setProperty("heading", true);
    titleRow->addWidget(title);
    titleRow->addStretch();
    
    auto* browseBtn = new QPushButton("Browse...", page);
    connect(browseBtn, &QPushButton::clicked, this, &DashboardWindow::onBrowseWallpaper);
    titleRow->addWidget(browseBtn);
    layout->addLayout(titleRow);

    m_galleryList = new QListWidget(page);
    m_galleryList->setViewMode(QListView::IconMode);
    m_galleryList->setResizeMode(QListView::Adjust);
    m_galleryList->setSpacing(15);
    m_galleryList->setIconSize(QSize(240, 135));
    m_galleryList->setMovement(QListView::Static);
    
    connect(m_galleryList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString path = item->data(Qt::UserRole).toString();
        emit wallpaperSelected(path);
    });

    layout->addWidget(m_galleryList, 1);
    return page;
}

QWidget* DashboardWindow::createDisplaysPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    auto* title = new QLabel("Displays & Engine", page);
    title->setProperty("heading", true);
    layout->addWidget(title);

    auto* group = new QWidget(page);
    group->setProperty("card", true);
    auto* form = new QFormLayout(group);
    form->setSpacing(15);

    m_fpsSpinBox = new QSpinBox(group);
    m_fpsSpinBox->setRange(5, 120);
    m_fpsSpinBox->setSuffix(" FPS");
    form->addRow("Target FPS:", m_fpsSpinBox);

    m_fillModeCombo = new QComboBox(group);
    m_fillModeCombo->addItem("Fill (crop to fit)", "fill");
    m_fillModeCombo->addItem("Fit (letterbox)", "fit");
    m_fillModeCombo->addItem("Stretch", "stretch");
    m_fillModeCombo->addItem("Center (no scaling)", "center");
    form->addRow("Fill Mode:", m_fillModeCombo);

    m_multiMonitorCombo = new QComboBox(group);
    m_multiMonitorCombo->addItem("Span across all monitors", "span");
    m_multiMonitorCombo->addItem("Duplicate on each monitor", "per_monitor");
    form->addRow("Multi-Monitor Layout:", m_multiMonitorCombo);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* DashboardWindow::createPlaybackPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    auto* title = new QLabel("Playback & Audio", page);
    title->setProperty("heading", true);
    layout->addWidget(title);

    auto* plGroup = new QWidget(page);
    plGroup->setProperty("card", true);
    auto* plForm = new QFormLayout(plGroup);
    plForm->setSpacing(15);

    m_playlistModeCombo = new QComboBox(plGroup);
    m_playlistModeCombo->addItem("Sequential", "sequential");
    m_playlistModeCombo->addItem("Shuffle", "shuffle");
    m_playlistModeCombo->addItem("Single (loop one)", "single");
    plForm->addRow("Playlist Mode:", m_playlistModeCombo);

    m_rotationSpinBox = new QSpinBox(plGroup);
    m_rotationSpinBox->setRange(0, 86400);
    m_rotationSpinBox->setSuffix(" sec");
    m_rotationSpinBox->setSpecialValueText("Off");
    plForm->addRow("Auto-rotate every:", m_rotationSpinBox);
    layout->addWidget(plGroup);

    auto* audioGroup = new QWidget(page);
    audioGroup->setProperty("card", true);
    auto* audioForm = new QFormLayout(audioGroup);
    audioForm->setSpacing(15);

    m_volumeSlider = new QSlider(Qt::Horizontal, audioGroup);
    m_volumeSlider->setRange(0, 100);
    m_volumeLabel = new QLabel("100%", audioGroup);
    connect(m_volumeSlider, &QSlider::valueChanged, [this](int v) {
        m_volumeLabel->setText(QString("%1%").arg(v));
    });

    auto* volRow = new QHBoxLayout();
    volRow->addWidget(m_volumeSlider);
    volRow->addWidget(m_volumeLabel);
    audioForm->addRow("Master Volume:", volRow);

    m_muteCheck = new QCheckBox("Mute audio entirely", audioGroup);
    audioForm->addRow("", m_muteCheck);
    layout->addWidget(audioGroup);

    layout->addStretch();
    return page;
}

QWidget* DashboardWindow::createSettingsPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    auto* title = new QLabel("System Settings", page);
    title->setProperty("heading", true);
    layout->addWidget(title);

    auto* group = new QWidget(page);
    group->setProperty("card", true);
    auto* form = new QVBoxLayout(group);
    form->setSpacing(15);

    m_autoStartCheck = new QCheckBox("Start with Windows", group);
    m_pauseFullscreenCheck = new QCheckBox("Pause when a fullscreen app is running", group);
    m_pauseBatteryCheck = new QCheckBox("Pause when on battery power", group);

    form->addWidget(m_autoStartCheck);
    form->addWidget(m_pauseFullscreenCheck);
    form->addWidget(m_pauseBatteryCheck);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* DashboardWindow::createAboutPage() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("Wallpaper Studio", page);
    title->setProperty("heading", true);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* subtitle = new QLabel("Open-source Wallpaper Engine alternative\nVersion 2.0.0", page);
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);

    layout->addSpacing(20);

    auto* stack = new QLabel("Built with Qt 6 + FFmpeg\nLicensed under MIT", page);
    stack->setAlignment(Qt::AlignCenter);
    layout->addWidget(stack);

    return page;
}

void DashboardWindow::loadSettings() {
    m_fpsSpinBox->setValue(m_settings->value("engine/fps", 30).toInt());

    QString fillMode = m_settings->value("engine/fill_mode", "fill").toString().toLower();
    int fillIdx = m_fillModeCombo->findData(fillMode);
    m_fillModeCombo->setCurrentIndex(fillIdx >= 0 ? fillIdx : 0);

    QString mmMode = m_settings->value("engine/multi_monitor_mode", "span").toString().toLower();
    int mmIdx = m_multiMonitorCombo->findData(mmMode);
    m_multiMonitorCombo->setCurrentIndex(mmIdx >= 0 ? mmIdx : 0);

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

void DashboardWindow::applySettings() {
    m_settings->setValue("engine/fps", m_fpsSpinBox->value());
    m_settings->setValue("engine/fill_mode", m_fillModeCombo->currentData().toString());
    m_settings->setValue("engine/multi_monitor_mode", m_multiMonitorCombo->currentData().toString());
    m_settings->setValue("engine/auto_start", m_autoStartCheck->isChecked());

    m_settings->setValue("playlist/mode", m_playlistModeCombo->currentData().toString());
    m_settings->setValue("playlist/interval_sec", m_rotationSpinBox->value());

    m_settings->setValue("audio/volume", m_volumeSlider->value());
    m_settings->setValue("audio/mute", m_muteCheck->isChecked());

    m_settings->setValue("behavior/pause_fullscreen", m_pauseFullscreenCheck->isChecked());
    m_settings->setValue("behavior/pause_battery", m_pauseBatteryCheck->isChecked());

    m_settings->sync();
    emit settingsChanged();
}

void DashboardWindow::onBrowseWallpaper() {
    const QString filter =
        "Media Files (*.mp4 *.mkv *.webm *.avi *.mov *.wmv *.flv *.m4v *.ts *.mpeg *.mpg "
        "*.jpg *.jpeg *.png *.bmp *.gif *.webp *.apng);;"
        "All Files (*.*)";

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString selected = QFileDialog::getOpenFileName(this, "Choose Wallpaper Media", defaultDir, filter);

    if (selected.isEmpty()) return;
    
    QString absolute = QFileInfo(selected).absoluteFilePath();
    emit wallpaperSelected(absolute);
}

void DashboardWindow::generatePrebuiltWallpapers() {
    QString wpDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wallpapers";
    QDir dir(wpDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    auto generateGradient = [&](const QString& name, QColor c1, QColor c2) {
        QString path = dir.filePath(name);
        if (QFile::exists(path)) return;

        QImage img(1920, 1080, QImage::Format_RGB32);
        QPainter p(&img);
        QLinearGradient grad(0, 0, 1920, 1080);
        grad.setColorAt(0, c1);
        grad.setColorAt(1, c2);
        p.fillRect(img.rect(), grad);
        img.save(path, "JPEG", 90);
    };

    generateGradient("Neon Synthwave.jpg", QColor("#2a0845"), QColor("#6441A5"));
    generateGradient("Deep Ocean.jpg", QColor("#0f2027"), QColor("#203a43"));
    generateGradient("Forest Canopy.jpg", QColor("#134E5E"), QColor("#71B280"));
}

void DashboardWindow::loadGalleryItems() {
    m_galleryList->clear();
    QString wpDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wallpapers";
    QDir dir(wpDir);
    if (!dir.exists()) return;

    QStringList filters = {"*.jpg", "*.png", "*.jpeg", "*.mp4", "*.gif"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fi : files) {
        QIcon icon;
        if (fi.suffix().toLower() == "mp4") {
            // For videos, just use a generic icon for now
            icon = QIcon::fromTheme("video-x-generic");
        } else {
            QImage img;
            img.load(fi.absoluteFilePath());
            icon = QIcon(QPixmap::fromImage(img.scaled(240, 135, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)));
        }

        auto* item = new QListWidgetItem(icon, fi.baseName());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        m_galleryList->addItem(item);
    }
}

void DashboardWindow::applyTheme() {
    QString qss = R"(
        QMainWindow {
            background-color: #1e1e1e;
        }
        QWidget {
            color: #d4d4d4;
            font-family: "Segoe UI", sans-serif;
            font-size: 13px;
        }
        QListWidget {
            background-color: #252526;
            border: none;
            border-right: 1px solid #333333;
            outline: none;
            padding-top: 20px;
        }
        QListWidget::item {
            height: 45px;
            padding-left: 20px;
            color: #cccccc;
        }
        QListWidget::item:selected {
            background-color: #37373d;
            color: #ffffff;
            border-left: 3px solid #007acc;
        }
        QListWidget::item:hover:!selected {
            background-color: #2a2d2e;
        }
        QLabel[heading="true"] {
            font-size: 24px;
            font-weight: bold;
            color: #ffffff;
            margin-bottom: 10px;
        }
        QWidget[card="true"] {
            background-color: #252526;
            border-radius: 8px;
            border: 1px solid #333333;
            padding: 20px;
        }
        QPushButton {
            background-color: #007acc;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #0098ff;
        }
        QPushButton:pressed {
            background-color: #005a9e;
        }
        QComboBox, QSpinBox {
            background-color: #3c3c3c;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 6px;
            color: white;
            min-width: 150px;
        }
        QComboBox:hover, QSpinBox:hover {
            border: 1px solid #007acc;
        }
        QCheckBox {
            spacing: 10px;
            font-size: 14px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #555555;
            background-color: #3c3c3c;
        }
        QCheckBox::indicator:checked {
            background-color: #007acc;
            border: 1px solid #007acc;
            image: url(); /* In a real app we put a checkmark SVG here */
        }
        QSlider::groove:horizontal {
            border: 1px solid #555555;
            height: 6px;
            background: #3c3c3c;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #007acc;
            border: none;
            width: 16px;
            height: 16px;
            margin: -5px 0;
            border-radius: 8px;
        }
    )";
    setStyleSheet(qss);
}
