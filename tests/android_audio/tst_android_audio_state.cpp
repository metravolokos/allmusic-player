#include "AndroidAudioTestState.h"

#include <QtTest>

class AndroidAudioTestStateTest final : public QObject
{
    Q_OBJECT

private slots:
    void formatSelectorAcceptsOnlyKnownSources()
    {
        QVERIFY(AndroidAudioTestState::isValidTrackIndex(0));
        QVERIFY(AndroidAudioTestState::isValidTrackIndex(1));
        QVERIFY(AndroidAudioTestState::isValidTrackIndex(2));
        QVERIFY(!AndroidAudioTestState::isValidTrackIndex(-1));
        QVERIFY(!AndroidAudioTestState::isValidTrackIndex(3));
    }

    void preparingDisablesPlaybackControls()
    {
        const auto actions = AndroidAudioTestState::actions(AndroidAudioTestState::Value::Preparing);
        QVERIFY(!actions.play);
        QVERIFY(!actions.pause);
        QVERIFY(!actions.stop);
        QVERIFY(!actions.restart);
    }

    void readyAndStoppedAllowStartingAgain()
    {
        for (const auto state : {AndroidAudioTestState::Value::Ready,
                                 AndroidAudioTestState::Value::Stopped,
                                 AndroidAudioTestState::Value::Finished}) {
            const auto actions = AndroidAudioTestState::actions(state);
            QVERIFY(actions.play);
            QVERIFY(!actions.pause);
            QVERIFY(!actions.stop);
            QVERIFY(actions.restart);
        }
    }

    void activePlaybackAllowsPauseStopAndRestart()
    {
        for (const auto state : {AndroidAudioTestState::Value::Loading,
                                 AndroidAudioTestState::Value::Playing}) {
            const auto actions = AndroidAudioTestState::actions(state);
            QVERIFY(!actions.play);
            QVERIFY(actions.pause);
            QVERIFY(actions.stop);
            QVERIFY(actions.restart);
        }
    }

    void pausedPlaybackAllowsResumeOrStop()
    {
        const auto actions = AndroidAudioTestState::actions(AndroidAudioTestState::Value::Paused);
        QVERIFY(actions.play);
        QVERIFY(!actions.pause);
        QVERIFY(actions.stop);
        QVERIFY(actions.restart);
    }

    void errorAllowsExplicitRetry()
    {
        const auto actions = AndroidAudioTestState::actions(AndroidAudioTestState::Value::Error);
        QVERIFY(actions.play);
        QVERIFY(!actions.pause);
        QVERIFY(!actions.stop);
        QVERIFY(actions.restart);
    }
};

QTEST_APPLESS_MAIN(AndroidAudioTestStateTest)
#include "tst_android_audio_state.moc"
