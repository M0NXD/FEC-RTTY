#include "fectty/gui_text.hpp"
#include <QApplication>
#include <QPlainTextEdit>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int failures = 0;
    const auto check = [&](bool ok, const char* label) {
        if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    const std::string bytes = "1234567\xc3\xa9\n\n\xf0\x9f\x93\xbb\n";
    for (size_t width = 1; width <= 8; ++width) {
        fectty::GuiTextDecoder decoder;
        QPlainTextEdit view;
        for (size_t pos = 0; pos < bytes.size(); pos += width) {
            view.insertPlainText(decoder.push(std::string_view(bytes).substr(pos, width)));
        }
        check(view.toPlainText() == QString::fromUtf8(bytes.data(), static_cast<qsizetype>(bytes.size())),
              "split UTF8, embedded and trailing newlines render exactly");
    }
    fectty::GuiTextDecoder decoder;
    check(decoder.push(std::string("A\x04\x02\x01\0B\n\t", 9)) == QStringLiteral("AB\n\t"),
          "binary controls are hidden while line breaks and tabs remain");
    decoder.push("\xc3");
    decoder.reset();
    check(decoder.push("NEW\n") == QStringLiteral("NEW\n"), "new session clears incomplete UTF8 state");
    QPlainTextEdit lines;
    lines.insertPlainText(decoder.push("ONE\r\n\r\nTWO\r\n"));
    check(lines.toPlainText() == QStringLiteral("ONE\n\nTWO\n"), "CRLF text renders as normal lines");
    std::cout << "GUI text checks; failures=" << failures << '\n';
    return failures ? 1 : 0;
}
