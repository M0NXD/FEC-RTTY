#include "fectty/waterfall_widget.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>

namespace fectty {

WaterfallWidget::WaterfallWidget(QWidget* parent) : QWidget(parent),
    image_(int(WaterfallSpectrum::columns), history_rows, QImage::Format_RGB32) {
    setMinimumHeight(155);
    setMinimumWidth(320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName(QStringLiteral("Live audio waterfall"));
    setAccessibleDescription(QStringLiteral("Left click tunes RX; right click or Ctrl click tunes TX. Arrow keys tune by 1 Hz; Shift uses 10 Hz."));
    setToolTip(accessibleDescription());
    image_.fill(QColor(QStringLiteral("#08111c")));
}

QRectF WaterfallWidget::plot_rect() const {
    const double text_height = fontMetrics().height();
    const double top = 2*text_height + 12;
    return {8, top, double(width()-16), std::max(1.0, height()-top-text_height-10)};
}
double WaterfallWidget::frequency_at(double x) const {
    const auto plot = plot_rect();
    return std::clamp((x - plot.left()) / std::max(1.0, plot.width()) * WaterfallSpectrum::max_hz, 0.0, WaterfallSpectrum::max_hz);
}
double WaterfallWidget::position_at(double frequency) const {
    const auto plot = plot_rect();
    return plot.left() + std::clamp(frequency, 0.0, WaterfallSpectrum::max_hz) / WaterfallSpectrum::max_hz * plot.width();
}

QRgb WaterfallWidget::color(float dbfs) const {
    if (!std::isfinite(dbfs)) return qRgb(8,17,28);
    const double level = std::clamp((dbfs - floor_dbfs_) / 70.0, 0.0, 1.0);
    static const std::array<QColor, 6> stops{
        QColor(8,17,28), QColor(20,42,104), QColor(24,101,165),
        QColor(47,186,188), QColor(255,181,78), QColor(255,249,211)};
    const double scaled = level * (stops.size() - 1);
    const size_t index = std::min<size_t>(size_t(scaled), stops.size() - 2);
    const double fraction = scaled - index;
    const auto mix = [fraction](int a, int b) { return int(std::lround(a + fraction * (b - a))); };
    return qRgb(mix(stops[index].red(), stops[index+1].red()),
                mix(stops[index].green(), stops[index+1].green()),
                mix(stops[index].blue(), stops[index+1].blue()));
}

void WaterfallWidget::rebuild_image() {
    image_.fill(QColor(QStringLiteral("#08111c")));
    for (size_t row = 0; row < history_.size(); ++row) {
        auto* pixels = reinterpret_cast<QRgb*>(image_.scanLine(int(row)));
        for (size_t k = 0; k < WaterfallSpectrum::columns; ++k) pixels[k] = color(history_[row][k]);
    }
    update();
}
void WaterfallWidget::add_row(const WaterfallSpectrum::Row& row) {
    if (row.size() != WaterfallSpectrum::columns) return;
    history_.push_front(row);
    if (history_.size() > history_rows) history_.pop_back();
    rebuild_image();
}
void WaterfallWidget::clear() { history_.clear(); rebuild_image(); }
void WaterfallWidget::set_markers(double rx, double tx) { if (std::isfinite(rx)) rx_hz_ = std::clamp(rx,300.0,3000.0); if (std::isfinite(tx)) tx_hz_ = std::clamp(tx,300.0,3000.0); update(); }
void WaterfallWidget::set_floor(double value) { if (!std::isfinite(value)) return; floor_dbfs_ = std::clamp(value, -110.0, -20.0); rebuild_image(); }
void WaterfallWidget::set_running(bool running, bool input_enabled) { running_ = running; input_enabled_ = input_enabled; update(); }

void WaterfallWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#111923")));
    const auto plot = plot_rect();
    // Crop the FFT image to precisely 3500 Hz rather than stretching its last
    // bin past the labelled frequency scale.
    painter.drawImage(plot, image_, QRectF(0.5, 0, WaterfallSpectrum::max_hz / WaterfallSpectrum::bin_hz, history_rows));
    painter.setPen(QColor(QStringLiteral("#9aaabd")));
    const auto metrics = painter.fontMetrics();
    const int spacing = plot.width() < 600 ? 1000 : 500;
    for (int hz = 0; hz <= 3500; hz += spacing) {
        const auto x = position_at(hz);
        painter.drawLine(QPointF(x, plot.top()-5), QPointF(x, plot.top()));
        const auto text = QStringLiteral("%1").arg(hz);
        const auto label_width = metrics.horizontalAdvance(text) + 4;
        const auto left = std::clamp(x-label_width/2.0, 0.0, double(width()-label_width));
        painter.drawText(QRectF(left, metrics.height()+2, label_width, metrics.height()+2), Qt::AlignCenter, text);
    }
    painter.drawText(QRectF(8, 0, width()-16, metrics.height()+2), Qt::AlignLeft, QStringLiteral("Audio frequency · Hz"));
    auto status = running_ ? (input_enabled_ ? QStringLiteral("Live RX · newest at top") : QStringLiteral("TX-only · no RX input")) : QStringLiteral("Paused · start audio");
    const auto legend = QStringLiteral("%1 to %2 dBFS").arg(floor_dbfs_, 0, 'f', 0).arg(floor_dbfs_+70, 0, 'f', 0);
    if (metrics.horizontalAdvance(status) + metrics.horizontalAdvance(legend) + 24 > width())
        status = running_ ? (input_enabled_ ? QStringLiteral("Live RX") : QStringLiteral("No RX")) : QStringLiteral("Paused");
    const QRectF footer(8, height()-metrics.height()-7, width()-16, metrics.height()+4);
    painter.drawText(footer, Qt::AlignLeft, status);
    if (metrics.horizontalAdvance(status) + metrics.horizontalAdvance(legend) + 24 <= width())
        painter.drawText(footer, Qt::AlignRight, legend);

    painter.save();
    painter.setClipRect(plot);
    const QColor rx(QStringLiteral("#7ed7c3")), tx(QStringLiteral("#ffb45e"));
    painter.fillRect(QRectF(position_at(rx_hz_-75), plot.top(), position_at(rx_hz_+75)-position_at(rx_hz_-75), plot.height()), QColor(126,215,195,22));
    for (double offset : {-75.0, -25.0, 25.0, 75.0}) {
        painter.setPen(QPen(QColor(126,215,195,100), 1, Qt::DotLine));
        painter.drawLine(QPointF(position_at(rx_hz_+offset), plot.top()), QPointF(position_at(rx_hz_+offset), plot.bottom()));
    }
    painter.setPen(QPen(rx, 1.5));
    painter.drawLine(QPointF(position_at(rx_hz_), plot.top()), QPointF(position_at(rx_hz_), plot.bottom()));
    painter.setPen(QPen(tx, 1.5, Qt::DashLine));
    const double tx_x = position_at(tx_hz_) + (std::abs(tx_hz_-rx_hz_) < 1 ? 2 : 0);
    painter.drawLine(QPointF(tx_x, plot.top()), QPointF(tx_x, plot.bottom()));
    painter.restore();
    painter.setPen(QPen(QColor(QStringLiteral("#334558")), 1));
    painter.drawRect(plot);
    painter.setBrush(rx); painter.setPen(Qt::NoPen);
    painter.drawPolygon(QPolygonF{QPointF(position_at(rx_hz_)-5,plot.top()), QPointF(position_at(rx_hz_)+5,plot.top()), QPointF(position_at(rx_hz_),plot.top()+8)});
    painter.setBrush(tx);
    painter.drawPolygon(QPolygonF{QPointF(position_at(tx_hz_)-5,plot.bottom()), QPointF(position_at(tx_hz_)+5,plot.bottom()), QPointF(position_at(tx_hz_),plot.bottom()-8)});
    if (history_.empty()) {
        painter.setPen(QColor(QStringLiteral("#9aaabd")));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("Waiting for input audio"));
    }
    if (hasFocus()) { painter.setPen(QPen(QColor(QStringLiteral("#79c8ff")), 1, Qt::DotLine)); painter.setBrush(Qt::NoBrush); painter.drawRect(rect().adjusted(1,1,-2,-2)); }
}

