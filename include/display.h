#pragma once
#include "text_frame.h"

// Optional 128x32 SSD1306 OLED. Returns false, and later updates do nothing,
// if no display answers on I2C: the button test keeps working without it.
bool startDisplay();
// Never blocks: the latest screen replaces any update not yet drawn.
void showScreen(const ScreenText& screen);
