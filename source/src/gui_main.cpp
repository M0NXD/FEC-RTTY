#ifdef FECTTY_HAS_QT

#include "fectty/rig_control.hpp"
#include "fectty/radio_controller.hpp"
#include "fectty/hamlib_rig.hpp"
#include "fectty/bench_exit.hpp"
#include "fectty/gui_text.hpp"
#include "fectty/parse.hpp"
#include "fectty/net_rigctl.hpp"
#include "fectty/portaudio_io.hpp"
#include "fectty/reliable_receiver.hpp"
#include "fectty/settings.hpp"
#include "fectty/winmm_io.hpp"
#include "fectty/waterfall_widget.hpp"

#ifdef _WIN32
#include "fectty/omnirig.hpp"
#endif

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPalette>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

QString settings_path() {
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/fectty.ini");
}

QString legacy_settings_path() {
    const auto base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("FEC-TTY/FEC-TTY/fectty.ini"));
}

QLabel* value_label(const QString& value = QStringLiteral("—")) {
    auto* label = new QLabel(value);
    label->setObjectName(QStringLiteral("valueLabel"));
    return label;
}

class MainWindow final : public QMainWindow {
    QComboBox* rx_device_ = nullptr;
    QComboBox* tx_device_ = nullptr;
    QComboBox* audio_backend_ = nullptr;
    QPushButton* audio_refresh_ = nullptr;
    QComboBox* rig_backend_ = nullptr;
    QLineEdit* rig_host_ = nullptr;
    QSpinBox* rig_port_ = nullptr;
    QLabel* state_ = nullptr;
    QLabel* audio_state_ = nullptr;
    QLabel* frequency_ = nullptr;
    QLabel* rig_state_ = nullptr;
    QLabel* frames_ = nullptr;
    QLabel* crc_ = nullptr;
    QLabel* gaps_ = nullptr;
    QLabel* drops_ = nullptr;
    QLabel* capture_interruptions_ = nullptr;
    QLabel* captured_ = nullptr;
    QLabel* processed_ = nullptr;
    QLabel* event_ = nullptr;
    QPlainTextEdit* received_ = nullptr;
    QPlainTextEdit* transmit_ = nullptr;
    QDoubleSpinBox* center_frequency_ = nullptr;
    QDoubleSpinBox* tx_frequency_ = nullptr;
    QCheckBox* link_offsets_ = nullptr;
    QSlider* waterfall_floor_ = nullptr;
    fectty::WaterfallWidget* waterfall_ = nullptr;
    QSpinBox* ptt_lead_ = nullptr;
    QSpinBox* ptt_tail_ = nullptr;
    QPushButton* send_ = nullptr;
    QPushButton* session_button_ = nullptr;
    QDoubleSpinBox* gain_ = nullptr;
    QSlider* output_volume_ = nullptr;
    QLabel* output_volume_value_ = nullptr;
    QTimer* poll_timer_ = nullptr;
    fectty::AppSettings settings_;
    fectty::RadioController radio_;
    std::future<bool> radio_operation_;
    unsigned radio_revision_ = 0;
    std::chrono::steady_clock::time_point radio_next_read_{};
    QComboBox *hamlib_model_ = nullptr, *omni_number_ = nullptr, *ptt_source_ = nullptr, *radio_mode_ = nullptr;
    QLineEdit* hamlib_device_ = nullptr;
    QSpinBox *hamlib_baud_ = nullptr, *tx_limit_ = nullptr;
    QDoubleSpinBox* radio_frequency_ = nullptr;
    QCheckBox* ptt_armed_ = nullptr;
    QLabel* radio_details_ = nullptr;
    QPushButton *radio_connect_ = nullptr, *radio_disconnect_ = nullptr, *radio_read_ = nullptr, *radio_apply_ = nullptr;
    std::vector<QWidget*> radio_config_widgets_;
    bool copy_radio_on_read_ = false;
    QString settings_file_;
    std::unique_ptr<fectty::IAudioIo> audio_;
    fectty::ReliableReceiver receiver_;
    fectty::GuiTextDecoder text_decoder_;
    std::atomic<double> requested_rx_hz_{1500};
    std::atomic<unsigned> rx_tuning_generation_{0};
    std::atomic<size_t> received_samples_{0};
    std::atomic<bool> rx_stop_{true};
    std::thread rx_thread_;
    std::mutex rx_result_mutex_;
    std::string pending_rx_text_;
    fectty::ReliableRxStats rx_stats_{};
    std::vector<fectty::WaterfallSpectrum::Row> pending_waterfall_;
    size_t waterfall_rows_generated_ = 0;
    double waterfall_peak_hz_ = 0;
    std::atomic<bool> tx_busy_{false};
    std::atomic<bool> tx_success_{false};
    std::atomic<bool> tx_cancel_{false};
    std::atomic<size_t> tx_generated_samples_{0};
    std::thread tx_thread_;
    std::mutex audio_write_mutex_;
    std::atomic<double> tx_scale_{0.5};
    bool quit_after_send_ = false;
    std::mutex tx_error_mutex_;
    std::string tx_error_;
    QString audio_error_shown_;
    QString sent_draft_;
    bool bench_mode_ = false;
    size_t final_captured_ = 0, final_audio_drops_ = 0;
    size_t final_capture_interruptions_ = 0;

    void set_audio_controls_enabled(bool enabled) {
        if (audio_backend_) audio_backend_->setEnabled(enabled);
        if (rx_device_) rx_device_->setEnabled(enabled);
        if (tx_device_) tx_device_->setEnabled(enabled);
        if (audio_refresh_) audio_refresh_->setEnabled(enabled);
    }

    void update_tuning_controls() {
        const bool busy = tx_busy_.load();
        const bool rx_enabled = !busy || !link_offsets_->isChecked();
        center_frequency_->setEnabled(rx_enabled);
        tx_frequency_->setEnabled(!busy);
        link_offsets_->setEnabled(!busy);
        waterfall_->set_tuning_enabled(rx_enabled, !busy);
        waterfall_->set_markers(center_frequency_->value(), tx_frequency_->value());
    }

    void request_rx_tuning() {
        requested_rx_hz_ = center_frequency_->value();
        ++rx_tuning_generation_;
        {
            std::lock_guard lock(rx_result_mutex_);
            pending_rx_text_.clear();
            rx_stats_ = {};
        }
        text_decoder_.reset();
        if (audio_) add_event(QStringLiteral("RX retuned to %1 Hz; waiting for the next burst").arg(center_frequency_->value(), 0, 'f', 1));
    }

    void update_radio_controls() {
        if (!ptt_armed_ || !ptt_lead_) return;
        const auto backend=rig_backend_->currentData().toString();
        const auto status=radio_.status();
        const bool idle=!tx_busy_.load()&&!radio_operation_.valid()&&!status.ptt_owned;
        for(auto* w:radio_config_widgets_)w->setEnabled(idle);
        rig_host_->setEnabled(idle&&backend=="rigctld");rig_port_->setEnabled(idle&&backend=="rigctld");
        hamlib_model_->setEnabled(idle&&backend=="hamlib");hamlib_device_->setEnabled(idle&&backend=="hamlib");
        hamlib_baud_->setEnabled(idle&&backend=="hamlib");omni_number_->setEnabled(idle&&backend=="omnirig");
        ptt_source_->setEnabled(idle&&backend!="none"&&backend!="omnirig");
        radio_connect_->setEnabled(idle&&backend!="none");
        radio_disconnect_->setEnabled(idle&&status.rig.connected);
        radio_read_->setEnabled(idle&&status.rig.connected);
        radio_apply_->setEnabled(idle&&status.rig.connected&&!status.fault&&!status.rig.transmitting);
        ptt_armed_->setEnabled(idle&&backend!="none"&&status.rig.connected&&!status.fault);
        ptt_lead_->setEnabled(idle&&ptt_armed_->isChecked());ptt_tail_->setEnabled(idle&&ptt_armed_->isChecked());
        if(audio_&&!tx_busy_.load())send_->setEnabled(!radio_operation_.valid()&&!status.ptt_owned&&
            (!ptt_armed_->isChecked()||(status.rig.connected&&!status.fault&&!status.ptt_owned)));
    }

    void disconnect_radio_for_reconfiguration(const QString& reason) {
        if(!ptt_armed_)return;
        ptt_armed_->setChecked(false);
        if(radio_.status().rig.connected||radio_operation_.valid()){
            radio_operation_=radio_.disconnect();add_event(reason);
        }
        update_radio_controls();
    }

    void poll_radio() {
        if(radio_operation_.valid()&&radio_operation_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
            const bool ok=radio_operation_.get();
            if(!ok)add_event(QString::fromStdString(radio_.status().message));
            if(copy_radio_on_read_&&ok){
                const auto read=radio_.status().rig;radio_frequency_->setValue(read.frequency_hz/1e6);
                const int index=radio_mode_->findData(int(read.mode));if(index>=0)radio_mode_->setCurrentIndex(index);
            }
            copy_radio_on_read_=false;
            radio_next_read_=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        }
        auto status=radio_.status();
        if(status.revision!=radio_revision_){
            radio_revision_=status.revision;
            rig_state_->setText(status.fault?"CAT fault":status.ptt_owned?"CAT TX":status.rig.connected?"CAT ready":"CAT off");
            radio_details_->setStyleSheet(status.fault?"color: #ffb45e; font-weight: 700":"color: #e7edf5");
            radio_details_->setText(QStringLiteral("%1\n%2 MHz · %3 · %4").arg(QString::fromStdString(status.message))
                .arg(status.rig.frequency_hz/1e6,0,'f',6)
                .arg(status.rig.mode==fectty::RigMode::USB?"USB":status.rig.mode==fectty::RigMode::LSB?"LSB":
                     status.rig.mode==fectty::RigMode::DataUSB?"Data USB":status.rig.mode==fectty::RigMode::DataLSB?"Data LSB":"Unknown mode")
                .arg(status.rig.transmitting?"Radio TX":"Radio RX"));
            if(status.fault){
                if(tx_busy_.load()){tx_cancel_=true;if(audio_)audio_->cancel_write();}
                add_event(QString::fromStdString(status.message));
            }
        }
        if(status.rig.connected&&!status.fault&&!tx_busy_.load()&&!radio_operation_.valid()&&
           std::chrono::steady_clock::now()>=radio_next_read_){
            radio_operation_=radio_.refresh();radio_next_read_=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        }
        update_radio_controls();
    }

