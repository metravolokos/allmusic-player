#pragma once

struct AndroidAudioTestActions
{
    bool play = false;
    bool pause = false;
    bool stop = false;
    bool restart = false;
};

class AndroidAudioTestState
{
public:
    enum class Value
    {
        Preparing,
        Ready,
        Loading,
        Playing,
        Paused,
        Stopped,
        Finished,
        Error
    };

    static constexpr AndroidAudioTestActions actions(Value state)
    {
        switch (state) {
        case Value::Ready:
        case Value::Stopped:
        case Value::Finished:
            return {true, false, false, true};
        case Value::Loading:
            return {false, true, true, true};
        case Value::Playing:
            return {false, true, true, true};
        case Value::Paused:
            return {true, false, true, true};
        case Value::Error:
            return {true, false, false, true};
        case Value::Preparing:
            return {};
        }
        return {};
    }
};
