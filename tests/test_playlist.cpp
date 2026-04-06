#include <QtTest>
#include <QSignalSpy>
#include "../src/core/playlist.h"
#include "moc_playlist.cpp"

class TestPlaylist : public QObject
{
    Q_OBJECT

private slots:
    void testSequential()
    {
        Playlist playlist;
        playlist.setMode(Playlist::Sequential);
        playlist.setItems({"item1.mp4", "item2.png", "item3.mp4"});
        playlist.setCurrentIndex(0);

        QCOMPARE(playlist.current(), QString("item1.mp4"));

        playlist.next();
        QCOMPARE(playlist.current(), QString("item2.png"));

        playlist.next();
        QCOMPARE(playlist.current(), QString("item3.mp4"));

        // Loop back to start
        playlist.next();
        QCOMPARE(playlist.current(), QString("item1.mp4"));

        // Previous
        playlist.prev();
        QCOMPARE(playlist.current(), QString("item3.mp4"));
    }

    void testSingle()
    {
        Playlist playlist;
        playlist.setMode(Playlist::Single);
        playlist.setItems({"item1.mp4", "item2.png", "item3.mp4"});
        playlist.setCurrentIndex(1);

        QCOMPARE(playlist.current(), QString("item2.png"));

        // Next shouldn't change the item in Single mode
        playlist.next();
        QCOMPARE(playlist.current(), QString("item2.png"));

        // Prev shouldn't change the item in Single mode
        playlist.prev();
        QCOMPARE(playlist.current(), QString("item2.png"));
    }

    void testShuffle()
    {
        Playlist playlist;
        playlist.setMode(Playlist::Shuffle);
        playlist.setItems({"item1", "item2", "item3", "item4", "item5", "item6"});
        playlist.setCurrentIndex(0);

        // It's tricky to test shuffle randomly, but we can verify that
        // next() progresses indices up to size, and the mode is Shuffle.
        QCOMPARE(playlist.mode(), Playlist::Shuffle);
        QVERIFY(!playlist.current().isEmpty());

        QString first = playlist.current();
        playlist.next();
        QString second = playlist.current();

        // They should likely be different (unless shuffle happened to match same index which shouldn't happen sequentially)
        // Let's just ensure we can advance through all items without crashing
        for (int i = 0; i < 10; ++i) {
            playlist.next();
            QVERIFY(!playlist.current().isEmpty());
        }
    }

    void testAddRemove()
    {
        Playlist playlist;
        playlist.setItems({"item1"});
        playlist.addItem("item2");
        QCOMPARE(playlist.count(), 2);

        playlist.setCurrentIndex(0);
        QCOMPARE(playlist.current(), QString("item1"));

        playlist.removeItem(0);
        QCOMPARE(playlist.count(), 1);

        // Current index should adjust or stay valid
        QCOMPARE(playlist.current(), QString("item2"));
    }
};

QTEST_MAIN(TestPlaylist)
#include "test_playlist.moc"