    void migrate_legacy_settings() {
        if (QFile::exists(settings_file_)) return;
        const auto legacy = legacy_settings_path();
        if (legacy == settings_file_ || !QFile::exists(legacy)) return;
        QDir().mkpath(QFileInfo(settings_file_).absolutePath());
        QFile::copy(legacy, settings_file_);
    }

    void add_event(const QString& message) {
        event_->setText(message);
        statusBar()->showMessage(message, 5000);
    }

    void refresh_tx_scale() {
        const auto gain = gain_ ? gain_->value() : 0.5;
        const auto volume = output_volume_
                                ? static_cast<double>(output_volume_->value()) /
                                      100.0
                                : 1.0;
        tx_scale_.store(std::clamp(gain * volume, 0.0, 1.0));
    }

    void insert_received_text(const std::string& text) {
        if (text.empty() || !received_) return;
        auto cursor = received_->textCursor();
        cursor.movePosition(QTextCursor::End);
        received_->setTextCursor(cursor);
        received_->insertPlainText(text_decoder_.push(text));
        received_->ensureCursorVisible();
    }

    void receive_worker() {
        bool burst_started = false;
        unsigned generation = std::numeric_limits<unsigned>::max();
        fectty::WaterfallSpectrum analyser;
        auto process_chunk = [this, &burst_started, &generation, &analyser](std::vector<float>& chunk) {
            const unsigned requested_generation = rx_tuning_generation_.load();
            if (generation != requested_generation) {
                auto config = fectty::FskConfig{};
                const double shift = requested_rx_hz_.load() - 1500.0;
                for (auto& tone : config.tones) tone += shift;
                receiver_ = fectty::ReliableReceiver(config);
                burst_started = false;
                generation = requested_generation;
            }
            received_samples_.fetch_add(chunk.size());
            auto rows = analyser.push(chunk);
            if (!rows.empty()) {
                std::lock_guard lock(rx_result_mutex_);
                for (auto& row : rows) {
                    ++waterfall_rows_generated_;
                    const auto peak_bin = std::max_element(row.begin(), row.end());
                    if (*peak_bin > -100) waterfall_peak_hz_ = double(peak_bin-row.begin()) * fectty::WaterfallSpectrum::bin_hz;
                    if (pending_waterfall_.size() >= 32) pending_waterfall_.erase(pending_waterfall_.begin());
                    pending_waterfall_.push_back(std::move(row));
                }
            }
            size_t above_floor = 0;
            float peak = 0.0f;
            for (const auto sample : chunk) {
                const auto magnitude = std::abs(sample);
                peak = std::max(peak, magnitude);
                if (magnitude >= 0.0005f) ++above_floor;
            }
            const bool has_modem_energy = peak >= 0.001f &&
                                          above_floor >= std::max<size_t>(32, chunk.size() / 32);
            // A quiet VB-Audio stream can run indefinitely. Do not make the
            // expensive acquisition search inspect every idle callback. Once
            // a burst starts, however, every following chunk must reach the
            // receiver, including low-energy chunks, or the sample timeline
            // is no longer continuous and a frame boundary can be damaged.
            if (has_modem_energy) burst_started = true;
            if (!burst_started) return;
            const auto text = receiver_.push(std::span<const float>(chunk));
            receiver_.take_frames();
            std::lock_guard lock(rx_result_mutex_);
            if (generation == rx_tuning_generation_.load()) {
                pending_rx_text_ += text;
                rx_stats_ = receiver_.stats();
            }
        };

        while (!rx_stop_.load()) {
            if (!audio_) break;
            auto chunks = audio_->take_input_chunks();
            if (chunks.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            for (auto& chunk : chunks) {
                if (rx_stop_.load()) break;
                process_chunk(chunk);
            }
        }

        const auto final_text = receiver_.finish();
        receiver_.take_frames();
        std::lock_guard lock(rx_result_mutex_);
        if (generation == rx_tuning_generation_.load()) {
            pending_rx_text_ += final_text;
            rx_stats_ = receiver_.stats();
        }
    }

    void update_metrics() {
        fectty::ReliableRxStats stats;
        {
            std::lock_guard lock(rx_result_mutex_);
            stats = rx_stats_;
        }
        frequency_->setText(QStringLiteral("%1 Hz").arg(stats.last_frequency_offset_hz, 0, 'f', 1));
        frames_->setText(QString::number(stats.frames_ok));
        crc_->setText(QString::number(stats.crc_failures));
        gaps_->setText(QString::number(stats.sequence_gaps));
        const auto audio_drops = audio_ ? audio_->dropped_samples() : final_audio_drops_;
        drops_->setText(QString::number(audio_drops + stats.dropped_samples));
        capture_interruptions_->setText(QString::number(audio_ ? audio_->capture_interruptions() : final_capture_interruptions_));
        captured_->setText(QString::number(audio_ ? audio_->captured_samples() : final_captured_));
        processed_->setText(QString::number(received_samples_.load()));
        if (!tx_busy_.load() && audio_) {
            state_->setText(QString::fromLatin1(fectty::reliable_rx_state_name(stats.state)));
        } else if (!tx_busy_.load()) {
            state_->setText(QStringLiteral("Standby"));
        }
    }

    void poll_session() {
        poll_radio();
        std::string received_text;
        std::vector<fectty::WaterfallSpectrum::Row> rows;
        {
            std::lock_guard lock(rx_result_mutex_);
            received_text.swap(pending_rx_text_);
            rows.swap(pending_waterfall_);
        }
        for (const auto& row : rows) waterfall_->add_row(row);
        insert_received_text(received_text);
        update_metrics();
        if (audio_) {
            const auto error = QString::fromStdString(audio_->last_error());
            if (!error.isEmpty() && error != audio_error_shown_) {
                audio_error_shown_ = error;
                add_event(error);
            }
        }

        if (tx_thread_.joinable() && !tx_busy_.load()) {
            tx_thread_.join();
            send_->setEnabled(audio_ != nullptr);
            const bool succeeded = tx_success_.load();
            if (succeeded) {
                if (transmit_->toPlainText() == sent_draft_) transmit_->clear();
                add_event(QStringLiteral("TX complete; modem audio sent"));
            } else {
                std::string error;
                {
                    std::lock_guard lock(tx_error_mutex_);
                    error = tx_error_;
                }
                add_event(error.empty()
                              ? QStringLiteral("TX failed")
                              : QStringLiteral("TX failed: %1").arg(QString::fromStdString(error)));
            }
            if (audio_) state_->setText(QStringLiteral("Listening"));
            if (quit_after_send_) {
                QTimer::singleShot(0, this, [succeeded] {
                    QApplication::exit(succeeded ? 0 : 2);
                });
            }
        }
        update_tuning_controls();
    }

    bool start_session() {
        if (audio_) {
            add_event(QStringLiteral("Audio session is already running"));
            return false;
        }
        const auto backend = audio_backend_->currentData().toString();
        const auto input = rx_device_->currentData().toString();
        const auto output = tx_device_->currentData().toString();
        const auto prefix = backend == QStringLiteral("portaudio")
                                ? QStringLiteral("portaudio:")
                                : QStringLiteral("winmm:");
        if ((input != QStringLiteral("none") && !input.startsWith(prefix)) ||
            !output.startsWith(prefix)) {
            add_event(QStringLiteral("Choose an RX input and TX output for the selected audio backend"));
            return false;
        }

        std::unique_ptr<fectty::IAudioIo> candidate;
        if (backend == QStringLiteral("winmm")) {
            candidate = std::make_unique<fectty::WinMmAudioIo>();
        }
#ifdef FECTTY_HAS_PORTAUDIO
        else if (backend == QStringLiteral("portaudio")) {
            candidate = std::make_unique<fectty::PortAudioIo>();
        }
#endif
        if (!candidate) {
            add_event(QStringLiteral("The selected audio backend is not available in this build"));
            return false;
        }
        const bool opened = candidate->open(
            input.toStdString(), output.toStdString(), 48000.0,
            {});
        if (!opened) {
            const auto error = candidate->last_error();
            add_event(error.empty() ? QStringLiteral("Could not start audio")
                                    : QStringLiteral("Could not start audio: %1")
                                          .arg(QString::fromStdString(error)));
            std::cerr << "Audio open failed: " << error << '\n';
            return false;
        }

        received_samples_ = 0;
        final_captured_ = final_audio_drops_ = 0;
        final_capture_interruptions_ = 0;
        auto rx_config = fectty::FskConfig{};
        const double shift = center_frequency_->value() - 1500.0;
        for (auto& tone : rx_config.tones) tone += shift;
        requested_rx_hz_ = center_frequency_->value();
        receiver_ = fectty::ReliableReceiver(rx_config);
        text_decoder_.reset();
        {
            std::lock_guard lock(rx_result_mutex_);
            pending_rx_text_.clear();
            rx_stats_ = {};
            pending_waterfall_.clear();
            waterfall_rows_generated_ = 0;
            waterfall_peak_hz_ = 0;
        }
        waterfall_->clear();
        waterfall_->set_running(true, input != QStringLiteral("none"));
        audio_error_shown_.clear();
        audio_ = std::move(candidate);
        refresh_tx_scale();
        set_audio_controls_enabled(false);
        rx_stop_ = false;
        tx_cancel_ = false;
        rx_thread_ = std::thread([this] { receive_worker(); });
        session_button_->setText(QStringLiteral("Stop session"));
        send_->setEnabled(true);
        audio_state_->setText(input == QStringLiteral("none")
                                  ? QStringLiteral("TX only")
                                  : QStringLiteral("Running"));
        state_->setText(QStringLiteral("Search"));
        add_event(QStringLiteral("%1 modem session started; PTT requires explicit arming")
                      .arg(backend == QStringLiteral("portaudio") ? QStringLiteral("PortAudio")
                                                                    : QStringLiteral("WinMM")));
        return true;
    }

    void stop_session() {
        tx_cancel_ = true;
        if (audio_) audio_->cancel_write();
        if (tx_thread_.joinable()) {
            tx_thread_.join();
            tx_busy_ = false;
        }
        if(radio_.status().ptt_owned&&!radio_.end_tx().get())
            add_event(QString::fromStdString(radio_.status().message));
        rx_stop_ = true;
        if (rx_thread_.joinable()) rx_thread_.join();
        if (audio_) {
            final_captured_ = audio_->captured_samples();
            final_audio_drops_ = audio_->dropped_samples();
            final_capture_interruptions_ = audio_->capture_interruptions();
            audio_->close();
            audio_.reset();
        }
        std::string received_text;
        {
            std::lock_guard lock(rx_result_mutex_);
            received_text.swap(pending_rx_text_);
        }
        insert_received_text(received_text);
        session_button_->setText(QStringLiteral("Start session"));
        send_->setEnabled(false);
        audio_state_->setText(QStringLiteral("Stopped"));
        waterfall_->set_running(false);
        set_audio_controls_enabled(true);
        if (!tx_busy_.load()) state_->setText(QStringLiteral("Standby"));
        update_metrics();
        update_tuning_controls();
    }

    void send_message() {
        if (!audio_) {
            add_event(QStringLiteral("Start the live audio session before transmitting"));
            return;
        }
        if (tx_busy_.load()) {
            add_event(QStringLiteral("A transmission is already in progress"));
            return;
        }
        const auto text = transmit_->toPlainText();
        if (text.isEmpty()) {
            add_event(QStringLiteral("Enter a message before sending"));
            return;
        }
        const auto utf8 = text.toUtf8();
        const std::string message(utf8.constData(),
                                  static_cast<size_t>(utf8.size()));
        const bool cat=ptt_armed_->isChecked();
        const auto radio=radio_.status();
        if(radio_operation_.valid()||radio.ptt_owned||(cat&&(!radio.rig.connected||radio.fault))){
            add_event("CAT is busy, disconnected or faulted; reconnect before Send");return;
        }
        const auto ptt=ptt_source_->currentData().toString()=="mic"?fectty::PttMode::Mic:
                       ptt_source_->currentData().toString()=="data"?fectty::PttMode::Data:fectty::PttMode::On;
        const int lead=ptt_lead_->value(),tail=ptt_tail_->value(),limit=tx_limit_->value();
        const double duration=0.6+(8.0*message.size()+54.0*((message.size()+7)/8))/50.0;
        if(cat&&duration+(lead+tail)/1000.0+3.0>limit){
            add_event("Message exceeds the CAT TX time limit; shorten it or increase the limit");return;
        }
        tx_cancel_=false;
        tx_generated_samples_=0;
        refresh_tx_scale();
        tx_success_ = false;
        if (tx_thread_.joinable()) tx_thread_.join();
        sent_draft_ = text;
        {
            std::lock_guard lock(tx_error_mutex_);
            tx_error_.clear();
        }
        tx_busy_ = true;
        update_tuning_controls();
        send_->setEnabled(false);
        state_->setText(QStringLiteral("Transmitting"));
        add_event(QStringLiteral("Sending %1 bytes as direct FEC-RTTY")
                      .arg(utf8.size()));
        auto tx_config = fectty::FskConfig{};
        for (auto& tone : tx_config.tones) tone += tx_frequency_->value() - 1500.0;
        tx_thread_ = std::thread([this, message, tx_config, cat, ptt, lead, tail, limit] {
            std::string error;
            auto cancelled=[this,cat]{return tx_cancel_.load()||(cat&&radio_.status().fault);};
            auto delay=[&](int ms){
                const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);
                while(std::chrono::steady_clock::now()<end&&!cancelled())std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return !cancelled();
            };
            try {
            if(cat){
                if(!radio_.begin_tx(ptt,std::chrono::seconds(limit)).get())
                    throw std::runtime_error(radio_.status().message);
                if(!delay(lead))throw std::runtime_error("Transmission cancelled before audio");
            }
            fectty::ModemTransmitStream stream(message, tx_config);
            if (cancelled()) {
                error = "Transmission cancelled";
            } else {
                std::lock_guard write_lock(audio_write_mutex_);
                bool tail_sent = false;
                const bool written = audio_ && audio_->write_stream([this, &stream, &tail_sent, &cancelled] {
                    if (cancelled()) return std::vector<float>{};
                    auto samples = stream.next();
                    // Drain the output device's final partial hardware block.
                    // This is quiet audio after the burst, not another frame.
                    if (samples.empty() && !tail_sent) {
                        tail_sent = true;
                        tx_generated_samples_.fetch_add(4800);
                        return std::vector<float>(4800, 0.0f);
                    }
                    const auto scale = tx_scale_.load();
                    tx_generated_samples_.fetch_add(samples.size());
                    for (auto& sample : samples) sample = static_cast<float>(sample * scale);
                    return samples;
                });
                if (!written || cancelled()) {
                    error = audio_ ? audio_->last_error() : "Audio output failed";
                    if (error.empty()) error = cancelled() ? "Transmission cancelled" : "Audio output failed";
                }
            }
            if(cat&&error.empty()&&!delay(tail))error="Transmission cancelled during PTT tail";
            } catch (const std::exception& failure) {
                error = failure.what();
            } catch (...) {
                error = "Unexpected transmission failure";
            }
            if(cat){
                const bool released=radio_.end_tx().get();
                const auto status=radio_.status();
                if(!released||status.fault)error=status.message;
            }
            tx_success_ = error.empty();
            if (!tx_success_.load()) {
                std::lock_guard lock(tx_error_mutex_);
                tx_error_ = error.empty() ? "Transmission failed" : std::move(error);
            }
            tx_busy_ = false;
        });
    }

