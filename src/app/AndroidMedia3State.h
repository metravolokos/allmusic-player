#pragma once

struct AndroidMedia3Actions
{
    bool play = false;
    bool pause = false;
    bool stop = false;
};

class AndroidMedia3State
{
public:
    enum class Value : int
    {
        Preparing = 0,
        Ready = 1,
        Playing = 2,
        Paused = 3,
        Stopped = 4,
        Finished = 5,
        Error = 6,
        Buffering = 7
    };

    static constexpr AndroidMedia3Actions actions(Value state)
    {
        switch (state) {
        case Value::Ready:
        case Value::Stopped:
        case Value::Finished:
        case Value::Error:
            return {true, false, false};
        case Value::Preparing:
        case Value::Playing:
        case Value::Buffering:
            return {false, true, true};
        case Value::Paused:
            return {true, false, true};
        }
        return {};
    }

    static constexpr bool seekEnabled(Value state, bool hasDuration)
    {
        return hasDuration && (state == Value::Ready || state == Value::Playing
                               || state == Value::Paused || state == Value::Buffering);
    }

    // A user Stop is an explicit boundary: a late Media3 poll must not restart
    // playback in the UI. A new selection/play command first moves the local
    // state out of Stopped, after which Media3 updates are accepted again.
    static constexpr bool acceptsRemoteUpdate(Value current, Value incoming)
    {
        if (current == Value::Stopped)
            return incoming == Value::Stopped;
        if (current == Value::Paused)
            return incoming == Value::Paused || incoming == Value::Error;
        return true;
    }
};