void WaterfallWidget::tune(double x, bool transmit) {
    if ((transmit ? tx_enabled_ : rx_enabled_) && on_tune)
        on_tune(std::round(std::clamp(frequency_at(x), 300.0, 3000.0)*10)/10, transmit);
}
void WaterfallWidget::mousePressEvent(QMouseEvent* event) {
    if ((event->button() == Qt::LeftButton || event->button() == Qt::RightButton) && plot_rect().contains(event->position())) {
        setFocus();
        tune(event->position().x(), event->button() == Qt::RightButton || event->modifiers().testFlag(Qt::ControlModifier));
        event->accept(); return;
    }
    QWidget::mousePressEvent(event);
}
void WaterfallWidget::mouseMoveEvent(QMouseEvent* event) {
    if (plot_rect().contains(event->position())) {
        setToolTip(QStringLiteral("%1 Hz · left drag RX · right/Ctrl drag TX").arg(frequency_at(event->position().x()), 0, 'f', 1));
        if (event->buttons().testFlag(Qt::LeftButton) || event->buttons().testFlag(Qt::RightButton))
            tune(event->position().x(), event->buttons().testFlag(Qt::RightButton) || event->modifiers().testFlag(Qt::ControlModifier));
    }
    QWidget::mouseMoveEvent(event);
}
void WaterfallWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const bool tx = event->modifiers().testFlag(Qt::ControlModifier);
        const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
        const double current = tx ? tx_hz_ : rx_hz_;
        if ((tx ? tx_enabled_ : rx_enabled_) && on_tune)
            on_tune(std::clamp(current + (event->key() == Qt::Key_Right ? step : -step), 300.0, 3000.0), tx);
        event->accept(); return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace fectty
