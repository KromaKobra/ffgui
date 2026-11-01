#include "App.h"
#include "ModelRow.h"
#include "Models.h"

#include <QSignalSpy>
#include <QTest>

class TestModels : public QObject
{
    Q_OBJECT

private slots:
    void rowsCarryEveryRole()
    {
        RowModel m({ "a", "b" });
        m.append({ { "a", 1 } });
        QCOMPARE(m.count(), 1);
        QVERIFY(m.get(0).contains(QStringLiteral("b")));
        QCOMPARE(m.value(0, QStringLiteral("a")).toInt(), 1);
        QCOMPARE(m.find(QStringLiteral("a"), 1), 0);
        QCOMPARE(m.find(QStringLiteral("a"), 2), -1);
    }

    void updateReportsOnlyChangedRoles()
    {
        RowModel m({ "a", "b", "c" });
        m.append({ { "a", 1 }, { "b", 2 }, { "c", 3 } });
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
        QSignalSpy revision(&m, &RowModel::revisionChanged);

        m.update(0, { { "b", 5 }, { "c", 3 }, { "unknown", 9 } });
        QCOMPARE(changed.count(), 1);
        const auto roles = changed.at(0).at(2).value<QList<int>>();
        QCOMPARE(roles, QList<int>({ Qt::UserRole + 1 }));
        QCOMPARE(revision.count(), 1);

        m.update(0, { { "b", 5 } });                 // no change, no signal
        QCOMPARE(changed.count(), 1);
        QCOMPARE(revision.count(), 1);
    }

    void moveAndRemove()
    {
        RowModel m({ "k" });
        for (int i = 0; i < 4; ++i)
            m.append({ { "k", i } });
        QSignalSpy moved(&m, &QAbstractItemModel::rowsMoved);
        m.move(0, 3);
        QCOMPARE(moved.count(), 1);
        QCOMPARE(m.value(3, QStringLiteral("k")).toInt(), 0);
        QCOMPARE(m.value(0, QStringLiteral("k")).toInt(), 1);
        m.move(3, 1);
        QCOMPARE(m.value(1, QStringLiteral("k")).toInt(), 0);
        m.removeAt(1);
        QCOMPARE(m.count(), 3);
        QCOMPARE(m.find(QStringLiteral("k"), 0), -1);
    }

    void modelRowFollowsItsRow()
    {
        RowModel m({ "name" });
        m.append({ { "name", "a" } });
        m.append({ { "name", "b" } });
        ModelRow row;
        QSignalSpy spy(&row, &ModelRow::valuesChanged);
        row.setModel(&m);
        row.setRow(1);
        QVERIFY(row.valid());
        QCOMPARE(row.values().value(QStringLiteral("name")).toString(), QStringLiteral("b"));

        spy.clear();
        m.update(0, { { "name", "x" } });            // another row: nothing to report
        QCOMPARE(spy.count(), 0);
        m.update(1, { { "name", "y" } });
        QCOMPARE(spy.count(), 1);
        QCOMPARE(row.values().value(QStringLiteral("name")).toString(), QStringLiteral("y"));

        m.removeAt(1);
        QVERIFY(!row.valid());
    }

    void stepFilterSearchesPlainAndFfmpegNames()
    {
        App app;
        StepFilter filter;
        filter.setSourceModel(app.catalog()->steps());
        filter.setSearch(QStringLiteral("afftdn"));
        QCOMPARE(filter.count(), 1);
        QCOMPARE(filter.get(0).value(QStringLiteral("key")).toString(), QStringLiteral("denoise"));
        filter.setSearch(QStringLiteral("noise"));
        QVERIFY(filter.count() >= 1);
        filter.setSearch({});
        filter.setCategory(QStringLiteral("Time"));
        QCOMPARE(filter.count(), 4);
    }

    void selectionFollowsListChanges()
    {
        Selection s;
        s.select(QStringLiteral("step"), { { "index", 2 } });
        s.itemInserted(QStringLiteral("step"), 0);
        QCOMPARE(s.index(), 3);
        s.itemMoved(QStringLiteral("step"), 3, 0);
        QCOMPARE(s.index(), 0);
        s.itemRemoved(QStringLiteral("step"), 1);
        QCOMPARE(s.index(), 0);
        s.itemRemoved(QStringLiteral("step"), 0);
        QCOMPARE(s.kind(), QStringLiteral("none"));

        s.select(QStringLiteral("range"), { { "start", 5.0 }, { "end", 2.0 } });
        QCOMPARE(s.start(), 2.0);
        QCOMPARE(s.end(), 5.0);
    }

    void viewStateRoundTrips()
    {
        ViewState v;
        v.setLayer(QStringLiteral("loudness"), true);
        v.setSpectrogramShare(0.7);
        v.setChainGraph(true);
        const QVariantMap saved = v.save();
        ViewState w;
        w.apply(saved);
        QCOMPARE(w.save(), saved);
        // Missing keys take defaults; unknown layers are ignored.
        w.apply({ { "layers", QVariantMap { { "bogus", true } } } });
        QCOMPARE(w.layers(), ViewState::defaultLayers());
        QCOMPARE(w.spectrogramShare(), 0.45);
        QVERIFY(!w.chainGraph());
    }

    void transportClampsAndSeeks()
    {
        Transport t;
        t.play();
        QVERIFY(!t.playing());                      // nothing to play yet
        t.setDuration(10);
        t.seek(12);
        QCOMPARE(t.position(), 10.0);
        t.setMarkers({ 6, 2 });
        t.seek(5);
        t.previousMarker();
        QCOMPARE(t.position(), 2.0);
        t.nextMarker();
        QCOMPARE(t.position(), 6.0);
        t.toggleSolo(1);
        QVERIFY(t.isSoloed(1));
        QVERIFY(!t.isSoloed(0));
    }

    void metersHoldAndLatchClips()
    {
        Meters m;
        Meters::Reading r;
        r.peak = { -3, -80 };
        r.rms = { -12, -90 };
        r.clipped = { true, false };
        m.apply(r);
        QCOMPARE(m.peak().at(0), -3.0);
        QCOMPARE(m.peak().at(1), m.floor());
        QVERIFY(m.clip().at(0));
        r.peak = { -20, -80 };
        r.clipped = { false, false };
        m.apply(r);
        QCOMPARE(m.hold().at(0), -3.6);             // falls 0.6 dB per reading
        QVERIFY(m.clip().at(0));                    // latched
        m.resetClip(0);
        QVERIFY(!m.clip().at(0));
    }
};

QTEST_MAIN(TestModels)
#include "tst_models.moc"
