#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStringList>
#include <QRegularExpression>
#include "core/wallpaper_engine.h"
#include "core/playlist.h"
#include "renderers/color_renderer.h"
#include "renderers/image_renderer.h"
#include "renderers/video_renderer.h"
#include "renderers/gif_renderer.h"
#include "gui/tray_icon.h"
#include "gui/settings_dialog.h"
#include "gui/wallpaper_gallery.h"
#include "platform/system_integration.h"

namespace {

QString toAbsoluteMediaPath(const QString& input, const QDir& baseDir) {
    QString candidate = input.trimmed();
    if (candidate.isEmpty()) {
        return {};
    }

    QFileInfo info(candidate);
    if (info.isRelative()) {
        candidate = baseDir.filePath(candidate);
    }

    return QFileInfo(candidate).absoluteFilePath();
}

bool isImageFile(const QString& path) {
    QImageReader reader(path);
    return reader.canRead();
}

bool isGifFile(const QString& path) {
    return GifRenderer::canOpen(path);
}

FillMode parseFillMode(const QString& str) {
    QString lower = str.toLower().trimmed();
    if (lower == "stretch") return FillMode::Stretch;
    if (lower == "fit") return FillMode::Fit;
    if (lower == "center") return FillMode::Center;
    return FillMode::Fill;
}

WallpaperRenderer* createRendererForPath(const QString& path, FillMode fillMode, QObject* parent) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return new ColorRenderer(parent);
    }

    if (isGifFile(path)) {
        auto* r = new GifRenderer(path, parent);
        r->setFillMode(fillMode);
        return r;
    }

    if (isImageFile(path)) {
        auto* r = new ImageRenderer(path, parent);
        r->setFillMode(fillMode);
        return r;
    }

    if (VideoRenderer::canOpen(path)) {
        auto* r = new VideoRenderer(path, parent);
        r->setFillMode(fillMode);
        return r;
    }

    return new ColorRenderer(parent);
}

QStringList loadMediaCandidates(QSettings& settings, const QDir& baseDir) {
    QString itemsValue = settings.value("media/items").toString();
    if (itemsValue.trimmed().isEmpty()) {
        itemsValue = settings.value("image/path").toString();
    }

    QStringList candidates;
    const QStringList entries = itemsValue.split(QRegularExpression("[;|,]"), Qt::SkipEmptyParts);
    for (const QString& raw : entries) {
        const QString absolute = toAbsoluteMediaPath(raw, baseDir);
        if (!absolute.isEmpty()) {
            candidates << absolute;
        }
    }

    if (!candidates.isEmpty()) {
        return candidates;
    }

    const QStringList mediaFilters = {
        "*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov", "*.wmv", "*.flv", "*.m4v", "*.ts", "*.mpeg", "*.mpg",
        "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp", "*.gif", "*.apng"
    };
    const QFileInfoList files = baseDir.entryInfoList(mediaFilters, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        candidates << fi.absoluteFilePath();
    }

    return candidates;
}

} // namespace

