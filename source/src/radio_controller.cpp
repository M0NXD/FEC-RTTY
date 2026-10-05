#include "fectty/radio_controller.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace fectty {
class RadioController::Impl {
public:
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::packaged_task<bool()>> queue;
    RadioStatus published;
    RadioStatus current; // Worker-thread only.
    std::unique_ptr<IRigControl> rig;
    bool stopping = false;
    std::chrono::steady_clock::time_point expiry{}, next_check{};
    std::thread worker;

    Impl() : worker([this] { run(); }) {}
    ~Impl() {
        { std::lock_guard lock(mutex); stopping = true; }
        wake.notify_all();
        worker.join();
    }
    void publish() {
        std::lock_guard lock(mutex);
        ++current.revision;
        published = current;
    }
    void fault(const std::string& message) {
        current.fault = true;
        current.message = message;
        publish();
    }
    std::future<bool> submit(std::function<bool()> function) {
        std::packaged_task<bool()> task([this, function=std::move(function)] {
            try { return function(); }
            catch (...) { fault("Radio backend exception; check hardware PTT"); off(false); return false; }
        });
        auto future = task.get_future();
        { std::lock_guard lock(mutex); queue.push_back(std::move(task)); }
        wake.notify_one();
        return future;
    }
    bool off(bool force) {
        if (!rig || (!current.ptt_owned && !force)) return !current.ptt_owned;
        // Retry ONLY release, never automatic re-keying. A lost reply can leave
        // the physical transmitter on even if our cached state says otherwise.
        bool ok = false;
        for (int attempt=0; attempt<2 && !ok; ++attempt) {
            if (attempt && !rig->state().connected) rig->connect();
            ok = rig->set_ptt(PttMode::Off);
            if (ok) {
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);
                do {
                    RigState read;
                    if(!rig->read_state(read)){ok=false;break;}
                    current.rig=read;
                    ok=!read.transmitting;
                    if(ok)break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                } while(std::chrono::steady_clock::now()<deadline);
            }
        }
        if (ok) {
            current.ptt_owned = false;
            if(!current.fault)current.message="PTT OFF confirmed — radio RX";
        }
        else { current.ptt_owned = true; current.fault = true; current.message = "PTT release NOT confirmed — unkey the radio manually"; }
        publish();
        return ok;
    }
    bool read() {
        RigState read;
        if (!rig || !rig->read_state(read) || !read.connected) {
            current.rig.connected = false;
            fault("CAT read failed/disconnected; transmission inhibited");
            off(false);
            return false;
        }
        current.rig = read;
        publish();
        return true;
    }
    bool close() {
        const bool ok = off(false);
        if (!ok) return false;
        if (rig) { rig->disconnect(); rig.reset(); }
        current.rig.connected = false;
        if (ok) { current.fault=false; current.message="Radio disconnected"; }
        publish();
        return ok;
    }
    void run() {
        for (;;) {
            std::packaged_task<bool()> task;
            {
                std::unique_lock lock(mutex);
                wake.wait_for(lock, std::chrono::milliseconds(25), [this] { return stopping || !queue.empty(); });
                if (!queue.empty()) { task=std::move(queue.front()); queue.pop_front(); }
                else if (stopping) break;
            }
            const auto now = std::chrono::steady_clock::now();
            try {
                if (current.ptt_owned && !current.fault && now >= expiry) {
                    fault("TX watchdog expired; transmission cancelled");
                    off(false);
                } else if (current.ptt_owned && !current.fault && now >= next_check) {
                    next_check = now + std::chrono::seconds(1);
                    read();
                }
                if (task.valid()) task();
            } catch (...) {
                fault("Radio worker failed; check hardware PTT");
                try { off(false); } catch (...) { fault("PTT release NOT confirmed — unkey the radio manually"); }
            }
        }
        try { close(); if(rig){rig->disconnect();rig.reset();} } catch (...) { rig.reset(); }
    }
};
RadioController::RadioController() : impl_(std::make_unique<Impl>()) {}
RadioController::~RadioController() = default;
RadioStatus RadioController::status() const { std::lock_guard lock(impl_->mutex); return impl_->published; }
std::future<bool> RadioController::connect(Factory factory) {
    return impl_->submit([this, factory=std::move(factory)] {
        if (!impl_->close()) return false; // Do not forget uncertain PTT ownership.
        impl_->rig = factory();
        if (!impl_->rig || !impl_->rig->connect() || !impl_->read()) {
            impl_->fault("Radio connection/status failed; verify model, port and radio power");
            return false;
        }
        impl_->current.fault = false;
        impl_->current.message = "CAT connected — PTT not armed";
        impl_->publish();
        return true;
    });
}
std::future<bool> RadioController::disconnect() { return impl_->submit([this] { return impl_->close(); }); }
std::future<bool> RadioController::refresh() { return impl_->submit([this] { return impl_->read(); }); }
std::future<bool> RadioController::apply(uint64_t frequency, RigMode mode) {
    return impl_->submit([this, frequency, mode] {
        if (!frequency || mode==RigMode::Unknown || impl_->current.ptt_owned || impl_->current.fault || !impl_->read() || impl_->current.rig.transmitting) return false;
        if (!impl_->rig->set_frequency(frequency) || !impl_->rig->set_mode(mode)) {
            impl_->fault("CAT frequency/mode command failed; radio may be partially changed");
            return false;
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);
        do {
            if (!impl_->read()) return false;
            if(impl_->current.rig.frequency_hz==frequency&&impl_->current.rig.mode==mode)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        } while(std::chrono::steady_clock::now()<deadline);
        if(impl_->current.rig.frequency_hz!=frequency||impl_->current.rig.mode!=mode){impl_->fault("Radio frequency/mode readback differs; verify radio controls");return false;}
        impl_->current.message = "Radio frequency/mode applied and read back";
        impl_->publish();
        return true;
    });
}
std::future<bool> RadioController::begin_tx(PttMode mode, std::chrono::milliseconds maximum) {
    return impl_->submit([this, mode, maximum] {
        if (mode==PttMode::Off || maximum.count()<=0 || maximum>std::chrono::minutes(10) || impl_->current.fault || impl_->current.ptt_owned || !impl_->read()) return false;
        if (impl_->current.rig.transmitting || impl_->current.rig.mode==RigMode::Unknown) {
            impl_->fault("Radio already transmitting or mode unsupported; Send inhibited");
            return false;
        }
        impl_->current.ptt_owned = true; // BEFORE issuing the potentially keying command.
        impl_->expiry = std::chrono::steady_clock::now()+maximum;
        impl_->next_check = std::chrono::steady_clock::now()+std::chrono::seconds(1);
        bool keyed=impl_->rig->set_ptt(mode);
        if(keyed){
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1500);
            do {
                keyed=impl_->read()&&impl_->current.rig.transmitting;
                if(keyed||impl_->current.fault)break;
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            } while(std::chrono::steady_clock::now()<deadline);
        }
        if (!keyed) {
            impl_->fault("PTT ON not confirmed; no modem audio sent");
            impl_->off(false);
            return false;
        }
        impl_->current.message = "CAT PTT active";
        impl_->publish();
        return true;
    });
}
std::future<bool> RadioController::end_tx() { return impl_->submit([this] { return impl_->off(false); }); }
std::future<bool> RadioController::emergency_off() { return impl_->submit([this] { return impl_->off(true); }); }
}
