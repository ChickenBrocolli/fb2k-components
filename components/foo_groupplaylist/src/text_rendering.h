#pragma once

// How the panel draws its text: DirectWrite (colour emoji) or GDI (emoji in black and white).
// Automatic uses DirectWrite, except under Wine (Linux, and macOS through CrossOver and the like), whose DirectWrite
// crashes now and then and leaves the column titles blank. The main preferences page sets it.
enum class TextRendering { Automatic = 0, DirectWrite = 1, Gdi = 2 };

TextRendering GetTextRendering();
void SetTextRendering(TextRendering rendering);

bool RunningOnWine();

// Whether text is drawn with DirectWrite now: the setting, with Automatic resolved.
bool UseDirectWrite();
