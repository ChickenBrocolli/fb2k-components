#pragma once

#include <string>

// Helpers for dialogs where the user writes a title formatting pattern.

// Shows a menu of common fields under the button. Inserts the chosen field at the caret of the edit control.
void RunInsertFieldMenu(CWindow button, CEdit patternEdit);

// One line that shows what the pattern produces for the focused track of the active playlist.
std::string DescribePatternPreview(const char* pattern);