    QWidget* make_header() {
        auto* header = new QFrame;
        header->setObjectName(QStringLiteral("header"));
        auto* layout = new QGridLayout(header);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(18);
        auto* brand = new QLabel(QStringLiteral("FEC-RTTY"));
        brand->setObjectName(QStringLiteral("brand"));
        auto* subtitle = new QLabel(QStringLiteral("Reliable digital radio text"));
        subtitle->setObjectName(QStringLiteral("subtitle"));
        subtitle->setWordWrap(true);
        layout->addWidget(brand,0,0);
        layout->addWidget(subtitle,0,1,1,2);
        int pill_column=0;

        auto add_pill = [&](const QString& title, QLabel*& target) {
            auto* box = new QFrame;
            box->setObjectName(QStringLiteral("statusPill"));
            box->setMinimumWidth(104);
            auto* box_layout = new QVBoxLayout(box);
            box_layout->setContentsMargins(12, 7, 12, 7);
            box_layout->setSpacing(1);
            auto* caption = new QLabel(title.toUpper());
            caption->setObjectName(QStringLiteral("pillCaption"));
            target = value_label();
            box_layout->addWidget(caption);
            box_layout->addWidget(target);
            layout->addWidget(box,1,pill_column++);
        };
        add_pill(QStringLiteral("RX"), state_);
        add_pill(QStringLiteral("Audio"), audio_state_);
        add_pill(QStringLiteral("Radio"), rig_state_);
        return header;
    }

    void enumerate_audio_devices() {
        if (audio_) {
            add_event(QStringLiteral("Stop the audio session before refreshing devices"));
            return;
        }
        const auto old_rx = rx_device_->currentData().toString();
        const auto old_tx = tx_device_->currentData().toString();
        const auto old_rx_name = rx_device_->currentText();
        const auto old_tx_name = tx_device_->currentText();
        rx_device_->clear();
        tx_device_->clear();
        const auto backend = audio_backend_->currentData().toString();
        std::vector<fectty::AudioDeviceInfo> devices;
        if (backend == QStringLiteral("winmm")) {
            devices = fectty::WinMmAudioIo::list_devices();
        }
#ifdef FECTTY_HAS_PORTAUDIO
        else if (backend == QStringLiteral("portaudio")) {
            devices = fectty::PortAudioIo::list_devices();
        }
#endif
        for (const auto& device : devices) {
            if (device.input) {
                rx_device_->addItem(QString::fromStdString(device.name),
                                    QString::fromStdString(device.id));
            }
            if (device.output) {
                tx_device_->addItem(QString::fromStdString(device.name),
                                    QString::fromStdString(device.id));
            }
        }
        rx_device_->addItem(QStringLiteral("No RX (TX-only bench)"), QStringLiteral("none"));
        if (rx_device_->count() == 0) rx_device_->addItem(QStringLiteral("No RX devices found"));
        if (tx_device_->count() == 0) tx_device_->addItem(QStringLiteral("No TX devices found"));
        const auto restore = [&devices](QComboBox* combo, const QString& id, const QString& name, bool input) {
            const auto resolved = fectty::resolve_saved_audio_device(devices, id.toStdString(), name.toStdString(), input);
            const auto index = resolved.empty() ? -1 : combo->findData(QString::fromStdString(resolved));
            if (index >= 0) combo->setCurrentIndex(index);
            return index >= 0;
        };
        const bool rx_restored = restore(rx_device_, old_rx, old_rx_name, true);
        const bool tx_restored = restore(tx_device_, old_tx, old_tx_name, false);
        const bool rx_missing = !rx_restored && old_rx.startsWith(backend + ':') && !old_rx_name.isEmpty();
        const bool tx_missing = !tx_restored && old_tx.startsWith(backend + ':') && !old_tx_name.isEmpty();
        if (rx_missing) rx_device_->setCurrentIndex(-1);
        if (tx_missing) tx_device_->setCurrentIndex(-1);
        if (!rx_restored && !rx_missing) {
            for (int i = 0; i < rx_device_->count(); ++i) {
                if (rx_device_->itemText(i).contains(QStringLiteral("CABLE Output"),
                                                     Qt::CaseInsensitive)) {
                    rx_device_->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (!tx_restored && !tx_missing) {
            for (int i = 0; i < tx_device_->count(); ++i) {
                if (tx_device_->itemText(i).contains(QStringLiteral("CABLE Input"),
                                                     Qt::CaseInsensitive)) {
                    tx_device_->setCurrentIndex(i);
                    break;
                }
            }
        }
        add_event(rx_missing || tx_missing
            ? QStringLiteral("A selected audio device is missing or ambiguous; select it again")
            : QStringLiteral("Audio device list refreshed"));
    }

    QWidget* make_audio_page() {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);
        auto* form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        audio_backend_ = new QComboBox;
        audio_backend_->addItem(QStringLiteral("Windows Audio (WinMM)"), QStringLiteral("winmm"));
#ifdef FECTTY_HAS_PORTAUDIO
        audio_backend_->addItem(QStringLiteral("PortAudio"), QStringLiteral("portaudio"));
#else
        audio_backend_->addItem(QStringLiteral("PortAudio (not available in this build)"),
                                QStringLiteral("portaudio"));
        audio_backend_->setItemData(audio_backend_->count() - 1, false, Qt::UserRole - 1);
#endif
        rx_device_ = new QComboBox;
        tx_device_ = new QComboBox;
        form->addRow(QStringLiteral("Backend"), audio_backend_);
        form->addRow(QStringLiteral("RX input"), rx_device_);
        form->addRow(QStringLiteral("TX output"), tx_device_);
        layout->addLayout(form);

        auto* format = new QGroupBox(QStringLiteral("Modem audio format"));
        auto* format_layout = new QFormLayout(format);
        auto* rate = new QLabel(QStringLiteral("48,000 Hz / mono / 16-bit"));
        rate->setObjectName(QStringLiteral("fixedValue"));
        gain_ = new QDoubleSpinBox;
        gain_->setRange(0.05, 1.0);
        gain_->setSingleStep(0.05);
        gain_->setValue(0.5);
        gain_->setSuffix(QStringLiteral(" ×"));
        output_volume_ = new QSlider(Qt::Horizontal);
        output_volume_->setRange(0, 100);
        output_volume_->setValue(100);
        output_volume_->setTickInterval(10);
        output_volume_->setAccessibleName(QStringLiteral("TX output volume"));
        output_volume_->setToolTip(QStringLiteral(
            "Final audio level sent to the selected output. Reduce this to avoid overdriving the radio."));
        output_volume_value_ = new QLabel(QStringLiteral("100%"));
        output_volume_value_->setObjectName(QStringLiteral("fixedValue"));
        output_volume_value_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        output_volume_value_->setMinimumWidth(46);
        auto* output_volume_row = new QWidget;
        auto* output_volume_layout = new QHBoxLayout(output_volume_row);
        output_volume_layout->setContentsMargins(0, 0, 0, 0);
        output_volume_layout->setSpacing(8);
        output_volume_layout->addWidget(output_volume_, 1);
        output_volume_layout->addWidget(output_volume_value_);
        connect(output_volume_, &QSlider::valueChanged, this, [this](int value) {
            output_volume_value_->setText(QStringLiteral("%1%").arg(value));
            refresh_tx_scale();
        });
        connect(gain_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this](double) { refresh_tx_scale(); });
        format_layout->addRow(QStringLiteral("Required format"), rate);
        format_layout->addRow(QStringLiteral("Modem TX gain"), gain_);
        format_layout->addRow(QStringLiteral("Output volume"), output_volume_row);
        layout->addWidget(format);

        auto* buttons = new QHBoxLayout;
        session_button_ = new QPushButton(QStringLiteral("Start session"));
        session_button_->setObjectName(QStringLiteral("primaryButton"));
        audio_refresh_ = new QPushButton(QStringLiteral("Refresh devices"));
        auto* test = new QPushButton(QStringLiteral("Test selection"));
        buttons->addWidget(session_button_);
        buttons->addWidget(audio_refresh_);
        buttons->addWidget(test);
        buttons->addStretch();
        layout->addLayout(buttons);
        layout->addStretch();
        connect(session_button_, &QPushButton::clicked, this, [this] {
            if (audio_) stop_session();
            else start_session();
        });
        connect(audio_backend_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this] { enumerate_audio_devices(); });
        connect(audio_refresh_, &QPushButton::clicked, this, [this] { enumerate_audio_devices(); });
        connect(test, &QPushButton::clicked, this, [this] {
            if (rx_device_->currentData().toString().isEmpty() ||
                tx_device_->currentData().toString().isEmpty()) {
                add_event(QStringLiteral("Choose both an RX input and TX output first"));
                return;
            }
            add_event(QStringLiteral("Audio devices selected; Start session checks 48 kHz format and availability"));
        });
        enumerate_audio_devices();
        return page;
    }

