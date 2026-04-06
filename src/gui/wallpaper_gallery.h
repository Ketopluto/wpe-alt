#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QPixmap>
#include <QImageReader>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QMouseEvent>
#include <QFrame>
#include <QStyle>

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(const QString& mediaPath, QWidget* parent = nullptr)
        : QLabel(parent), m_mediaPath(mediaPath) {
        setCursor(Qt::PointingHandCursor);
    }
    QString mediaPath() const { return m_mediaPath; }
signals:
    void clicked(const QString& path);
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked(m_mediaPath);
        }
        QLabel::mousePressEvent(event);
    }
private:
    QString m_mediaPath;
};

class WallpaperGallery : public QDialog {
    Q_OBJECT
public:
    explicit WallpaperGallery(const QStringList& mediaPaths, const QString& currentMedia,
                              QWidget* parent = nullptr)
        : QDialog(parent), m_mediaPaths(mediaPaths), m_currentMedia(currentMedia) {
        setWindowTitle("Wallpaper Gallery");
        setMinimumSize(640, 480);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        auto* mainLayout = new QVBoxLayout(this);

        // Toolbar
        auto* toolbar = new QHBoxLayout();
        auto* addFileBtn = new QPushButton("Add File...", this);
        auto* addFolderBtn = new QPushButton("Add Folder...", this);
        auto* removeBtn = new QPushButton("Remove Selected", this);
        toolbar->addWidget(addFileBtn);
        toolbar->addWidget(addFolderBtn);
        toolbar->addStretch();
        toolbar->addWidget(removeBtn);
        mainLayout->addLayout(toolbar);

        connect(addFileBtn, &QPushButton::clicked, this, &WallpaperGallery::addFile);
        connect(addFolderBtn, &QPushButton::clicked, this, &WallpaperGallery::addFolder);
        connect(removeBtn, &QPushButton::clicked, this, &WallpaperGallery::removeSelected);

        // Scroll area with grid
        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setWidgetResizable(true);
        m_gridWidget = new QWidget();
        m_gridLayout = new QGridLayout(m_gridWidget);
        m_gridLayout->setSpacing(8);
        m_scrollArea->setWidget(m_gridWidget);
        mainLayout->addWidget(m_scrollArea);

        // Status
        m_statusLabel = new QLabel(this);
        mainLayout->addWidget(m_statusLabel);

        rebuildGrid();
    }

    QStringList mediaPaths() const { return m_mediaPaths; }

signals:
    void wallpaperSelected(const QString& path);
    void playlistModified(const QStringList& paths);

private slots:
    void addFile() {
        const QString filter =
            "Media Files (*.mp4 *.mkv *.webm *.avi *.mov *.wmv *.flv *.m4v *.ts *.mpeg *.mpg "
            "*.jpg *.jpeg *.png *.bmp *.gif *.webp *.apng);;"
            "All Files (*.*)";
        QStringList files = QFileDialog::getOpenFileNames(this, "Add Media Files", QString(), filter);
        for (const QString& f : files) {
            QString abs = QFileInfo(f).absoluteFilePath();
            if (!m_mediaPaths.contains(abs)) {
                m_mediaPaths.append(abs);
            }
        }
        rebuildGrid();
        emit playlistModified(m_mediaPaths);
    }

    void addFolder() {
        QString dir = QFileDialog::getExistingDirectory(this, "Add Media Folder");
        if (dir.isEmpty()) return;

        const QStringList filters = {
            "*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov", "*.wmv", "*.flv", "*.m4v",
            "*.ts", "*.mpeg", "*.mpg", "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.gif",
            "*.webp", "*.apng"
        };
        QDir d(dir);
        const auto files = d.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            QString abs = fi.absoluteFilePath();
            if (!m_mediaPaths.contains(abs)) {
                m_mediaPaths.append(abs);
            }
        }
        rebuildGrid();
        emit playlistModified(m_mediaPaths);
    }

    void removeSelected() {
        if (m_selectedPath.isEmpty()) return;
        m_mediaPaths.removeAll(m_selectedPath);
        m_selectedPath.clear();
        rebuildGrid();
        emit playlistModified(m_mediaPaths);
    }

    void onThumbnailClicked(const QString& path) {
        m_selectedPath = path;
        highlightSelected();
        emit wallpaperSelected(path);
    }