int main(int argc, char *argv[])
{
    fprintf(stderr, "[DBG] main() entered\n"); fflush(stderr);

    QApplication app(argc, argv);
    fprintf(stderr, "[DBG] QApplication created\n"); fflush(stderr);

    app.setApplicationName("WPE-Alt");
    app.setApplicationVersion("1.0.0");
    app.setQuitOnLastWindowClosed(false);

    // Single instance check
    if (!SystemIntegration::tryAcquireSingleInstance()) {
        fprintf(stderr, "[DBG] Single instance check FAILED — exiting\n"); fflush(stderr);
        QMessageBox::information(nullptr, "WPE-Alt",
            "WPE-Alt is already running. Check the system tray.");
        return 0;
    }
    fprintf(stderr, "[DBG] Single instance OK\n"); fflush(stderr);
    fprintf(stderr, "[DBG] Parsing command line...\n"); fflush(stderr);

    // Command line
    QCommandLineParser parser;
    parser.setApplicationDescription("WPE-Alt — Open-source Wallpaper Engine alternative");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({{"c", "config"}, "Path to wallpaper.ini", "path"});
    parser.process(app);

    // Config
    QString iniPath = parser.value("config");
    if (iniPath.trimmed().isEmpty()) {
        iniPath = QDir(QCoreApplication::applicationDirPath()).filePath("wallpaper.ini");
    }
    iniPath = QFileInfo(iniPath).absoluteFilePath();
    QSettings settings(iniPath, QSettings::IniFormat);
    const QFileInfo iniInfo(iniPath);
    const QDir baseDir = iniInfo.absoluteDir();

    fprintf(stderr, "[DBG] Config loaded from: %s\n", qPrintable(iniPath)); fflush(stderr);

    // Engine settings
    FillMode fillMode = parseFillMode(settings.value("engine/fill_mode", "fill").toString());
    float volume = settings.value("audio/volume", 100).toInt() / 100.0f;
    bool muted = settings.value("audio/mute", false).toBool();

    // Playlist
    Playlist playlist;
    QStringList candidates = loadMediaCandidates(settings, baseDir);
    // Resolve relative paths
    QStringList resolved;
    for (const QString& c : candidates) {
        resolved << toAbsoluteMediaPath(c, baseDir);
    }
    playlist.setItems(resolved);

    QString plMode = settings.value("playlist/mode", "sequential").toString().toLower();
    if (plMode == "shuffle") playlist.setMode(Playlist::Shuffle);
    else if (plMode == "single") playlist.setMode(Playlist::Single);
    else playlist.setMode(Playlist::Sequential);

    playlist.setRotationInterval(settings.value("playlist/interval_sec", 0).toInt());
    playlist.setCurrentIndex(settings.value("media/start_index", 0).toInt());

    fprintf(stderr, "[DBG] Playlist ready, %d items\n", playlist.count()); fflush(stderr);

    // Engine
    fprintf(stderr, "[DBG] Creating WallpaperEngine...\n"); fflush(stderr);
    WallpaperEngine engine;
    engine.setTargetFps(settings.value("engine/fps", 30).toInt());

    auto applyMedia = [&](const QString& mediaPath) {
        WallpaperRenderer* renderer = createRendererForPath(mediaPath, fillMode, &engine);
        renderer->setVolume(volume);
        renderer->setMuted(muted);
        engine.setRenderer(renderer);
    };

    // Set initial wallpaper
    fprintf(stderr, "[DBG] Applying initial media: %s\n", qPrintable(playlist.current())); fflush(stderr);
    applyMedia(playlist.current());
    fprintf(stderr, "[DBG] Initial media applied\n"); fflush(stderr);

    // System integration
    SystemIntegration sysIntegration;
    sysIntegration.setPauseOnFullscreen(settings.value("behavior/pause_fullscreen", true).toBool());
    sysIntegration.setPauseOnBattery(settings.value("behavior/pause_battery", false).toBool());

    bool userPaused = false;

    fprintf(stderr, "[DBG] Creating tray icon...\n"); fflush(stderr);
    // Tray
    TrayIcon tray;
    fprintf(stderr, "[DBG] Tray created\n"); fflush(stderr);

    QObject::connect(&tray, &TrayIcon::enableToggled, [&](bool enabled) {
        if (enabled) {
            engine.start();
            sysIntegration.startMonitoring();
        } else {
            sysIntegration.stopMonitoring();
            engine.stop();
        }
    });

    QObject::connect(&tray, &TrayIcon::pauseToggled, [&](bool paused) {
        userPaused = paused;
        engine.setPaused(paused || sysIntegration.isPausedBySystem());
    });

    QObject::connect(&tray, &TrayIcon::muteToggled, [&](bool m) {
        muted = m;
        settings.setValue("audio/mute", m);
        settings.sync();
        // Propagate to current renderer via engine
        // (volume state is set on renderer creation)
    });

    QObject::connect(&tray, &TrayIcon::volumeChanged, [&](float v) {
        volume = v;
        settings.setValue("audio/volume", static_cast<int>(v * 100));
        settings.sync();
    });

    QObject::connect(&tray, &TrayIcon::nextRequested, &playlist, &Playlist::next);
    QObject::connect(&tray, &TrayIcon::previousRequested, &playlist, &Playlist::prev);

    QObject::connect(&playlist, &Playlist::currentChanged, [&](const QString& path) {
        applyMedia(path);
        tray.setToolTip(QString("WPE-Alt\n%1").arg(QFileInfo(path).fileName()));
    });

    QObject::connect(&tray, &TrayIcon::mediaSelectionRequested, [&]() {
        const QString filter =
            "Media Files (*.mp4 *.mkv *.webm *.avi *.mov *.wmv *.flv *.m4v *.ts *.mpeg *.mpg "
            "*.jpg *.jpeg *.png *.bmp *.gif *.webp *.apng);;"
            "All Files (*.*)";

        const QString selected = QFileDialog::getOpenFileName(
            nullptr,
            "Choose Wallpaper Media",
            baseDir.absolutePath(),
            filter
        );

        if (selected.isEmpty()) return;

        const QString absolute = QFileInfo(selected).absoluteFilePath();
        if (!isGifFile(absolute) && !isImageFile(absolute) && !VideoRenderer::canOpen(absolute)) {
            QMessageBox::warning(nullptr, "Unsupported Media",
                                 "Unable to decode this file. Try another file or transcode it to H.264 MP4.");
            return;
        }

        playlist.addItem(absolute);
        playlist.setCurrentIndex(playlist.items().indexOf(absolute));
        playlist.saveToSettings(settings);
        tray.setToolTip(QString("WPE-Alt\n%1").arg(QFileInfo(absolute).fileName()));
    });

    QObject::connect(&tray, &TrayIcon::galleryRequested, [&]() {
        WallpaperGallery gallery(playlist.items(), playlist.current());

        QObject::connect(&gallery, &WallpaperGallery::wallpaperSelected, [&](const QString& path) {
            int idx = playlist.items().indexOf(path);
            if (idx >= 0) {
                playlist.setCurrentIndex(idx);
            }
        });

        QObject::connect(&gallery, &WallpaperGallery::playlistModified, [&](const QStringList& paths) {
            playlist.setItems(paths);
            playlist.saveToSettings(settings);
        });

        gallery.exec();
    });

    QObject::connect(&tray, &TrayIcon::settingsRequested, [&]() {
        SettingsDialog dlg(&settings);

        QObject::connect(&dlg, &SettingsDialog::settingsChanged, [&]() {
            // Re-read settings
            engine.setTargetFps(settings.value("engine/fps", 30).toInt());
            fillMode = parseFillMode(settings.value("engine/fill_mode", "fill").toString());
            volume = settings.value("audio/volume", 100).toInt() / 100.0f;
            muted = settings.value("audio/mute", false).toBool();

            sysIntegration.setPauseOnFullscreen(
                settings.value("behavior/pause_fullscreen", true).toBool());
            sysIntegration.setPauseOnBattery(
                settings.value("behavior/pause_battery", false).toBool());

            // Apply auto-start
            bool autoStart = settings.value("engine/auto_start", false).toBool();
            sysIntegration.setAutoStart(autoStart);

            // Apply playlist settings
            QString plModeStr = settings.value("playlist/mode", "sequential").toString().toLower();
            if (plModeStr == "shuffle") playlist.setMode(Playlist::Shuffle);
            else if (plModeStr == "single") playlist.setMode(Playlist::Single);
            else playlist.setMode(Playlist::Sequential);
            playlist.setRotationInterval(settings.value("playlist/interval_sec", 0).toInt());

            // Re-apply current wallpaper with new settings
            applyMedia(playlist.current());

            tray.setMutedState(muted);
            tray.setVolumeState(static_cast<int>(volume * 100));
        });

        dlg.exec();
    });

    // System pause
    QObject::connect(&sysIntegration, &SystemIntegration::systemPauseChanged, [&](bool sysPaused) {
        engine.setPaused(userPaused || sysPaused);
    });

    // Apply auto-start state from settings
    sysIntegration.setAutoStart(settings.value("engine/auto_start", false).toBool());

    tray.setPausedState(false);
    tray.setMutedState(muted);
    tray.setVolumeState(static_cast<int>(volume * 100));
    tray.show();

    engine.start();
    sysIntegration.startMonitoring();

    if (!playlist.current().isEmpty()) {
        tray.setToolTip(QString("WPE-Alt\n%1").arg(QFileInfo(playlist.current()).fileName()));
    }

    return app.exec();
}
