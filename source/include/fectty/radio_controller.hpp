#pragma once
#include "fectty/rig_control.hpp"
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>

namespace fectty {
struct RadioStatus {
    RigState rig;
    bool ptt_owned = false; // Also true if a failed command MAY have keyed.
    bool fault = false;
    std::string message = "Radio disconnected";
    unsigned revision = 0;
};

// All backend calls, construction and COM cleanup occur on ONE worker thread.
// The UI only reads snapshots; the audio callback never performs CAT work.
class RadioController {
    class Impl;
    std::unique_ptr<Impl> impl_;
public:
    using Factory = std::function<std::unique_ptr<IRigControl>()>;
    RadioController();
    ~RadioController();
    RadioController(const RadioController&) = delete;
    RadioController& operator=(const RadioController&) = delete;
    RadioStatus status() const;
    std::future<bool> connect(Factory);
    std::future<bool> disconnect();
    std::future<bool> refresh();
    std::future<bool> apply(uint64_t frequency_hz, RigMode);
    std::future<bool> begin_tx(PttMode, std::chrono::milliseconds maximum);
    std::future<bool> end_tx();
    std::future<bool> emergency_off();
};
}
