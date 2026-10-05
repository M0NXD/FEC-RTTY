#pragma once
#include "fectty/waterfall_spectrum.hpp"
#include <QImage>
#include <QWidget>
#include <deque>
#include <functional>

namespace fectty {

class WaterfallWidget final : public QWidget {
public:
    explicit WaterfallWidget(QWidget* parent = nullptr);
    std::function<void(double frequency_hz, bool transmit)> on_tune;
    void add_row(const WaterfallSpectrum::Row& row);
    void clear();
    void set_markers(double rx_hz, double tx_hz);
    void set_floor(double dbfs);
    void set_running(bool running, bool input_enabled = true);
    void set_tuning_enabled(bool rx, bool tx) { rx_enabled_ = rx; tx_enabled_ = tx; }
    double frequency_at(double x) const;
    double position_at(double frequency) const;
    size_t row_count() const { return history_.size(); }
    double rx_hz() const { return rx_hz_; }
    double tx_hz() const { return tx_hz_; }
    QSize sizeHint() const override { return {760, 230}; }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    static constexpr int history_rows = 240;
    std::deque<WaterfallSpectrum::Row> history_;
    QImage image_;
    double rx_hz_ = 1500, tx_hz_ = 1500, floor_dbfs_ = -85;
    bool running_ = false, input_enabled_ = true;
    bool rx_enabled_ = true, tx_enabled_ = true;
    QRectF plot_rect() const;
    void rebuild_image();
    QRgb color(float dbfs) const;
    void tune(double x, bool transmit);
};

} // namespace fectty