    QWidget* make_radio_page() {
        auto* page=new QWidget;auto* layout=new QVBoxLayout(page);layout->setContentsMargins(14,14,14,14);
        auto* form=new QFormLayout;form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        rig_backend_=new QComboBox;rig_backend_->addItem("NullRig / audio only","none");
#ifdef FECTTY_HAS_HAMLIB
        rig_backend_->addItem("Hamlib / direct serial or USB","hamlib");
#endif
        rig_backend_->addItem("Hamlib / rigctld TCP","rigctld");
#ifdef _WIN32
        rig_backend_->addItem("OmniRig","omnirig");
#endif
        hamlib_model_=new QComboBox;hamlib_model_->addItem("Select your radio",0);
#ifdef FECTTY_HAS_HAMLIB
        for(const auto& [id,name]:fectty::available_hamlib_models())hamlib_model_->addItem(QString::fromStdString(name),id);
#endif
        hamlib_model_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);hamlib_model_->setMinimumContentsLength(12);
        hamlib_device_=new QLineEdit;hamlib_device_->setPlaceholderText("COM port, e.g. COM4");
        hamlib_baud_=new QSpinBox;hamlib_baud_->setRange(300,115200);hamlib_baud_->setValue(9600);
        rig_host_=new QLineEdit("127.0.0.1");
        rig_host_->setToolTip("Numeric IPv4/IPv6 address or localhost. rigctld must already be running.");
        rig_port_=new QSpinBox;rig_port_->setRange(1,65535);rig_port_->setValue(4532);
        omni_number_=new QComboBox;omni_number_->addItem("Rig 1",1);omni_number_->addItem("Rig 2",2);
        radio_frequency_=new QDoubleSpinBox;radio_frequency_->setRange(0.000001,2147.483647);
        radio_frequency_->setDecimals(6);radio_frequency_->setSuffix(" MHz");radio_frequency_->setValue(14.080);
        radio_frequency_->setKeyboardTracking(false);
        radio_frequency_->setToolTip("RF dial frequency. Changes the radio only when Apply is pressed. Not the waterfall audio offset.");
        radio_mode_=new QComboBox;
        radio_mode_->addItem("USB",int(fectty::RigMode::USB));radio_mode_->addItem("LSB",int(fectty::RigMode::LSB));
        radio_mode_->addItem("Data USB",int(fectty::RigMode::DataUSB));radio_mode_->addItem("Data LSB",int(fectty::RigMode::DataLSB));
        ptt_source_=new QComboBox;ptt_source_->addItem("Radio default","on");ptt_source_->addItem("Microphone input","mic");ptt_source_->addItem("Data input","data");
        tx_limit_=new QSpinBox;tx_limit_->setRange(1,600);tx_limit_->setValue(120);tx_limit_->setSuffix(" s");
        tx_limit_->setToolTip("Maximum keyed time. Long messages are rejected before keying; watchdog cancels on expiry.");
        form->addRow("Control method",rig_backend_);form->addRow("Radio model",hamlib_model_);
        form->addRow("Serial / USB port",hamlib_device_);form->addRow("Baud rate",hamlib_baud_);
        form->addRow("TCP host",rig_host_);form->addRow("TCP port",rig_port_);form->addRow("OmniRig slot",omni_number_);
        layout->addLayout(form);
        auto* connections=new QHBoxLayout;
        radio_connect_=new QPushButton("Connect");radio_disconnect_=new QPushButton("Disconnect");radio_read_=new QPushButton("Read");
        for(auto* w:{radio_connect_,radio_disconnect_,radio_read_})connections->addWidget(w);
        layout->addLayout(connections);
        auto* dial=new QFormLayout;dial->setRowWrapPolicy(QFormLayout::WrapLongRows);
        dial->addRow("RF dial",radio_frequency_);dial->addRow("Radio mode",radio_mode_);
        radio_apply_=new QPushButton("Apply dial + mode");dial->addRow(radio_apply_);
        dial->addRow("PTT source",ptt_source_);dial->addRow("TX time limit",tx_limit_);layout->addLayout(dial);
        ptt_armed_=new QCheckBox("Arm CAT PTT for Send");
        ptt_armed_->setToolTip("Off on every launch. When off, Send outputs audio only; VOX or external software can still key your radio.");
        layout->addWidget(ptt_armed_);
        auto* emergency=new QPushButton("Stop TX / force PTT OFF");
        emergency->setStyleSheet("QPushButton { background: #743a30; color: #ffffff; border-color: #ffb45e; }");
        radio_details_=new QLabel("Radio disconnected — PTT not armed");radio_details_->setWordWrap(true);
        radio_details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* note=new QLabel("Connect reads status only. Read copies the current dial/mode into the fields. Apply changes the radio explicitly. OmniRig must be installed/configured separately and uses generic PTT. Turn VOX off when testing audio only. If release is not confirmed, unkey the radio manually.");
        note->setWordWrap(true);note->setObjectName("helpText");layout->addWidget(note);layout->addStretch();
        radio_config_widgets_={rig_backend_,hamlib_model_,hamlib_device_,hamlib_baud_,rig_host_,rig_port_,
                               omni_number_,radio_frequency_,radio_mode_,ptt_source_,tx_limit_};
        auto changed=[this]{disconnect_radio_for_reconfiguration("Radio connection settings changed; reconnect before arming PTT");};
        connect(rig_backend_,qOverload<int>(&QComboBox::currentIndexChanged),this,changed);
        connect(hamlib_model_,qOverload<int>(&QComboBox::currentIndexChanged),this,changed);
        connect(hamlib_device_,&QLineEdit::editingFinished,this,changed);
        connect(hamlib_baud_,qOverload<int>(&QSpinBox::valueChanged),this,changed);
        connect(rig_host_,&QLineEdit::editingFinished,this,changed);
        connect(rig_port_,qOverload<int>(&QSpinBox::valueChanged),this,changed);
        connect(omni_number_,qOverload<int>(&QComboBox::currentIndexChanged),this,changed);
        connect(radio_connect_,&QPushButton::clicked,this,[this]{connect_radio_backend();});
        connect(radio_disconnect_,&QPushButton::clicked,this,[this]{
            ptt_armed_->setChecked(false);radio_operation_=radio_.disconnect();update_radio_controls();
        });
        connect(radio_read_,&QPushButton::clicked,this,[this]{
            radio_operation_=radio_.refresh();copy_radio_on_read_=true;update_radio_controls();
        });
        connect(radio_apply_,&QPushButton::clicked,this,[this]{
            radio_operation_=radio_.apply(static_cast<uint64_t>(std::llround(radio_frequency_->value()*1e6)),
                static_cast<fectty::RigMode>(radio_mode_->currentData().toInt()));update_radio_controls();
        });
        connect(ptt_armed_,&QCheckBox::toggled,this,[this](bool armed){
            add_event(armed?"CAT PTT armed — Send can key the radio":"CAT PTT disarmed — audio only");update_radio_controls();
        });
        connect(emergency,&QPushButton::clicked,this,[this]{
            tx_cancel_=true;if(audio_)audio_->cancel_write();
            ptt_armed_->setChecked(false);radio_operation_=radio_.emergency_off();update_radio_controls();
        });
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(page);
        auto* panel=new QWidget;auto* panel_layout=new QVBoxLayout(panel);
        panel_layout->setContentsMargins(8,8,8,8);
        panel_layout->addWidget(radio_details_);panel_layout->addWidget(scroll,1);
        panel_layout->addWidget(emergency);return panel;
    }

    void connect_radio_backend() {
        ptt_armed_->setChecked(false);
        const auto backend=rig_backend_->currentData().toString();
        fectty::RadioController::Factory factory;
        if(backend=="rigctld"){
            const auto host=rig_host_->text().trimmed().toStdString();const auto port=static_cast<uint16_t>(rig_port_->value());
            factory=[host,port]{return std::make_unique<fectty::NetRigctlControl>(host,port);};
        }
#ifdef FECTTY_HAS_HAMLIB
        else if(backend=="hamlib"){
            const int model=hamlib_model_->currentData().toInt(),baud=hamlib_baud_->value();
            auto device=hamlib_device_->text().trimmed().toStdString();
            if(model<=0||(model!=1&&device.empty())){add_event("Select the exact radio model and serial/USB port first");return;}
#ifdef _WIN32
            if(device.starts_with("COM")||device.starts_with("com"))device=R"(\\.\)"+device;
#endif
            factory=[model,device,baud]{return std::make_unique<fectty::HamlibRigControl>(model,device,baud);};
        }
#endif
#ifdef _WIN32
        else if(backend=="omnirig"){
            const int slot=omni_number_->currentData().toInt();factory=[slot]{return std::make_unique<fectty::OmniRigControl>(slot);};
        }
#endif
        else {radio_operation_=radio_.disconnect();return;}
        radio_operation_=radio_.connect(std::move(factory));add_event("Connecting CAT; no PTT or dial write");update_radio_controls();
    }

    QWidget* make_modem_page() {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);
        auto* form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        auto* mode = new QComboBox;
        mode->addItem(QStringLiteral("FEC-RTTY 4-FSK / FEC"));
        ptt_lead_ = new QSpinBox;
        ptt_lead_->setRange(0, 2000);
        ptt_lead_->setValue(settings_.ptt_lead_ms);
        ptt_lead_->setSuffix(QStringLiteral(" ms"));
        ptt_tail_ = new QSpinBox;
        ptt_tail_->setRange(0, 2000);
        ptt_tail_->setValue(settings_.ptt_tail_ms);
        ptt_tail_->setSuffix(QStringLiteral(" ms"));
        ptt_lead_->setEnabled(false);
        ptt_tail_->setEnabled(false);
        ptt_lead_->setToolTip(QStringLiteral("Delay after confirmed PTT ON before modem audio. Only used with armed CAT."));
        ptt_tail_->setToolTip(ptt_lead_->toolTip());
        form->addRow(QStringLiteral("Mode"), mode);
        form->addRow(QStringLiteral("PTT lead"), ptt_lead_);
        form->addRow(QStringLiteral("PTT tail"), ptt_tail_);
        layout->addLayout(form);
        auto* help = new QLabel(QStringLiteral(
            "50 baud, 48 kHz continuous-phase 4-FSK with rate-1/2 convolutional "
            "FEC. Messages are transmitted and decoded directly with no link "
            "establishment, station addressing, ACK wait, or encryption. "
            "Line breaks are preserved. Use the waterfall RX/TX audio-offset "
            "controls to tune the four-tone center; they do not change the radio dial."));
        help->setWordWrap(true);
        help->setObjectName(QStringLiteral("helpText"));
        layout->addWidget(help);
        layout->addStretch();
        return page;
    }

    QDockWidget* make_setup_dock() {
        auto* dock = new QDockWidget(QStringLiteral("Session setup"), this);
        dock->setObjectName(QStringLiteral("setupDock"));
        auto* tabs = new QTabWidget;
        tabs->addTab(make_audio_page(), QStringLiteral("Audio"));
        tabs->addTab(make_radio_page(), QStringLiteral("Radio"));
        tabs->addTab(make_modem_page(), QStringLiteral("Modem"));
        dock->setWidget(tabs);
        return dock;
    }

    QDockWidget* make_diagnostics_dock() {
        auto* dock = new QDockWidget(QStringLiteral("Diagnostics"), this);
        dock->setObjectName(QStringLiteral("diagnosticsDock"));
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        auto* form = new QFormLayout;
        frequency_ = value_label(QStringLiteral("0 Hz"));
        frames_ = value_label(QStringLiteral("0"));
        crc_ = value_label(QStringLiteral("0"));
        gaps_ = value_label(QStringLiteral("0"));
        drops_ = value_label(QStringLiteral("0"));
        capture_interruptions_ = value_label(QStringLiteral("0"));
        captured_ = value_label(QStringLiteral("0"));
        processed_ = value_label(QStringLiteral("0"));
        form->addRow(QStringLiteral("Frequency offset"), frequency_);
        form->addRow(QStringLiteral("Valid frames"), frames_);
        form->addRow(QStringLiteral("CRC failures"), crc_);
        form->addRow(QStringLiteral("Sequence gaps"), gaps_);
        form->addRow(QStringLiteral("Dropped samples"), drops_);
        form->addRow(QStringLiteral("Capture interruptions"), capture_interruptions_);
        form->addRow(QStringLiteral("Captured RX samples"), captured_);
        form->addRow(QStringLiteral("Processed RX samples"), processed_);
        layout->addLayout(form);
        auto* help = new QLabel(QStringLiteral(
            "Live modem telemetry will appear here when the session service is connected."));
        help->setWordWrap(true);
        help->setObjectName(QStringLiteral("helpText"));
        layout->addWidget(help);
        layout->addStretch();
        dock->setWidget(page);
        return dock;
    }

    QWidget* make_waterfall() {
        auto* group = new QGroupBox(QStringLiteral("Signals && audio offsets"));
        auto* layout = new QVBoxLayout(group);
        auto* controls = new QGridLayout;
        center_frequency_ = new QDoubleSpinBox;
        tx_frequency_ = new QDoubleSpinBox;
        for (auto* frequency : {center_frequency_, tx_frequency_}) {
            frequency->setRange(300, 3000);
            frequency->setDecimals(1);
            frequency->setSingleStep(1);
            frequency->setValue(1500);
            frequency->setSuffix(QStringLiteral(" Hz"));
            frequency->setKeyboardTracking(false);
            frequency->setToolTip(QStringLiteral("Center of the four modem tones in audio Hz, not radio RF frequency."));
        }
        center_frequency_->setAccessibleName(QStringLiteral("RX audio offset"));
        tx_frequency_->setAccessibleName(QStringLiteral("TX audio offset"));
        auto* rx_label = new QLabel(QStringLiteral("RX"));
        auto* tx_label = new QLabel(QStringLiteral("TX"));
        rx_label->setStyleSheet(QStringLiteral("color: #7ed7c3; font-weight: 700"));
        tx_label->setStyleSheet(QStringLiteral("color: #ffb45e; font-weight: 700"));
        rx_label->setBuddy(center_frequency_); tx_label->setBuddy(tx_frequency_);
        controls->addWidget(rx_label, 0, 0); controls->addWidget(center_frequency_, 0, 1);
        controls->addWidget(tx_label, 0, 2); controls->addWidget(tx_frequency_, 0, 3);
        controls->setColumnStretch(1, 1); controls->setColumnStretch(3, 1);
        link_offsets_ = new QCheckBox(QStringLiteral("Link RX + TX"));
        link_offsets_->setChecked(true);
        link_offsets_->setToolTip(QStringLiteral("When linked, tuning either marker moves both. Uncheck for split audio offsets."));
        controls->addWidget(link_offsets_, 1, 0, 1, 2);
        controls->addWidget(new QLabel(QStringLiteral("Sensitivity")), 1, 2);
        waterfall_floor_ = new QSlider(Qt::Horizontal);
        waterfall_floor_->setRange(20, 110); waterfall_floor_->setValue(85);
        waterfall_floor_->setAccessibleName(QStringLiteral("Waterfall sensitivity"));
        waterfall_floor_->setToolTip(QStringLiteral("Changes waterfall brightness only; never RX gain or TX volume."));
        controls->addWidget(waterfall_floor_, 1, 3);
        layout->addLayout(controls);
        waterfall_ = new fectty::WaterfallWidget;
        layout->addWidget(waterfall_, 1);
        auto* hint = new QLabel(QStringLiteral("Left: RX · Right/Ctrl: TX · Fields: exact offset"));
        hint->setObjectName(QStringLiteral("helpText")); hint->setWordWrap(true);
        layout->addWidget(hint);
        waterfall_->on_tune = [this](double hz, bool tx) { (tx ? tx_frequency_ : center_frequency_)->setValue(hz); };
        connect(center_frequency_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double hz) {
            if (link_offsets_->isChecked()) { const QSignalBlocker block(tx_frequency_); tx_frequency_->setValue(hz); }
            request_rx_tuning(); update_tuning_controls();
        });
        connect(tx_frequency_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double hz) {
            if (link_offsets_->isChecked()) center_frequency_->setValue(hz);
            update_tuning_controls();
        });
        connect(link_offsets_, &QCheckBox::toggled, this, [this](bool linked) {
            if (linked) tx_frequency_->setValue(center_frequency_->value());
            update_tuning_controls();
        });
        connect(waterfall_floor_, &QSlider::valueChanged, this, [this](int value) { waterfall_->set_floor(-value); });
        return group;
    }

    QWidget* make_conversation() {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(18, 18, 18, 12);
        layout->setSpacing(12);
        layout->addWidget(make_header());
        layout->addWidget(make_waterfall(), 1);
        auto* title = new QLabel(QStringLiteral("Conversation"));
        title->setObjectName(QStringLiteral("sectionTitle"));
        layout->addWidget(title);
        received_ = new QPlainTextEdit;
        received_->setReadOnly(true);
        received_->setPlaceholderText(QStringLiteral("Received text will appear here…"));
        received_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
        layout->addWidget(received_, 1);
        auto* compose_label = new QLabel(QStringLiteral("Transmit message"));
        compose_label->setObjectName(QStringLiteral("sectionTitle"));
        layout->addWidget(compose_label);
        auto* compose = new QHBoxLayout;
        transmit_ = new QPlainTextEdit;
        transmit_->setPlaceholderText(QStringLiteral("Type a message…"));
        transmit_->setMaximumHeight(150);
        transmit_->setTabChangesFocus(true);
        send_ = new QPushButton(QStringLiteral("Send"));
        send_->setObjectName(QStringLiteral("primaryButton"));
        send_->setMinimumWidth(110);
        compose->addWidget(transmit_, 1);
        compose->addWidget(send_);
        layout->addLayout(compose);
        event_ = new QLabel(QStringLiteral("GUI shell ready; select devices and a control method to begin."));
        event_->setObjectName(QStringLiteral("eventLabel"));
        event_->setWordWrap(true);
        layout->addWidget(event_);
        connect(send_, &QPushButton::clicked, this, [this] { send_message(); });
        return page;
    }

    void load_settings() {
        if (!fectty::load_settings(settings_, settings_file_.toStdString()) && QFile::exists(settings_file_)) {
            add_event(QStringLiteral("Saved settings are invalid; using defaults"));
        }
        const auto backend = audio_backend_->findData(QString::fromStdString(settings_.audio_backend));
        if (backend >= 0) audio_backend_->setCurrentIndex(backend);
        enumerate_audio_devices();
        const auto rig = rig_backend_->findData(QString::fromStdString(settings_.rig_backend));
        if (rig >= 0) rig_backend_->setCurrentIndex(rig);
        rig_host_->setText(QString::fromStdString(settings_.rig_host));
        rig_port_->setValue(settings_.rig_port);
        const int model=hamlib_model_->findData(settings_.hamlib_model);if(model>=0)hamlib_model_->setCurrentIndex(model);
        hamlib_device_->setText(QString::fromStdString(settings_.hamlib_device));hamlib_baud_->setValue(settings_.hamlib_baud);
        omni_number_->setCurrentIndex(settings_.omnirig_number-1);tx_limit_->setValue(settings_.tx_limit_seconds);
        ptt_source_->setCurrentIndex(ptt_source_->findData(QString::fromStdString(settings_.ptt_source)));
        ptt_armed_->setChecked(false);
        const auto restore_named = [this](QComboBox* combo, const std::string& id, const std::string& name) {
            if (id == "none") { combo->setCurrentIndex(combo->findData(QStringLiteral("none"))); return; }
            // OS/PortAudio indices can move when endpoints change. Never use
            // a saved numeric ID if its remembered device name differs.
            if (name.empty()) {
                if (!id.empty()) add_event(QStringLiteral("Legacy device IDs were not restored; check the displayed audio selections"));
                return;
            }
            std::vector<fectty::AudioDeviceInfo> devices;
            const bool input = combo == rx_device_;
            for (int i = 0; i < combo->count(); ++i)
                devices.push_back({combo->itemData(i).toString().toStdString(), combo->itemText(i).toStdString(), input, !input});
            const auto resolved = fectty::resolve_saved_audio_device(devices, id, name, input);
            const auto index = resolved.empty() ? -1 : combo->findData(QString::fromStdString(resolved));
            combo->setCurrentIndex(index);
            if (index < 0) add_event(QStringLiteral("A saved audio device is unavailable; select it again"));
        };
        restore_named(rx_device_, settings_.audio_input, settings_.audio_input_name);
        restore_named(tx_device_, settings_.audio_output, settings_.audio_output_name);
        if (center_frequency_) center_frequency_->setValue(settings_.center_hz);
        if (link_offsets_) link_offsets_->setChecked(false);
        if (tx_frequency_) tx_frequency_->setValue(settings_.tx_center_hz);
        if (link_offsets_) link_offsets_->setChecked(settings_.link_offsets);
        if (waterfall_floor_) waterfall_floor_->setValue(-settings_.waterfall_floor_dbfs);
        if (ptt_lead_) ptt_lead_->setValue(settings_.ptt_lead_ms);
        if (ptt_tail_) ptt_tail_->setValue(settings_.ptt_tail_ms);
        if (output_volume_) {
            const auto volume = std::isfinite(settings_.output_volume)
                                    ? std::clamp(settings_.output_volume, 0.0, 1.0)
                                    : 1.0;
            output_volume_->setValue(static_cast<int>(std::lround(volume * 100.0)));
        }
    }

    void save_settings() const {
        if (bench_mode_) return;
        auto copy = settings_;
        copy.audio_backend = audio_backend_->currentData().toString().toStdString();
        copy.rig_backend = rig_backend_->currentData().toString().toStdString();
        copy.rig_host = rig_host_->text().toStdString();
        copy.rig_port = static_cast<uint16_t>(rig_port_->value());
        copy.hamlib_model=hamlib_model_->currentData().toInt();copy.hamlib_device=hamlib_device_->text().toStdString();
        copy.hamlib_baud=hamlib_baud_->value();copy.omnirig_number=omni_number_->currentData().toInt();
        copy.tx_limit_seconds=tx_limit_->value();copy.ptt_source=ptt_source_->currentData().toString().toStdString();
        copy.audio_input = rx_device_->currentData().toString().toStdString();
        copy.audio_output = tx_device_->currentData().toString().toStdString();
        copy.audio_input_name = rx_device_->currentText().toStdString();
        copy.audio_output_name = tx_device_->currentText().toStdString();
        if (center_frequency_) copy.center_hz = center_frequency_->value();
        if (tx_frequency_) copy.tx_center_hz = tx_frequency_->value();
        copy.link_offsets = link_offsets_->isChecked();
        copy.waterfall_floor_dbfs = -waterfall_floor_->value();
        if (ptt_lead_) copy.ptt_lead_ms = ptt_lead_->value();
        if (ptt_tail_) copy.ptt_tail_ms = ptt_tail_->value();
        if (output_volume_) copy.output_volume = output_volume_->value() / 100.0;
        if (!fectty::save_settings(copy, settings_file_.toStdString())) {
            std::cerr << "Could not save application settings\n";
        }
    }

    bool bench_radio_requested_=false,bench_arm_requested_=false;
    uint64_t bench_apply_frequency_=0;
    fectty::RigMode bench_apply_mode_=fectty::RigMode::USB;

    void begin_bench_audio(const QString& message,std::chrono::steady_clock::time_point deadline){
        if(radio_operation_.valid()){
            if(std::chrono::steady_clock::now()>deadline){add_event("CAT preparation timed out");QApplication::exit(2);return;}
            QTimer::singleShot(50,this,[this,message,deadline]{begin_bench_audio(message,deadline);});return;
        }
        if(bench_radio_requested_&&(!radio_.status().rig.connected||radio_.status().fault)){QApplication::exit(2);return;}
        if(bench_apply_frequency_){
            radio_operation_=radio_.apply(bench_apply_frequency_,bench_apply_mode_);bench_apply_frequency_=0;
            QTimer::singleShot(50,this,[this,message,deadline]{begin_bench_audio(message,deadline);});return;
        }
        ptt_armed_->setChecked(bench_arm_requested_);
        if(!start_session()){QApplication::exit(2);return;}
        if(!message.isEmpty())QTimer::singleShot(1500,this,[this,message]{transmit_->setPlainText(message);send_message();});
    }

