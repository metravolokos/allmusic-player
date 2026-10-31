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
        Error = 6
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
            return {false, true, true};
        case Value::Paused:
            return {true, false, true};
        }
        return {};
    }

    static constexpr bool seekEnabled(Value state, bool hasDuration)
    {
        return hasDuration && state != Value::Preparing && state != Value::Error;
    }
};
