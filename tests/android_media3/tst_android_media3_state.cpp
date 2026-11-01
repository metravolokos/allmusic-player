#include "AndroidCloudTrackListModel.h"
#include "AndroidFavoritesModel.h"
#include "AndroidMedia3State.h"
#include "TrackIdentity.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {
QVariantMap cloudEntry(const QString &label, const QString &sourcePath)
{
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("stableId"), TrackIdentity::stableIdForPath(sourcePath)},
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
        model.setEntries({cloudEntry(QStringLiteral("Ёжик.flac"),
                                     QStringLiteral("/AllMusic/Ёжик.flac")),
                          cloudEntry(QStringLiteral("КИНО — Remaster.mp3"),
                                     QStringLiteral("/AllMusic/КИНО — Remaster.mp3")),
                          cloudEntry(QStringLiteral("Кино — Remaster.mp3"),
                                     QStringLiteral("/AllMusic/Кино — Remaster.mp3")),
                          cloudEntry(QStringLiteral("Café.flac"),
                                     QStringLiteral("/AllMusic/Café.flac"))});

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
        model.setEntries({cloudEntry(QStringLiteral("Яблоко.flac"),
                                     QStringLiteral("/AllMusic/Яблоко.flac")),
                          cloudEntry(QStringLiteral("Альбом 2.mp3"),
                                     QStringLiteral("/AllMusic/Album-2-first.mp3")),
                          cloudEntry(QStringLiteral("Бета.mp3"),
                                     QStringLiteral("/AllMusic/Бета.mp3")),
                          cloudEntry(QStringLiteral("Альбом 2.mp3"),
                                     QStringLiteral("/AllMusic/Album-2-second.mp3")),
                          cloudEntry(QStringLiteral("Альбом 10.mp3"),
                                     QStringLiteral("/AllMusic/Альбом 10.mp3"))});

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
        model.setEntries({cloudEntry(QStringLiteral("Первый.mp3"),
                                     QStringLiteral("/AllMusic/Первый.mp3")),
                          cloudEntry(QStringLiteral("Второй.flac"),
                                     QStringLiteral("/AllMusic/Второй.flac"))});
        QVERIFY(model.containsSourceIndex(1));

        model.setSearchText(QStringLiteral("первый"));
        QVERIFY(!model.containsSourceIndex(1));
        QVERIFY(model.containsSourceIndex(0));

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
        model.setEntries({cloudEntry(QStringLiteral("Новый список.mp3"),
                                     QStringLiteral("/AllMusic/Новый список.mp3"))});
        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model.totalCount(), 1);
        // The search filter intentionally survives a source snapshot update.
        QVERIFY(!model.containsSourceIndex(1));
        QVERIFY(!model.containsSourceIndex(0));
        model.setSearchText(QString());
        QVERIFY(model.containsSourceIndex(0));
    }

    // Exercise the real Qt QML runtime, not just textual QML expectations.
    // This never opens the network or needs Android, audio output or user files.
    void qmlCallsCppModelAndReceivesPropertyNotifications()
    {
        AndroidCloudTrackListModel model;
        model.setEntries({cloudEntry(QStringLiteral("КИНО.mp3"),
                                     QStringLiteral("/AllMusic/КИНО.mp3")),
                          cloudEntry(QStringLiteral("Другой.flac"),
                                     QStringLiteral("/AllMusic/Другой.flac")),
                          cloudEntry(QStringLiteral("кино.flac"),
                                     QStringLiteral("/AllMusic/кино.flac"))});

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("cloudModel"), &model);
        QQmlComponent component(&engine);
        component.setData(R"QML(
import QtQml
QtObject {
    property int matches: cloudModel.resultCount
    property string query: cloudModel.searchText
    property int ordering: cloudModel.sortMode
    function searchAndCheck() {
        cloudModel.searchText = "кИНО"
        return cloudModel.containsSourceIndex(0)
               && cloudModel.containsSourceIndex(2)
               && !cloudModel.containsSourceIndex(1)
    }
    function setDescending() {
        cloudModel.sortMode = 1
        return cloudModel.sortMode
    }
}
)QML", QUrl(QStringLiteral("file:///qml-cpp-contract.qml")));

        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> qml(component.create());
        QVERIFY2(qml != nullptr, qPrintable(component.errorString()));
        QCOMPARE(qml->property("matches").toInt(), 3);

        QSignalSpy searchChanged(&model, &AndroidCloudTrackListModel::searchTextChanged);
        QVariant accepted;
        QVERIFY(QMetaObject::invokeMethod(qml.get(), "searchAndCheck",
                                          Q_RETURN_ARG(QVariant, accepted)));
        QVERIFY(accepted.toBool());
        QCOMPARE(searchChanged.count(), 1);
        QCOMPARE(model.resultCount(), 2);
        QCOMPARE(qml->property("matches").toInt(), 2);
        QCOMPARE(qml->property("query").toString(), QStringLiteral("кИНО"));

        QSignalSpy sortingChanged(&model, &AndroidCloudTrackListModel::sortModeChanged);
        QVariant ordering;
        QVERIFY(QMetaObject::invokeMethod(qml.get(), "setDescending",
                                          Q_RETURN_ARG(QVariant, ordering)));
        QCOMPARE(ordering.toInt(), AndroidCloudTrackListModel::NameDescending);
        QCOMPARE(sortingChanged.count(), 1);
        QCOMPARE(qml->property("ordering").toInt(),
                 AndroidCloudTrackListModel::NameDescending);

        // Source snapshot replacement must update the already-bound QML property.
        model.setEntries({cloudEntry(QStringLiteral("Новое.mp3"),
                                     QStringLiteral("/AllMusic/Новое.mp3"))});
        QCOMPARE(qml->property("matches").toInt(), 0);
    }

    void cloudModelHandlesFiveThousandSyntheticEntries()
    {
        AndroidCloudTrackListModel model;
        QVariantList entries;
        entries.reserve(5000);
        for (int index = 0; index < 5000; ++index) {
            const QString filename = QStringLiteral("Фиктивный трек %1.mp3")
                                         .arg(index, 4, 10, QChar(u'0'));
            entries.push_back(cloudEntry(filename, QStringLiteral("/AllMusic/") + filename));
        }

        model.setEntries(entries);
        QCOMPARE(model.totalCount(), 5000);
        QCOMPARE(model.resultCount(), 5000);
        model.setSearchText(QStringLiteral("ФИКТИВНЫЙ ТРЕК 4999"));
        QCOMPARE(model.resultCount(), 1);
        QCOMPARE(model.index(0, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 4999);
    }

    void favoritesCanBeAddedRemovedReaddedAndPersisted()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString settingsFile = temporaryDirectory.filePath(QStringLiteral("favorites.ini"));
        const QString sourcePath = QStringLiteral("/AllMusic/Album/Track.mp3");
        const QString stableId = TrackIdentity::stableIdForPath(sourcePath);

        {
            AndroidFavoritesModel favorites(settingsFile);
            QCOMPARE(favorites.count(), 0);
            QVERIFY(favorites.toggleFavorite(stableId, QStringLiteral("Track.mp3"), sourcePath));
            QVERIFY(favorites.contains(stableId));
            QCOMPARE(favorites.count(), 1);
            QCOMPARE(favorites.index(0, 0).data(AndroidFavoritesModel::LabelRole).toString(),
                     QStringLiteral("Track.mp3"));

            QVERIFY(favorites.toggleFavorite(stableId, QStringLiteral("Track.mp3"), sourcePath));
            QVERIFY(!favorites.contains(stableId));
            QCOMPARE(favorites.count(), 0);

            QVERIFY(favorites.toggleFavorite(stableId, QStringLiteral("Обновлённое имя.mp3"),
                                             sourcePath));
            QCOMPARE(favorites.count(), 1);
        }

        AndroidFavoritesModel reopened(settingsFile);
        QVERIFY(reopened.contains(stableId));
        QCOMPARE(reopened.count(), 1);
        QCOMPARE(reopened.index(0, 0).data(AndroidFavoritesModel::LabelRole).toString(),
                 QStringLiteral("Обновлённое имя.mp3"));
        QCOMPARE(reopened.sourcePathForStableId(stableId), sourcePath);
    }

    void disappearingTrackRemainsInFavoritesAndIsMarkedUnavailable()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        AndroidFavoritesModel favorites(temporaryDirectory.filePath(QStringLiteral("favorites.ini")));
        const QString sourcePath = QStringLiteral("/AllMusic/Album/Track.flac");
        const QString stableId = TrackIdentity::stableIdForPath(sourcePath);
        QVERIFY(favorites.toggleFavorite(stableId, QStringLiteral("Track.flac"), sourcePath));

        favorites.setCurrentEntries({cloudEntry(QStringLiteral("Track.flac"), sourcePath)},
                                    QStringLiteral("/AllMusic/Album/"), true);
        QCOMPARE(favorites.sourceIndexForStableId(stableId), 0);
        QVERIFY(favorites.index(0, 0).data(AndroidFavoritesModel::IsAvailableRole).toBool());
        QVERIFY(!favorites.index(0, 0).data(AndroidFavoritesModel::IsMissingRole).toBool());

        // A refreshed directory no longer contains the audio entry. The local
        // favorite remains, but is now explicitly represented as unavailable.
        favorites.setCurrentEntries({}, QStringLiteral("/AllMusic/Album/"), true);
        QCOMPARE(favorites.count(), 1);
        QVERIFY(favorites.contains(stableId));
        QCOMPARE(favorites.sourceIndexForStableId(stableId), -1);
        QVERIFY(!favorites.index(0, 0).data(AndroidFavoritesModel::IsAvailableRole).toBool());
        QVERIFY(favorites.index(0, 0).data(AndroidFavoritesModel::IsMissingRole).toBool());
        QVERIFY(favorites.isMissing(stableId));
        QCOMPARE(favorites.sourcePathForStableId(stableId), sourcePath);
    }

    void favoriteIdentityIsIndependentOfSearchAndSortRows()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QString sourcePath = QStringLiteral("/AllMusic/Zed.mp3");
        const QString stableId = TrackIdentity::stableIdForPath(sourcePath);
        AndroidFavoritesModel favorites(temporaryDirectory.filePath(QStringLiteral("favorites.ini")));
        QVERIFY(favorites.toggleFavorite(stableId, QStringLiteral("Zed.mp3"), sourcePath));

        const QVariantList entries = {
            cloudEntry(QStringLiteral("Zed.mp3"), sourcePath),
            cloudEntry(QStringLiteral("Alpha.mp3"), QStringLiteral("/AllMusic/Alpha.mp3")),
        };
        favorites.setCurrentEntries(entries, QStringLiteral("/AllMusic/"), true);

        AndroidCloudTrackListModel cloud;
        cloud.setEntries(entries);
        cloud.setFavoriteIds(favorites.favoriteIds());
        cloud.setSortMode(AndroidCloudTrackListModel::NameAscending);
        QCOMPARE(cloud.index(1, 0).data(AndroidCloudTrackSourceModel::StableIdRole).toString(),
                 stableId);
        QCOMPARE(cloud.index(1, 0).data(AndroidCloudTrackSourceModel::SourceIndexRole).toInt(), 0);
        QVERIFY(cloud.index(1, 0).data(AndroidCloudTrackSourceModel::IsFavoriteRole).toBool());
        QCOMPARE(favorites.sourceIndexForStableId(stableId), 0);

        cloud.setSearchText(QStringLiteral("Alpha"));
        QCOMPARE(cloud.resultCount(), 1);
        QVERIFY(favorites.contains(stableId));
        QCOMPARE(favorites.sourceIndexForStableId(stableId), 0);
        QCOMPARE(favorites.sourcePathForStableId(stableId), sourcePath);
        QVERIFY(!stableId.contains(QStringLiteral("Zed")));
        QVERIFY(!favorites.roleNames().values().contains(QByteArrayLiteral("sourcePath")));
        QVERIFY(!cloud.index(0, 0).model()->roleNames().values().contains(
            QByteArrayLiteral("remotePath")));
    }

    void favoritesRejectInvalidOrMismatchedPaths()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        AndroidFavoritesModel favorites(temporaryDirectory.filePath(QStringLiteral("favorites.ini")));
        const QString traversalPath = QStringLiteral("/AllMusic/../outside.mp3");
        QVERIFY(!favorites.toggleFavorite(TrackIdentity::stableIdForPath(traversalPath),
                                         QStringLiteral("outside.mp3"), traversalPath));
        QVERIFY(!favorites.toggleFavorite(
            TrackIdentity::stableIdForPath(QStringLiteral("/AllMusic/one.mp3")),
            QStringLiteral("two.mp3"), QStringLiteral("/AllMusic/two.mp3")));
        QCOMPARE(favorites.count(), 0);
    }
};

QTEST_GUILESS_MAIN(AndroidMedia3StateTest)
#include "tst_android_media3_state.moc"