private:
    void rebuildGrid() {
        // Clear existing
        QLayoutItem* item;
        while ((item = m_gridLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        const int cols = 4;
        const int thumbSize = 140;

        for (int i = 0; i < m_mediaPaths.size(); ++i) {
            const QString& path = m_mediaPaths.at(i);
            auto* container = new QFrame(m_gridWidget);
            container->setFrameStyle(QFrame::Box);
            container->setLineWidth(2);
            container->setObjectName(path);
            container->setFixedSize(thumbSize + 12, thumbSize + 32);

            auto* containerLayout = new QVBoxLayout(container);
            containerLayout->setContentsMargins(4, 4, 4, 4);
            containerLayout->setSpacing(2);

            auto* thumb = new ClickableLabel(path, container);
            thumb->setFixedSize(thumbSize, thumbSize);
            thumb->setAlignment(Qt::AlignCenter);
            thumb->setStyleSheet("background: #1a1a2e;");
            connect(thumb, &ClickableLabel::clicked, this, &WallpaperGallery::onThumbnailClicked);

            QPixmap pm = generateThumbnail(path, thumbSize);
            if (!pm.isNull()) {
                thumb->setPixmap(pm.scaled(thumbSize, thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                thumb->setText(QFileInfo(path).suffix().toUpper());
                thumb->setStyleSheet("background: #1a1a2e; color: #e94560; font-size: 16px; font-weight: bold;");
            }

            containerLayout->addWidget(thumb);

            auto* nameLabel = new QLabel(QFileInfo(path).fileName(), container);
            nameLabel->setAlignment(Qt::AlignCenter);
            nameLabel->setMaximumWidth(thumbSize);
            QFont f = nameLabel->font();
            f.setPointSize(7);
            nameLabel->setFont(f);
            nameLabel->setToolTip(path);
            containerLayout->addWidget(nameLabel);

            int row = i / cols;
            int col = i % cols;
            m_gridLayout->addWidget(container, row, col);
        }

        m_statusLabel->setText(QString("%1 wallpaper(s) in gallery").arg(m_mediaPaths.size()));
        highlightSelected();
    }

    void highlightSelected() {
        for (int i = 0; i < m_gridLayout->count(); ++i) {
            QWidget* w = m_gridLayout->itemAt(i)->widget();
            if (!w) continue;
            auto* frame = qobject_cast<QFrame*>(w);
            if (!frame) continue;
            bool isCurrent = (frame->objectName() == m_currentMedia);
            bool isSelected = (frame->objectName() == m_selectedPath);
            if (isSelected) {
                frame->setStyleSheet("QFrame { border: 2px solid #e94560; background: #16213e; }");
            } else if (isCurrent) {
                frame->setStyleSheet("QFrame { border: 2px solid #0f3460; background: #1a1a2e; }");
            } else {
                frame->setStyleSheet("QFrame { border: 1px solid #333; background: #0f0f23; }");
            }
        }
    }

    QPixmap generateThumbnail(const QString& path, int size) {
        QImageReader reader(path);
        if (!reader.canRead()) return {};
        reader.setAutoTransform(true);
        QSize imgSize = reader.size();
        if (imgSize.isValid() && (imgSize.width() > size * 2 || imgSize.height() > size * 2)) {
            reader.setScaledSize(imgSize.scaled(size, size, Qt::KeepAspectRatio));
        }
        QImage img = reader.read();
        if (img.isNull()) return {};
        return QPixmap::fromImage(img);
    }

    QStringList m_mediaPaths;
    QString m_currentMedia;
    QString m_selectedPath;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_gridWidget = nullptr;
    QGridLayout* m_gridLayout = nullptr;
    QLabel* m_statusLabel = nullptr;
};
