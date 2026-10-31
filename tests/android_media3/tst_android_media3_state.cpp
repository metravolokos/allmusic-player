#include "AndroidMedia3State.h"

#include <QtTest>

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
};

QTEST_APPLESS_MAIN(AndroidMedia3StateTest)
#include "tst_android_media3_state.moc"
