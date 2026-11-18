#pragma once

#include <string>
#include <utility>
#include <vector>

// The language of the component's own texts: English or Korean.
// Automatic follows the Windows display language. The main preferences page sets it.
enum class UiLanguage { Automatic = 0, English = 1, Korean = 2 };

UiLanguage GetUiLanguage();
void SetUiLanguage(UiLanguage language);
bool IsKoreanUi();

// The text in the current language. english is the key in ui_text.h; a text with no entry stays English.
const char* Tr(const char* english);
std::wstring TrW(const char* english);

// A built-in column's title in the current language. Custom column names are the user's and stay as they are.
std::string ColumnTitle(const std::string& name);

// "12 tracks" in the current language.
std::string TrackCount(size_t tracks);

// The texts of a dialog's controls, kept in English so the dialog can be shown again in another language.
class DialogTexts {
public:
	// Reads the English texts from the dialog template's controls and shows them in the current language.
	void Translate(HWND dialog);
	// Shows the read texts again in the current language.
	void Retranslate();

private:
	std::vector<std::pair<HWND, std::wstring>> m_english;
};
