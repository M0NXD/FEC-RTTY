#pragma once

#include <QString>
#include <QStringDecoder>
#include <string_view>

namespace fectty {
// Retain incomplete UTF-8 across modem frames and GUI timer ticks.
class GuiTextDecoder {
    QStringDecoder decoder_{QStringDecoder::Utf8};
public:
    QString push(std::string_view bytes) {
        const QString decoded = decoder_.decode(QByteArrayView(bytes.data(),
                                                   static_cast<qsizetype>(bytes.size())));
        QString visible;
        visible.reserve(decoded.size());
        for (const auto character : decoded) {
            const auto code = character.unicode();
            if (code == '\n' || code == '\r' || code == '\t' ||
                (code >= 0x20 && !(code >= 0x7f && code <= 0x9f))) {
                visible += character;
            }
        }
        return visible;
    }
    void reset() { decoder_.resetState(); }
};
} // namespace fectty