public:
    void show_radio_page(){findChild<QTabWidget*>()->setCurrentIndex(1);}
    bool configure_bench_radio(const fectty::AppSettings& r,bool arm,uint64_t frequency,fectty::RigMode mode){
        const int backend=rig_backend_->findData(QString::fromStdString(r.rig_backend));
        if(backend<0||(arm&&r.rig_backend=="none"))return false;
        rig_backend_->setCurrentIndex(backend);rig_host_->setText(QString::fromStdString(r.rig_host));rig_port_->setValue(r.rig_port);
        const int model=hamlib_model_->findData(r.hamlib_model);
        if(r.rig_backend=="hamlib"&&model<0)return false;
        if(model>=0)hamlib_model_->setCurrentIndex(model);
        hamlib_device_->setText(QString::fromStdString(r.hamlib_device));hamlib_baud_->setValue(r.hamlib_baud);
        omni_number_->setCurrentIndex(r.omnirig_number-1);tx_limit_->setValue(r.tx_limit_seconds);
        ptt_source_->setCurrentIndex(ptt_source_->findData(QString::fromStdString(r.ptt_source)));
        ptt_lead_->setValue(r.ptt_lead_ms);ptt_tail_->setValue(r.ptt_tail_ms);
        bench_radio_requested_=r.rig_backend!="none";bench_arm_requested_=arm;
        bench_apply_frequency_=frequency;bench_apply_mode_=mode;
        if(bench_radio_requested_)connect_radio_backend();
        return true;
    }
    bool transmission_succeeded() const { return tx_success_.load(); }
    bool save_snapshot(const QString& path) { return grab().save(path); }
    void retune_rx(double hz) { center_frequency_->setValue(hz); }
    void set_quit_after_send(bool enabled) { quit_after_send_ = enabled; }
    void set_bench_parameters(double center, int volume, double rx_center=0, double tx_center=0) {
        if (center > 0) { link_offsets_->setChecked(true); center_frequency_->setValue(center); }
        if (rx_center > 0 || tx_center > 0) {
            link_offsets_->setChecked(false);
            if (rx_center > 0) center_frequency_->setValue(rx_center);
            if (tx_center > 0) tx_frequency_->setValue(tx_center);
        }
        if (volume >= 0) output_volume_->setValue(volume);
    }

    void prepare_bench(const QString& input, const QString& output, const QString& message) {
        const auto backend_id = input.startsWith(QStringLiteral("portaudio:")) ||
                                        output.startsWith(QStringLiteral("portaudio:"))
                                    ? QStringLiteral("portaudio")
                                    : QStringLiteral("winmm");
        const auto backend = audio_backend_->findData(backend_id);
        if (backend >= 0) audio_backend_->setCurrentIndex(backend);
        const auto rx = rx_device_->findData(input);
        const auto tx = tx_device_->findData(output);
        if ((!input.isEmpty() && rx < 0) || (!output.isEmpty() && tx < 0)) {
            std::cerr << "Requested audio device is unavailable\n";
            add_event(QStringLiteral("Requested audio device is unavailable"));
            QTimer::singleShot(0, this, [] { QApplication::exit(2); });
            return;
        }
        if (rx >= 0) rx_device_->setCurrentIndex(rx);
        if (tx >= 0) tx_device_->setCurrentIndex(tx);
        QTimer::singleShot(750, this, [this, message] {
            begin_bench_audio(message,std::chrono::steady_clock::now()+std::chrono::seconds(15));
        });
    }

    bool finish_bench(const QString& report_path) {
        stop_session();
        if (report_path.isEmpty()) return true;
        QJsonObject report;
        report[QStringLiteral("received_text")] = received_->toPlainText();
        report[QStringLiteral("draft_text")] = transmit_->toPlainText();
        report[QStringLiteral("tx_success")] = tx_success_.load();
        report["tx_generated_samples"]=static_cast<qint64>(tx_generated_samples_.load());
        const auto radio=radio_.status();
        report["rig_backend"]=rig_backend_->currentData().toString();report["cat_armed"]=ptt_armed_->isChecked();
        report["cat_connected"]=radio.rig.connected;report["cat_ptt_owned"]=radio.ptt_owned;
        report["cat_transmitting"]=radio.rig.transmitting;report["cat_fault"]=radio.fault;
        report["cat_message"]=QString::fromStdString(radio.message);
        report[QStringLiteral("tx_error")] = QString::fromStdString(tx_error_);
        report[QStringLiteral("frames_ok")] = static_cast<qint64>(rx_stats_.frames_ok);
        report[QStringLiteral("crc_failures")] = static_cast<qint64>(rx_stats_.crc_failures);
        report[QStringLiteral("sequence_gaps")] = static_cast<qint64>(rx_stats_.sequence_gaps);
        report[QStringLiteral("acquisitions")] = static_cast<qint64>(rx_stats_.acquisitions);
        report[QStringLiteral("dropped_samples")] = static_cast<qint64>(final_audio_drops_ + rx_stats_.dropped_samples);
        report[QStringLiteral("capture_interruptions")] = static_cast<qint64>(final_capture_interruptions_);
        report[QStringLiteral("captured_samples")] = static_cast<qint64>(final_captured_);
        report[QStringLiteral("processed_samples")] = static_cast<qint64>(received_samples_.load());
        report[QStringLiteral("samples_buffered")] = static_cast<qint64>(rx_stats_.samples_buffered);
        report[QStringLiteral("receive_state")] = QString::fromLatin1(fectty::reliable_rx_state_name(rx_stats_.state));
        report[QStringLiteral("center_hz")] = center_frequency_->value();
        report[QStringLiteral("tx_center_hz")] = tx_frequency_->value();
        report[QStringLiteral("rx_device")] = rx_device_->currentData().toString();
        report[QStringLiteral("tx_device")] = tx_device_->currentData().toString();
        report[QStringLiteral("rx_device_name")] = rx_device_->currentText();
        report[QStringLiteral("tx_device_name")] = tx_device_->currentText();
        report[QStringLiteral("link_offsets")] = link_offsets_->isChecked();
        report[QStringLiteral("waterfall_rows")] = static_cast<qint64>(waterfall_rows_generated_);
        report[QStringLiteral("waterfall_peak_hz")] = waterfall_peak_hz_;
        report[QStringLiteral("event")] = event_->text();
        QSaveFile file(report_path);
        const auto bytes = QJsonDocument(report).toJson();
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }

    MainWindow(bool bench_mode=false) : settings_file_(settings_path()), bench_mode_(bench_mode) {
        setWindowTitle(QStringLiteral("FEC-RTTY - M0NXD"));
        setMinimumSize(1024, 680);
        resize(1600, 980);
        setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks |
                       QMainWindow::AnimatedDocks);
        setCentralWidget(make_conversation());
        addDockWidget(Qt::LeftDockWidgetArea, make_setup_dock());
        addDockWidget(Qt::RightDockWidgetArea, make_diagnostics_dock());
        poll_timer_ = new QTimer(this);
        poll_timer_->setInterval(50);
        connect(poll_timer_, &QTimer::timeout, this, [this] { poll_session(); });
        poll_timer_->start();
        send_->setEnabled(false);
        state_->setText(QStringLiteral("Standby"));
        audio_state_->setText(QStringLiteral("Not started"));
        rig_state_->setText(QStringLiteral("NullRig"));
        migrate_legacy_settings();
        load_settings();
        update_radio_controls();
        statusBar()->showMessage(QStringLiteral("PTT disarmed — connect and arm explicitly for radio TX"));
    }

    ~MainWindow() override {
        if (poll_timer_) poll_timer_->stop();
        stop_session();
        save_settings();
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        stop_session();
        save_settings();
        QMainWindow::closeEvent(event);
    }
};

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const auto arguments = app.arguments();
    bool auto_start = false;
    bool show_radio=false;
    QString auto_input;
    QString auto_output;
    QString auto_message;
    bool quit_after_send = false;
    QString report_path;
    QString snapshot_path;
    int run_seconds = 0;
    double bench_center = 0;
    double bench_rx_center = 0, bench_tx_center = 0;
    double retune_rx_center = 0;
    int retune_after = 3;
    int bench_volume = -1;
    fectty::AppSettings bench_radio;
    bool cat_arm=false,radio_arguments=false;
    uint64_t cat_frequency=0;
    fectty::RigMode cat_mode=fectty::RigMode::USB;
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        if (argument == "--autostart") {
            auto_start = true;
        } else if(argument=="--show-radio"){
            show_radio=true;
        } else if (argument == "--rx" && i + 1 < arguments.size()) {
            auto_input = arguments[++i];
        } else if (argument == "--tx" && i + 1 < arguments.size()) {
            auto_output = arguments[++i];
        } else if (argument == "--send" && i + 1 < arguments.size()) {
            auto_message = arguments[++i];
            if (auto_message.isEmpty()) { std::cerr << "Message is empty\n"; return 2; }
            auto_start = true;
        } else if (argument == "--quit-after-send") {
            quit_after_send = true;
        } else if (argument == "--center-hz" && i + 1 < arguments.size()) {
            if (!fectty::parse_number(arguments[++i].toStdString(), bench_center) || bench_center < 300 || bench_center > 3000) {
                std::cerr << "Center must be 300 to 3000 Hz\n"; return 2;
            }
        } else if ((argument == "--rx-center-hz" || argument == "--tx-center-hz") && i + 1 < arguments.size()) {
            auto& target = argument == "--rx-center-hz" ? bench_rx_center : bench_tx_center;
            if (!fectty::parse_number(arguments[++i].toStdString(), target) || target < 300 || target > 3000) {
                std::cerr << "Audio offset must be 300 to 3000 Hz\n"; return 2;
            }
        } else if (argument == "--retune-rx-hz" && i + 1 < arguments.size()) {
            if (!fectty::parse_number(arguments[++i].toStdString(), retune_rx_center) || retune_rx_center < 300 || retune_rx_center > 3000) {
                std::cerr << "Retune offset must be 300 to 3000 Hz\n"; return 2;
            }
        } else if (argument == "--retune-after" && i + 1 < arguments.size()) {
            if (!fectty::parse_number(arguments[++i].toStdString(), retune_after) || retune_after < 1 || retune_after > 3600) {
                std::cerr << "Retune delay must be 1 to 3600 seconds\n"; return 2;
            }
        } else if (argument == "--volume" && i + 1 < arguments.size()) {
            if (!fectty::parse_number(arguments[++i].toStdString(), bench_volume) || bench_volume < 0 || bench_volume > 100) {
                std::cerr << "Volume must be 0 to 100 percent\n"; return 2;
            }
        } else if (argument == "--run-seconds" && i + 1 < arguments.size()) {
            if (!fectty::parse_number(arguments[++i].toStdString(), run_seconds) ||
                run_seconds < 1 || run_seconds > 3600) {
                std::cerr << "Run duration must be 1 to 3600 seconds\n";
                return 2;
            }
        } else if (argument == "--report" && i + 1 < arguments.size()) {
            report_path = arguments[++i];
            if (report_path.isEmpty()) { std::cerr << "Report path is empty\n"; return 2; }
        } else if (argument == "--snapshot" && i + 1 < arguments.size()) {
            snapshot_path = arguments[++i];
            if (snapshot_path.isEmpty()) { std::cerr << "Snapshot path is empty\n"; return 2; }
        } else if(argument=="--cat-ptt"){
            cat_arm=true;radio_arguments=true;
        } else if((argument=="--rig-backend"||argument=="--rig-host"||argument=="--rig-device"||argument=="--ptt-source")&&i+1<arguments.size()){
            radio_arguments=true;const auto value=arguments[++i].toStdString();
            if(argument=="--rig-backend")bench_radio.rig_backend=value;
            else if(argument=="--rig-host")bench_radio.rig_host=value;
            else if(argument=="--rig-device")bench_radio.hamlib_device=value;
            else {if(value!="on"&&value!="mic"&&value!="data"){std::cerr<<"Invalid PTT source\n";return 2;}bench_radio.ptt_source=value;}
        } else if((argument=="--rig-port"||argument=="--rig-model"||argument=="--rig-baud"||argument=="--omnirig-number"||
                   argument=="--ptt-lead-ms"||argument=="--ptt-tail-ms"||argument=="--tx-limit-seconds")&&i+1<arguments.size()){
            radio_arguments=true;int value=0;
            if(!fectty::parse_number(arguments[++i].toStdString(),value)){std::cerr<<"Invalid CAT number\n";return 2;}
            int low=0,high=2000;
            if(argument=="--rig-port"){low=1;high=65535;}
            if(argument=="--rig-model"){low=1;high=999999;}
            if(argument=="--rig-baud"){low=300;high=115200;}
            if(argument=="--omnirig-number"){low=1;high=2;}
            if(argument=="--tx-limit-seconds"){low=1;high=600;}
            if(value<low||value>high){std::cerr<<"CAT value out of range\n";return 2;}
            if(argument=="--rig-port")bench_radio.rig_port=static_cast<uint16_t>(value);
            else if(argument=="--rig-model")bench_radio.hamlib_model=value;
            else if(argument=="--rig-baud")bench_radio.hamlib_baud=value;
            else if(argument=="--omnirig-number")bench_radio.omnirig_number=value;
            else if(argument=="--ptt-lead-ms")bench_radio.ptt_lead_ms=value;
            else if(argument=="--ptt-tail-ms")bench_radio.ptt_tail_ms=value;
            else bench_radio.tx_limit_seconds=value;
        } else if(argument=="--cat-apply-hz"&&i+1<arguments.size()){
            radio_arguments=true;
            if(!fectty::parse_number(arguments[++i].toStdString(),cat_frequency)||cat_frequency==0||cat_frequency>2147483647ULL)return 2;
        } else if(argument=="--cat-mode"&&i+1<arguments.size()){
            radio_arguments=true;const auto value=arguments[++i];
            if(value=="USB")cat_mode=fectty::RigMode::USB;else if(value=="LSB")cat_mode=fectty::RigMode::LSB;
            else if(value=="PKTUSB")cat_mode=fectty::RigMode::DataUSB;else if(value=="PKTLSB")cat_mode=fectty::RigMode::DataLSB;else return 2;
        } else if(argument=="--version"){
            std::cout<<FECTTY_PROJECT_VERSION<<'\n';return 0;
        } else if (argument == "--help") {
            std::cout << "FEC-RTTY GUI\n"
                      << "  --autostart             start the selected live audio session\n"
                      << "  --rx BACKEND:N          select the RX input (winmm:N or portaudio:N)\n"
                      << "  --tx BACKEND:N          select the TX output (winmm:N or portaudio:N)\n"
                      << "  --quit-after-send       exit with status after an automatic TX\n"
                      << "  --center-hz N           override center frequency for bench run\n"
                      << "  --rx-center-hz N        independent RX audio offset\n"
                      << "  --tx-center-hz N        independent TX audio offset\n"
                      << "  --retune-rx-hz N        retune RX during bench run\n"
                      << "  --retune-after N        retune delay in seconds (default 3)\n"
                      << "  --volume N              override output volume (0 to 100 percent)\n"
                      << "  --run-seconds N         close after N seconds (bench testing)\n"
                      << "  --report PATH           save exact displayed RX and counters as JSON\n"
                      << "  --snapshot PATH         save the GUI image at end of bench run\n"
                      << "  --send TEXT             transmit TEXT after starting\n"
                      << "  --rig-backend NAME      none, hamlib, rigctld or omnirig (bench default none)\n"
                      << "  --rig-model N           Hamlib model ID; --rig-device COMn; --rig-baud N\n"
                      << "  --rig-host IP           rigctld numeric address; --rig-port N (4532)\n"
                      << "  --omnirig-number N      OmniRig slot 1 or 2\n"
                      << "  --cat-ptt               explicitly arm CAT PTT for this run\n"
                      << "  --cat-apply-hz N        explicitly set RF dial + --cat-mode USB|LSB|PKTUSB|PKTLSB\n"
                      << "  --ptt-source NAME       on (radio default), mic or data\n"
                      << "  --ptt-lead-ms N          lead delay; --ptt-tail-ms N (0 to 2000)\n"
                      << "  --tx-limit-seconds N    maximum keyed time (1 to 600, default 120)\n";
            return 0;
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument.toStdString() << '\n';
            return 2;
        }
    }
    if(radio_arguments&&!auto_start){std::cerr<<"CAT CLI options require --autostart or --send\n";return 2;}
    if((cat_arm||cat_frequency)&&bench_radio.rig_backend=="none"){std::cerr<<"CAT action requires a real backend\n";return 2;}
    if (quit_after_send && auto_message.isEmpty()) {
        std::cerr << "--quit-after-send requires --send TEXT\n";
        return 2;
    }
    if (!auto_start && (!auto_input.isEmpty() || !auto_output.isEmpty())) {
        std::cerr << "Audio arguments require --autostart or --send\n";
        return 2;
    }
    QApplication::setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("FEC-RTTY"));
    app.setOrganizationName(QStringLiteral("FEC-RTTY"));

    QPalette palette;
    const auto set_palette_group = [&palette](QPalette::ColorGroup group) {
        palette.setColor(group, QPalette::Window, QColor(QStringLiteral("#111923")));
        palette.setColor(group, QPalette::WindowText, QColor(QStringLiteral("#e7edf5")));
        palette.setColor(group, QPalette::Base, QColor(QStringLiteral("#172331")));
        palette.setColor(group, QPalette::AlternateBase, QColor(QStringLiteral("#1d2c3b")));
        palette.setColor(group, QPalette::ToolTipBase, QColor(QStringLiteral("#26384b")));
        palette.setColor(group, QPalette::ToolTipText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(group, QPalette::Text, QColor(QStringLiteral("#e7edf5")));
        palette.setColor(group, QPalette::Button, QColor(QStringLiteral("#26384b")));
        palette.setColor(group, QPalette::ButtonText, QColor(QStringLiteral("#e7edf5")));
        palette.setColor(group, QPalette::BrightText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(group, QPalette::Highlight, QColor(QStringLiteral("#246d60")));
        palette.setColor(group, QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(group, QPalette::PlaceholderText, QColor(QStringLiteral("#9aaabd")));
        palette.setColor(group, QPalette::Link, QColor(QStringLiteral("#79c8ff")));
        palette.setColor(group, QPalette::LinkVisited, QColor(QStringLiteral("#b48cff")));
    };
    set_palette_group(QPalette::Active);
    set_palette_group(QPalette::Inactive);
    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(QStringLiteral("#111923")));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#aab6c4")));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(QStringLiteral("#1b2735")));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(QStringLiteral("#202f3e")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#aab6c4")));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#1b2735")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#aab6c4")));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QColor(QStringLiteral("#c9d3de")));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(QStringLiteral("#334558")));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(QStringLiteral("#c9d3de")));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(QStringLiteral("#8391a2")));
    app.setPalette(palette);
    app.setStyleSheet(QStringLiteral(
        "QMainWindow { background: #111923; }"
        "QWidget { color: #e7edf5; font-size: 10pt; }"
        "QDockWidget { color: #e7edf5; font-weight: 600; }"
        "QDockWidget::title { background: #1b2735; color: #e7edf5; padding: 9px 12px; border-bottom: 1px solid #334558; }"
        "QDockWidget::close-button, QDockWidget::float-button { background: transparent; border: none; }"
        "QMainWindow::separator { background: #334558; width: 2px; height: 2px; }"
        "QPlainTextEdit, QLineEdit, QComboBox, QAbstractSpinBox { background: #172331; color: #e7edf5; border: 1px solid #334558; border-radius: 6px; padding: 6px; selection-background-color: #246d60; selection-color: #ffffff; }"
        "QPlainTextEdit:focus, QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border: 1px solid #79c8ff; }"
        "QPlainTextEdit:disabled, QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { background: #1b2735; color: #aab6c4; border-color: #2b3948; }"
        "QComboBox::drop-down { width: 28px; border-left: 1px solid #334558; }"
        "QComboBox QAbstractItemView { background: #172331; color: #e7edf5; border: 1px solid #334558; selection-background-color: #246d60; selection-color: #ffffff; outline: none; }"
        "QGroupBox { background: #111923; border: 1px solid #2d4053; border-radius: 8px; margin-top: 12px; padding: 12px 8px 8px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; color: #7ed7c3; background: #111923; }"
        "QPushButton { color: #e7edf5; background: #26384b; border: 1px solid #40566d; border-radius: 6px; padding: 8px 13px; }"
        "QPushButton:hover { background: #304a62; border-color: #5a748e; }"
        "QPushButton:pressed { background: #1d2c3b; }"
        "QPushButton:focus { border: 1px solid #79c8ff; }"
        "QPushButton:disabled { background: #1b2735; color: #aab6c4; border-color: #2b3948; }"
        "QPushButton#primaryButton { color: #f4fbff; background: #246d60; border-color: #61d1b9; font-weight: 700; }"
        "QPushButton#primaryButton:hover { background: #2f705f; }"
        "QPushButton#primaryButton:pressed { background: #1f5f53; }"
        "QPushButton#primaryButton:disabled { background: #1b4b43; color: #aab6c4; border-color: #3b7167; }"
        "QLabel#brand { font-size: 19pt; font-weight: 800; color: #7ed7c3; }"
        "QLabel#subtitle { color: #9aaabd; font-size: 11pt; }"
        "QLabel#sectionTitle { font-size: 12pt; font-weight: 700; color: #ffffff; }"
        "QLabel#helpText, QLabel#eventLabel { color: #9aaabd; }"
        "QLabel#pillCaption { color: #8ca0b5; font-size: 8pt; font-weight: 700; }"
        "QFrame#header { background: #172331; border: 1px solid #2d4053; border-radius: 9px; }"
        "QFrame#statusPill { background: #1d2c3b; border: 1px solid #34495e; border-radius: 6px; }"
        "QLabel#valueLabel, QLabel#fixedValue { color: #7ed7c3; font-weight: 700; }"
        "QTabWidget::pane { background: #111923; border: 1px solid #2d4053; top: -1px; }"
        "QTabBar::tab { background: #172331; color: #b8c5d4; border: 1px solid #2d4053; padding: 8px 13px; margin-right: 2px; }"
        "QTabBar::tab:hover { background: #26384b; color: #ffffff; }"
        "QTabBar::tab:selected { background: #1d2c3b; color: #7ed7c3; border-bottom-color: #7ed7c3; }"
        "QStatusBar { background: #172331; color: #b8c5d4; border-top: 1px solid #2d4053; }"
        "QStatusBar::item { border: none; }"
        "QToolTip { background: #26384b; color: #ffffff; border: 1px solid #61d1b9; padding: 5px; }"
        "QMenu { background: #172331; color: #e7edf5; border: 1px solid #334558; }"
        "QMenu::item:selected { background: #246d60; color: #ffffff; }"
        "QScrollBar:vertical { background: #111923; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #334558; min-height: 28px; border-radius: 6px; }"
        "QScrollBar::handle:vertical:hover { background: #4a627a; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: #111923; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #334558; min-width: 28px; border-radius: 6px; }"
        "QScrollBar::handle:horizontal:hover { background: #4a627a; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"));
    MainWindow window(auto_start || run_seconds > 0 || !report_path.isEmpty() || !snapshot_path.isEmpty() || bench_center > 0 || bench_volume >= 0 || bench_rx_center > 0 || bench_tx_center > 0 || retune_rx_center > 0);
    if(auto_start&&!window.configure_bench_radio(bench_radio,cat_arm,cat_frequency,cat_mode)){
        std::cerr<<"Invalid or unavailable radio backend/model\n";return 2;
    }
    window.set_quit_after_send(quit_after_send);
    window.set_bench_parameters(bench_center, bench_volume, bench_rx_center, bench_tx_center);
    window.show();
    if(show_radio)window.show_radio_page();
    if (auto_start) {
        QTimer::singleShot(0, &window, [&window, auto_input, auto_output, auto_message] {
            window.prepare_bench(auto_input, auto_output, auto_message);
        });
    }
    if (run_seconds > 0) QTimer::singleShot(run_seconds * 1000, &window, [] { QApplication::quit(); });
    if (retune_rx_center > 0) QTimer::singleShot(retune_after * 1000, &window, [&window, retune_rx_center] { window.retune_rx(retune_rx_center); });
    const auto result = app.exec();
    if (!snapshot_path.isEmpty() && !window.save_snapshot(snapshot_path)) {
        std::cerr << "Could not save GUI snapshot\n"; return 2;
    }
    if (!window.finish_bench(report_path)) {
        std::cerr << "Could not save bench report\n";
        return 2;
    }
    // A timer/window close may end the event loop with success while the
    // explicitly requested bench transmission failed, never began or was
    // cancelled during final shutdown. Report that outcome to callers too.
    return fectty::gui_bench_exit_code(result, !auto_message.isEmpty(),
                                      window.transmission_succeeded());
}

#else
int main() { return 0; }
#endif
