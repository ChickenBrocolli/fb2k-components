#pragma once

#include <string>
#include <utility>
#include <vector>

#include "column_def.h"

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
std::string ColumnTitle(const ColumnDef& def);

// "12 tracks" in the current language.
std::string TrackCount(size_t tracks);

// The texts of a dialog's controls, kept in English so the dialog can be shown again in another language.
// After each translation the labels are fitted to their text (label_fit.h): the dialogs are laid out for English,
// and a shorter translation would leave a gap between a label or check box and the control after it.
class DialogTexts {
public:
	// Reads the English texts and places from the dialog template's controls and shows them in the current language.
	void Translate(HWND dialog);
	// Shows the read texts again in the current language and fits the labels to them.
	void Retranslate();

private:
	struct Control {
		HWND window;
		RECT englishPlace;  // in dialog client coordinates, as the template laid it out
		std::wstring english;  // empty for a control whose text is not translated
		bool label;  // a text label or check box: it takes part in lining up a column of labels
		bool fits;  // a label or check box that can be narrowed to its text
		bool checkBox;  // the text starts after a check box or radio button square
	};

	void FitLabels();

	HWND m_dialog = nullptr;
	std::vector<Control> m_controls;
};
