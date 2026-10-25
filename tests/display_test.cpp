#include "text_frame.h"

constexpr bool glyphPlacement() {
    TextFrame frame;
    frame.drawText(1, "A1");
    const uint8_t* line = &frame.pixels[TextFrame::width];
    for (int x = 0; x < 5; ++x) {
        if (line[x] != font5x7['A' - 0x20][x] || line[6 + x] != font5x7['1' - 0x20][x]) return false;
    }
    if (line[5] != 0 || line[11] != 0) return false; // One blank column between characters.
    for (int x = 0; x < TextFrame::width; ++x) {
        if (frame.pixels[x] != 0) return false; // Other lines untouched.
    }
    return true;
}

constexpr bool clippingAndInvalidInput() {
    TextFrame frame;
    frame.drawText(0, "0123456789012345678901234"); // 25 characters; 21 fit.
    for (int x = 0; x < 5; ++x) {
        if (frame.pixels[20 * 6 + x] != font5x7['0' - 0x20][x]) return false; // 21st character drawn.
    }
    for (int x = TextFrame::columns * 6; x < TextFrame::width; ++x) {
        if (frame.pixels[x] != 0) return false; // Two spare columns stay blank.
    }
    frame.drawText(-1, "X");
    frame.drawText(TextFrame::lines, "X");
    frame.drawText(2, nullptr);
    frame.drawText(3, "\x01");
    for (int x = 0; x < 5; ++x) {
        if (frame.pixels[3 * TextFrame::width + x] != font5x7['?' - 0x20][x]) return false;
    }
    frame.clear();
    for (const uint8_t column : frame.pixels) {
        if (column) return false;
    }
    return true;
}

static_assert(glyphPlacement(), "Glyph columns land on the right line and cell");
static_assert(clippingAndInvalidInput(), "Clip long text, ignore bad lines, replace unknown characters");
