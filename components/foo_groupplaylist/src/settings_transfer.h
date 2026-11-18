#pragma once

#include <optional>
#include <string>

// Every GroupPlaylist setting in one text file, to keep or to take to another foobar2000 (settings_file.h).
// Left out: what belongs to a playlist (its group preset and scroll position) or to a track (ratings),
// and each panel's own columns and widths, which the foobar2000 layout keeps.

// The saved settings as the text of a settings file.
std::string ExportSettingsText();

bool IsSettingsText(const std::string& text);

// Saves the settings the text has and updates every panel. A setting the file does not have, or has in a form
// that cannot be read, stays as it is. Returns false when the text is not a settings file.
bool ImportSettingsText(const std::string& text);

// A file chosen in the Save or Open dialog.
std::optional<std::wstring> AskSettingsFile(HWND parent, bool save);
bool WriteSettingsFile(const std::wstring& path, const std::string& text);
std::optional<std::string> ReadSettingsFile(const std::wstring& path);
