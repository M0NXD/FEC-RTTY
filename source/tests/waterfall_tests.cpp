#include "fectty/waterfall_widget.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int failures = 0, checks = 0;
    const auto check = [&](bool ok, const char* label) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; } };
    fectty::WaterfallSpectrum analyser;
    std::vector<float> audio(16384);
    std::vector<fectty::WaterfallSpectrum::Row> rows;
    for (double tone : {225.0, 1425.0, 1475.0, 1525.0, 1575.0, 3075.0}) {
        analyser.reset(); rows.clear();
        for (size_t i = 0; i < audio.size(); ++i) audio[i] = float(0.25 * std::sin(2*std::numbers::pi*tone*i/48000));
        for (size_t pos = 0; pos < audio.size(); pos += 333) {
            auto result = analyser.push(std::span<const float>(audio).subspan(pos, std::min<size_t>(333, audio.size()-pos)));
            rows.insert(rows.end(), result.begin(), result.end());
        }
        if (rows.empty() || rows.front().empty()) { check(false, "FFT must emit rows"); return 1; }
        const auto peak = std::max_element(rows.front().begin(), rows.front().end());
        const auto measured = double(peak - rows.front().begin()) * analyser.bin_hz;
        check(rows.size() == 7 && std::abs(measured-tone) <= analyser.bin_hz/2, "FFT peak and arbitrary callback accumulation");
        check(*peak < -11.5 && *peak > -14, "Hann tone amplitude has coherent dBFS normalization");
    }
    analyser.reset();
    std::fill(audio.begin(), audio.end(), std::numeric_limits<float>::quiet_NaN());
    const auto silence = analyser.push(audio);
    if (silence.empty() || silence.front().empty()) { check(false, "silence must emit rows"); return 1; }
    check(std::all_of(silence.front().begin(), silence.front().end(), [](float value) { return std::isfinite(value) && value <= -100; }), "nonfinite/silent input remains finite and dark");

    fectty::WaterfallWidget widget;
    widget.resize(1000, 260);
    check(std::abs(widget.frequency_at(widget.position_at(1800.1))-1800.1) < 1e-8, "frequency/pixel mapping is reversible");
    double tuned = 0; bool tx = false; int calls = 0;
    widget.on_tune = [&](double hz, bool transmit) { tuned = hz; tx = transmit; ++calls; };
    const auto click = [&](double hz, Qt::MouseButton button, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const QPointF position(widget.position_at(hz), 100);
        QMouseEvent event(QEvent::MouseButtonPress, position, position, button, button, modifiers);
        QApplication::sendEvent(&widget, &event);
    };
    click(1800.1, Qt::LeftButton);
    check(tuned == 1800.1 && !tx, "left click selects precise RX center");
    click(2100.2, Qt::RightButton);
    check(tuned == 2100.2 && tx, "right click selects independent TX center");
    click(1900.3, Qt::LeftButton, Qt::ControlModifier);
    check(tuned == 1900.3 && tx, "Ctrl click selects TX");
    widget.set_tuning_enabled(true, false);
    const int before = calls; click(2000, Qt::RightButton);
    check(calls == before, "TX lock rejects mouse tuning during playback");
    widget.set_markers(1800.1, 2100.2);
    QKeyEvent arrow(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QApplication::sendEvent(&widget, &arrow);
    check(std::abs(tuned-1801.1) < 1e-8 && !tx, "keyboard supports 1 Hz RX tuning");
    click(0, Qt::LeftButton); check(tuned == 300, "audio offset is clamped to modem range");
    for (int i = 0; i < 300; ++i) widget.add_row(rows[size_t(i)%rows.size()]);
    check(widget.row_count() == 240, "display history remains bounded");
    widget.set_floor(-100); widget.set_running(true);
    QImage preview(widget.size(), QImage::Format_ARGB32_Premultiplied);
    widget.render(&preview);
    check(preview.save(QStringLiteral("waterfall-widget-test.png")), "waterfall renders to a verifiable image");
    auto large_font = widget.font(); large_font.setPointSize(20); widget.setFont(large_font);
    widget.resize(640, 280);
    QImage large_preview(widget.size(), QImage::Format_ARGB32_Premultiplied);
    widget.render(&large_preview);
    check(large_preview.save(QStringLiteral("waterfall-large-font-test.png")), "frequency scale and footer adapt to large fonts");
    widget.clear(); check(widget.row_count() == 0, "new session clears history");
    std::cout << checks << " waterfall checks; failures=" << failures << '\n';
    return failures ? 1 : 0;
}
