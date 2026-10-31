#include "AndroidCloudTrackListModel.h"
#include "AndroidMedia3State.h"

#include <QtTest>

namespace {
QVariantMap cloudEntry(const QString &label)
{
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("isDirectory"), false},
        {QStringLiteral("isAudio"), true},
    };
}
}

class AndroidMedia3StateTest final : public QObject
{
    Q_OBJECT

private slots:
    void preparingDisablesPlayUntilTheTrackIsReady()
    {
        const auto actions = AndroidMedia3State::actions(AndroidMedia3State::Value::Preparing);
        QVERIFY(!actions.play);
        QVERIFY(actions.pause);
        QVERIFY(actions.stop);
    }

    void readyAndStoppedAllowPlay()
    {
        for (const auto state : {AndroidMedia3State::Value::Ready,
                                 AndroidMedia3State::Value::Stopped,
                                 AndroidMedia3State::Value::Finished}) {
            const auto actions = AndroidMedia3State::actions(state);
            QVERIFY(actions.play);
            QVERIFY(!actions.pause);
            QVERIFY(!actions.stop);
        }
    }

    void activePlaybackAllowsPauseAndStop()
    {
        const auto actions = AndroidMedia3State::actions(AndroidMedia3State::Value::Playing);
        QVERIFY(!actions.play);
        QVERIFY(actions.pause);
        QVERIFY(actions.stop);
    }

    void pausedPlaybackAllowsResumeOrStop()
    {
        const auto actions = AndroidMedia3State::actions(AndroidMedia3State::Value::Paused);
        QVERIFY(actions.play);
        QVERIFY(!actions.pause);
        QVERIFY(actions.stop);
    }

    void errorAllowsRetry()
    {
        const auto actions = AndroidMedia3State::actions(AndroidMedia3State::Value::Error);
        QVERIFY(actions.play);
        QVERIFY(!actions.pause);
        QVERIFY(!actions.stop);
    }

    void seekNeedsDurationAndAUsableState()
    {
        QVERIFY(!AndroidMedia3State::seekEnabled(AndroidMedia3State::Value::Ready, false));
        QVERIFY(!AndroidMedia3State::seekEnabled(AndroidMedia3State::Value::Preparing, true));
        QVERIFY(!AndroidMedia3State::seekEnabled(AndroidMedia3State::Value::Error, true));
        QVERIFY(AndroidMedia3State::seekEnabled(AndroidMedia3State::Value::Playing, true));
        QVERIFY(AndroidMedia3State::seekEnabled(AndroidMedia3State::Value::Paused, true));
    }

    void cloudSearchIsUnicodeAndCaseInsensitive()
    {
        AndroidCloudTrackListModel model;
        model.setEntries({cloudEntry(QStringLiteral("Ёжик.flac")),
                          cloudEntry(QStringLiteral("КИНО — Remaster.mp3")),
                          cloudEntry(QStringLiteral("Кино — Remaster.mp3")),
                          cloudEntry(QStringLiteral("Café.flac"))});

        model.setSearchText(QStringLiteral("кИНО"));
        QCOMPARE(model.totalCount(), 4);
        QCOMPARE(model.resultCount(), 2);
        QCOMPARE(model.index(0, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 1);
        QCOMPARE(model.index(1, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 2);

        model.setSearchText(QStringLiteral("É"));
        QCOMPARE(model.resultCount(), 1);
        QCOMPARE(model.index(0, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 3);
    }

    void cloudSortKeepsDuplicateNamesMappedToTheirSourceRows()
    {
        AndroidCloudTrackListModel model;
        model.setEntries({cloudEntry(QStringLiteral("Яблоко.flac")),
                          cloudEntry(QStringLiteral("Альбом 2.mp3")),
                          cloudEntry(QStringLiteral("Бета.mp3")),
                          cloudEntry(QStringLiteral("Альбом 2.mp3")),
                          cloudEntry(QStringLiteral("Альбом 10.mp3"))});

        const auto sourceIndexAt = [&model](int row) {
            return model.index(row, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt();
        };
        QCOMPARE(sourceIndexAt(0), 1);
        QCOMPARE(sourceIndexAt(1), 3);
        QCOMPARE(sourceIndexAt(2), 4);
        QCOMPARE(sourceIndexAt(3), 2);
        QCOMPARE(sourceIndexAt(4), 0);

        model.setSortMode(AndroidCloudTrackListModel::NameDescending);
        QCOMPARE(sourceIndexAt(0), 0);
        QCOMPARE(sourceIndexAt(1), 2);
        QCOMPARE(sourceIndexAt(2), 4);
        QCOMPARE(sourceIndexAt(3), 3);
        QCOMPARE(sourceIndexAt(4), 1);

        model.setSortMode(AndroidCloudTrackListModel::SourceOrder);
        for (int row = 0; row < model.resultCount(); ++row)
            QCOMPARE(sourceIndexAt(row), row);
    }

    void filteringHidesSelectionAndNewSnapshotsResetSourceRows()
    {
        AndroidCloudTrackListModel model;
        model.setEntries({cloudEntry(QStringLiteral("Первый.mp3")),
                          cloudEntry(QStringLiteral("Второй.flac"))});
        QVERIFY(model.containsSourceIndex(1));

        model.setSearchText(QStringLiteral("первый"));
        QVERIFY(!model.containsSourceIndex(1));
        QVERIFY(model.containsSourceIndex(0));

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        model.setEntries({cloudEntry(QStringLiteral("Новый список.mp3"))});
        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model.totalCount(), 1);
        // The search filter intentionally survives a source snapshot update.
        QVERIFY(!model.containsSourceIndex(1));
        QVERIFY(!model.containsSourceIndex(0));
        model.setSearchText(QString());
        QVERIFY(model.containsSourceIndex(0));
    }

    void cloudModelHandlesFiveThousandSyntheticEntries()
    {
        AndroidCloudTrackListModel model;
        QVariantList entries;
        entries.reserve(5000);
        for (int index = 0; index < 5000; ++index) {
            entries.push_back(cloudEntry(QStringLiteral("Фиктивный трек %1.mp3")
                                             .arg(index, 4, 10, QChar(u'0'))));
        }

        model.setEntries(entries);
        QCOMPARE(model.totalCount(), 5000);
        QCOMPARE(model.resultCount(), 5000);
        model.setSearchText(QStringLiteral("ФИКТИВНЫЙ ТРЕК 4999"));
        QCOMPARE(model.resultCount(), 1);
        QCOMPARE(model.index(0, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 4999);
    }
};

QTEST_APPLESS_MAIN(AndroidMedia3StateTest)
#include "tst_android_media3_state.moc"
