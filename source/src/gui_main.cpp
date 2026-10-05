#ifdef FECTTY_HAS_QT

#include "fectty/rig_control.hpp"
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
    std::unique_ptr<fectty::IRigControl> rig_control_;
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

    void disconnect_radio_for_reconfiguration(const QString& reason) {
        const bool was_connected = rig_control_ != nullptr;
        if (rig_control_) {
            rig_control_->disconnect();
            rig_control_.reset();
        }
        rig_state_->setText(rig_backend_->currentData().toString() == QStringLiteral("none")
                                ? QStringLiteral("NullRig")
                                : QStringLiteral("Not connected"));
        if (was_connected) add_event(reason);
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
        add_event(QStringLiteral("Direct %1 modem session started; NullRig/CAT/PTT disabled")
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
        tx_thread_ = std::thread([this, message, tx_config] {
            std::string error;
            try {
            fectty::ModemTransmitStream stream(message, tx_config);
            if (tx_cancel_.load()) {
                error = "Transmission cancelled";
            } else {
                std::lock_guard write_lock(audio_write_mutex_);
                bool tail_sent = false;
                const bool written = audio_ && audio_->write_stream([this, &stream, &tail_sent] {
                    if (tx_cancel_.load()) return std::vector<float>{};
                    auto samples = stream.next();
                    // Drain the output device's final partial hardware block.
                    // This is quiet audio after the burst, not another frame.
                    if (samples.empty() && !tail_sent) {
                        tail_sent = true;
                        return std::vector<float>(4800, 0.0f);
                    }
                    const auto scale = tx_scale_.load();
                    for (auto& sample : samples) sample = static_cast<float>(sample * scale);
                    return samples;
                });
                if (!written || tx_cancel_.load()) {
                    error = audio_ ? audio_->last_error() : "Audio output failed";
                    if (error.empty()) error = tx_cancel_.load() ? "Transmission cancelled" : "Audio output failed";
                }
            }
            } catch (const std::exception& failure) {
                error = failure.what();
            } catch (...) {
                error = "Unexpected transmission failure";
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
        auto* layout = new QHBoxLayout(header);
        layout->setContentsMargins(18, 14, 18, 14);
        layout->setSpacing(18);
        auto* brand = new QLabel(QStringLiteral("FEC-RTTY"));
        brand->setObjectName(QStringLiteral("brand"));
        auto* subtitle = new QLabel(QStringLiteral("Reliable digital radio text"));
        subtitle->setObjectName(QStringLiteral("subtitle"));
        layout->addWidget(brand);
        layout->addWidget(subtitle);
        layout->addStretch();

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
            layout->addWidget(box);
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
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(12);
        auto* form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        rig_backend_ = new QComboBox;
        rig_backend_->addItem(QStringLiteral("NullRig / bench mode"), QStringLiteral("none"));
        rig_backend_->addItem(QStringLiteral("Hamlib / rigctld"), QStringLiteral("rigctld"));
        rig_backend_->addItem(QStringLiteral("OmniRig"), QStringLiteral("omnirig"));
        rig_host_ = new QLineEdit(QStringLiteral("127.0.0.1"));
        rig_port_ = new QSpinBox;
        rig_port_->setRange(1, 65535);
        rig_port_->setValue(4532);
        form->addRow(QStringLiteral("Control method"), rig_backend_);
        form->addRow(QStringLiteral("Host"), rig_host_);
        form->addRow(QStringLiteral("Port"), rig_port_);
        layout->addLayout(form);
        auto* note = new QLabel(QStringLiteral(
            "NullRig is the safe default. CAT/PTT remains disabled in this bench build. Connect tests backend availability."));
        note->setWordWrap(true);
        note->setObjectName(QStringLiteral("helpText"));
        layout->addWidget(note);
        auto* connect_button = new QPushButton(QStringLiteral("Connect / test backend"));
        layout->addWidget(connect_button);
        layout->addStretch();
        connect(rig_backend_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
            disconnect_radio_for_reconfiguration(
                QStringLiteral("Radio backend changed; previous connection closed"));
            const bool network = rig_backend_->currentData().toString() == QStringLiteral("rigctld");
            rig_host_->setEnabled(network);
            rig_port_->setEnabled(network);
        });
        connect(rig_host_, &QLineEdit::editingFinished, this, [this] {
            disconnect_radio_for_reconfiguration(
                QStringLiteral("Radio host changed; previous connection closed"));
        });
        connect(rig_port_, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
            disconnect_radio_for_reconfiguration(
                QStringLiteral("Radio port changed; previous connection closed"));
        });
        connect(connect_button, &QPushButton::clicked, this, [this] {
            connect_radio_backend();
        });
        const bool network = rig_backend_->currentData().toString() == QStringLiteral("rigctld");
        rig_host_->setEnabled(network);
        rig_port_->setEnabled(network);
        return page;
    }

    void connect_radio_backend() {
        if (rig_control_) {
            rig_control_->disconnect();
            rig_control_.reset();
        }

        const auto backend = rig_backend_->currentData().toString();
        if (backend == QStringLiteral("none")) {
            auto candidate = std::make_unique<fectty::NullRigControl>();
            if (candidate->connect()) {
                rig_control_ = std::move(candidate);
                rig_state_->setText(QStringLiteral("NullRig ready"));
                add_event(QStringLiteral("NullRig connected; radio control is simulated"));
            } else {
                rig_state_->setText(QStringLiteral("Not connected"));
                add_event(QStringLiteral("NullRig connection failed"));
            }
            return;
        }

        if (backend == QStringLiteral("rigctld")) {
            const auto host = rig_host_->text().trimmed().toStdString();
            const auto port = static_cast<uint16_t>(rig_port_->value());
            auto candidate = std::make_unique<fectty::NetRigctlControl>(host, port);
            if (candidate->connect()) {
                rig_control_ = std::move(candidate);
                rig_state_->setText(QStringLiteral("rigctld connected"));
                add_event(QStringLiteral("Connected to rigctld at %1:%2; CAT/PTT remains off")
                              .arg(QString::fromStdString(host))
                              .arg(port));
            } else {
                rig_state_->setText(QStringLiteral("Not connected"));
                add_event(QStringLiteral("Could not connect to rigctld at %1:%2")
                              .arg(QString::fromStdString(host))
                              .arg(port));
            }
            return;
        }

#ifdef _WIN32
        if (backend == QStringLiteral("omnirig")) {
            auto candidate = std::make_unique<fectty::OmniRigControl>();
            if (candidate->connect()) {
                rig_control_ = std::move(candidate);
                rig_state_->setText(QStringLiteral("OmniRig connected"));
                add_event(QStringLiteral("Connected to OmniRig Rig 1; CAT/PTT remains off"));
            } else {
                rig_state_->setText(QStringLiteral("Not connected"));
                add_event(QStringLiteral("Could not connect to OmniRig Rig 1"));
            }
            return;
        }
#endif

        rig_state_->setText(QStringLiteral("Not connected"));
        add_event(QStringLiteral("Unknown radio-control backend"));
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
        ptt_lead_->setToolTip(QStringLiteral("CAT/PTT is disabled in this bench build."));
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
        auto* hint = new QLabel(QStringLiteral("Left click/drag: RX · Right or Ctrl click/drag: TX · Numeric fields: exact offset"));
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

public:
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
            if (!start_session()) {
                QTimer::singleShot(0, this, [] { QApplication::exit(2); });
                return;
            }
            if (!message.isEmpty()) {
                QTimer::singleShot(1500, this, [this, message] {
                    transmit_->setPlainText(message);
                    send_message();
                });
            }
        });
    }

    bool finish_bench(const QString& report_path) {
        stop_session();
        if (report_path.isEmpty()) return true;
        QJsonObject report;
        report[QStringLiteral("received_text")] = received_->toPlainText();
        report[QStringLiteral("draft_text")] = transmit_->toPlainText();
        report[QStringLiteral("tx_success")] = tx_success_.load();
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
        statusBar()->showMessage(QStringLiteral("CAT/PTT disabled — NullRig bench mode"));
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
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const auto argument = arguments[i];
        if (argument == "--autostart") {
            auto_start = true;
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
                      << "  --send TEXT             transmit TEXT after starting\n";
            return 0;
        } else {
            std::cerr << "Unknown or incomplete argument: " << argument.toStdString() << '\n';
            return 2;
        }
    }
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
    window.set_quit_after_send(quit_after_send);
    window.set_bench_parameters(bench_center, bench_volume, bench_rx_center, bench_tx_center);
    window.show();
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
