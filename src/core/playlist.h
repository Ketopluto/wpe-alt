#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QSettings>
#include <QRegularExpression>
#include <algorithm>
#include <numeric>
#include <random>

class Playlist : public QObject {
    Q_OBJECT
public:
    enum Mode { Sequential, Shuffle, Single };
    Q_ENUM(Mode)

    explicit Playlist(QObject* parent = nullptr)
        : QObject(parent), m_mode(Sequential), m_index(0) {
        m_rotationTimer = new QTimer(this);
        m_rotationTimer->setTimerType(Qt::CoarseTimer);
        connect(m_rotationTimer, &QTimer::timeout, this, &Playlist::next);
    }

    void setItems(const QStringList& items) {
        m_items = items;
        m_shuffledIndices.clear();
        if (m_items.isEmpty()) {
            m_index = 0;
            return;
        }
        m_index = qBound(0, m_index, m_items.size() - 1);
        rebuildShuffleOrder();
    }

    QStringList items() const { return m_items; }
    int count() const { return m_items.size(); }
    bool isEmpty() const { return m_items.isEmpty(); }

    QString current() const {
        if (m_items.isEmpty()) return {};
        int idx = effectiveIndex();
        return (idx >= 0 && idx < m_items.size()) ? m_items.at(idx) : QString{};
    }

    int currentIndex() const { return m_index; }

    void setCurrentIndex(int index) {
        if (m_items.isEmpty()) return;
        m_index = qBound(0, index, m_items.size() - 1);
        emit currentChanged(current());
    }

    Mode mode() const { return m_mode; }

    void setMode(Mode mode) {
        if (m_mode == mode) return;
        m_mode = mode;
        if (m_mode == Shuffle) {
            rebuildShuffleOrder();
        }
        emit modeChanged(m_mode);
    }

    int rotationIntervalSec() const { return m_rotationSec; }

    void setRotationInterval(int seconds) {
        m_rotationSec = qMax(0, seconds);
        if (m_rotationSec > 0) {
            m_rotationTimer->setInterval(m_rotationSec * 1000);
            if (!m_rotationTimer->isActive() && m_items.size() > 1) {
                m_rotationTimer->start();
            }
        } else {
            m_rotationTimer->stop();
        }
    }

    void addItem(const QString& path) {
        if (path.isEmpty() || m_items.contains(path)) return;
        m_items.append(path);
        rebuildShuffleOrder();
        emit playlistChanged();
    }

    void removeItem(int index) {
        if (index < 0 || index >= m_items.size()) return;
        m_items.removeAt(index);
        rebuildShuffleOrder();
        if (m_index >= m_items.size()) {
            m_index = m_items.isEmpty() ? 0 : m_items.size() - 1;
        }
        emit playlistChanged();
    }

    void loadFromSettings(QSettings& settings) {
        QString raw = settings.value("media/items").toString();
        if (raw.trimmed().isEmpty()) {
            raw = settings.value("image/path").toString();
        }
        QStringList items;
        const auto entries = raw.split(QRegularExpression("[;|,]"), Qt::SkipEmptyParts);
        for (const QString& e : entries) {
            QString trimmed = e.trimmed();
            if (!trimmed.isEmpty()) items << trimmed;
        }
        setItems(items);

        QString modeStr = settings.value("playlist/mode", "sequential").toString().toLower();
        if (modeStr == "shuffle") setMode(Shuffle);
        else if (modeStr == "single") setMode(Single);
        else setMode(Sequential);

        setRotationInterval(settings.value("playlist/interval_sec", 0).toInt());
        setCurrentIndex(settings.value("media/start_index", 0).toInt());
    }

    void saveToSettings(QSettings& settings) const {
        settings.setValue("media/items", m_items.join(";"));
        settings.setValue("media/start_index", m_index);
        QString modeStr = "sequential";
        if (m_mode == Shuffle) modeStr = "shuffle";
        else if (m_mode == Single) modeStr = "single";
        settings.setValue("playlist/mode", modeStr);
        settings.setValue("playlist/interval_sec", m_rotationSec);
        settings.sync();
    }

public slots:
    void next() {
        if (m_items.size() <= 1) return;
        if (m_mode == Single) {
            emit currentChanged(current());
            return;
        }
        m_index = (m_index + 1) % m_items.size();
        emit currentChanged(current());
    }

    void prev() {
        if (m_items.size() <= 1) return;
        if (m_mode == Single) {
            emit currentChanged(current());
            return;
        }
        m_index = (m_index - 1 + m_items.size()) % m_items.size();
        emit currentChanged(current());
    }

signals:
    void currentChanged(const QString& path);
    void modeChanged(Playlist::Mode mode);
    void playlistChanged();

private:
    int effectiveIndex() const {
        if (m_mode == Shuffle && !m_shuffledIndices.isEmpty()) {
            int si = qBound(0, m_index, (int)m_shuffledIndices.size() - 1);
            return m_shuffledIndices.at(si);
        }
        return m_index;
    }

    void rebuildShuffleOrder() {
        m_shuffledIndices.resize(m_items.size());
        std::iota(m_shuffledIndices.begin(), m_shuffledIndices.end(), 0);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(m_shuffledIndices.begin(), m_shuffledIndices.end(), gen);
    }

    QStringList m_items;
    QVector<int> m_shuffledIndices;
    int m_index = 0;
    Mode m_mode = Sequential;
    QTimer* m_rotationTimer = nullptr;
    int m_rotationSec = 0;
};
