#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include "playlist_view.h"
#include "preferences_pages.h"
#include "rating_store.h"
#include "resource.h"
#include "settings_transfer.h"
#include "ui_language.h"
#include "view_settings.h"

namespace {

// The page edits a copy. Apply saves the copy and redraws every panel, as foobar2000 preferences pages do.
// Smooth scrolling and header pinning belong to the Scrolling page; this page leaves them as saved.
struct PageValues {
	ViewSettings view;
	RatingStorage rating;
	UiLanguage language;

	bool operator==(const PageValues& other) const {
		const ViewSettings& a = view;
		const ViewSettings& b = other.view;
		return rating == other.rating && language == other.language &&
			a.customTrackFont == b.customTrackFont && memcmp(&a.trackFont, &b.trackFont, sizeof(LOGFONTW)) == 0 &&
			a.customHeaderFont == b.customHeaderFont && memcmp(&a.headerFont, &b.headerFont, sizeof(LOGFONTW)) == 0 &&
			a.headerColor == b.headerColor && a.labelColor == b.labelColor &&
			a.rowPadding == b.rowPadding && a.coverSize == b.coverSize &&
			a.presetPerPlaylist == b.presetPerPlaylist && a.dimBrackets == b.dimBrackets &&
			a.sortOnHeaderClick == b.sortOnHeaderClick && a.dragColumns == b.dragColumns && a.scrollPerPlaylist == b.scrollPerPlaylist &&
			a.boldPlaying == b.boldPlaying;
	}
};

PageValues SavedValues() {
	return { LoadViewSettings(), GetRatingStorage(), GetUiLanguage() };
}

// The title of the page's message boxes.
const wchar_t kMessageTitle[] = L"GroupPlaylist";

// The DPI of the screen, which the font dialog sizes its fonts for.
int ScreenDpi() {
	CWindowDC screen(NULL);
	return screen.GetDeviceCaps(LOGPIXELSY);
}

// The saved fonts are at 96 DPI (view_settings.h), so the point size comes from 96 pixels per inch.
std::wstring DescribeFont(const LOGFONTW& font) {
	const int points = MulDiv(font.lfHeight < 0 ? -font.lfHeight : font.lfHeight, 72, 96);
	return std::wstring(font.lfFaceName) + L", " + std::to_wstring(points) + L" pt" + (font.lfWeight >= FW_BOLD ? TrW(", bold") : L"");
}

class PreferencesDialog : public CDialogImpl<PreferencesDialog>, public preferences_page_instance {
public:
	enum { IDD = IDD_PREFERENCES };

	explicit PreferencesDialog(preferences_page_callback::ptr callback)
		: m_callback(callback), m_values(SavedValues()), m_savedCoverSize(m_values.view.coverSize), m_savedDragColumns(m_values.view.dragColumns) {
		s_open = this;
	}
	~PreferencesDialog() { if (s_open == this) s_open = nullptr; }

	// The page while it exists, so a change made in a panel shows on it. foobar2000 keeps a page that was shown
	// until Preferences closes.
	static PreferencesDialog* s_open;

	// A panel saved a new cover size (Ctrl+wheel, dragging the cover column) or column dragging (the header menu).
	// The page takes what changed since it last looked, so Apply does not put the old value back. A setting saved
	// elsewhere (header pinning, say) changes neither, so an edit on the page that was not applied yet stays.
	void TakePanelChanges() {
		const ViewSettings saved = LoadViewSettings();
		bool taken = false;
		if (saved.coverSize != m_savedCoverSize) {
			m_values.view.coverSize = saved.coverSize;
			taken = true;
		}
		if (saved.dragColumns != m_savedDragColumns) {
			m_values.view.dragColumns = saved.dragColumns;
			taken = true;
		}
		m_savedCoverSize = saved.coverSize;
		m_savedDragColumns = saved.dragColumns;
		if (!taken) return;
		if (m_hWnd) ShowValues();
		m_callback->on_state_changed();
	}

	t_uint32 get_state() override {
		t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
		if (!(m_values == SavedValues())) state |= preferences_state::changed;
		return state;
	}

	void apply() override {
		const bool languageChanged = m_values.language != GetUiLanguage();
		if (languageChanged) SetUiLanguage(m_values.language);
		if (m_values.rating != GetRatingStorage()) PlaylistView::ApplyRatingStorage(m_values.rating);
		// This also redraws the column titles and track counts, so a new language shows in every panel.
		ViewSettings view = m_values.view;
		const ViewSettings saved = LoadViewSettings();
		view.smoothScroll = saved.smoothScroll;
		view.pinHeaders = saved.pinHeaders;
		PlaylistView::ApplyViewSettings(view);
		if (languageChanged) {
			m_texts.Retranslate();
			FillChoices();
			ShowValues();
		}
		m_callback->on_state_changed();
	}

	void reset() override {
		m_values = { DefaultViewSettings(), RatingStorage::FileTags, UiLanguage::Automatic };
		ShowValues();
		m_callback->on_state_changed();
	}

	BEGIN_MSG_MAP_EX(PreferencesDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DRAWITEM(OnDrawItem)
		COMMAND_ID_HANDLER_EX(IDC_PREF_TRACK_FONT_CUSTOM, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_HEADER_FONT_CUSTOM, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_PRESET_PER_LIST, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_DIM_BRACKETS, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_SORT_ON_CLICK, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_DRAG_COLUMNS, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_SCROLL_PER_LIST, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_BOLD_PLAYING, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_TRACK_FONT_BUTTON, OnChooseFont)
		COMMAND_ID_HANDLER_EX(IDC_PREF_HEADER_FONT_BUTTON, OnChooseFont)
		COMMAND_ID_HANDLER_EX(IDC_PREF_HEADER_COLOR, OnChooseColor)
		COMMAND_ID_HANDLER_EX(IDC_PREF_LABEL_COLOR, OnChooseColor)
		COMMAND_HANDLER_EX(IDC_PREF_ROW_PADDING, EN_CHANGE, OnNumberChanged)
		COMMAND_HANDLER_EX(IDC_PREF_COVER_SIZE, EN_CHANGE, OnNumberChanged)
		COMMAND_HANDLER_EX(IDC_PREF_RATING, CBN_SELCHANGE, OnRatingChanged)
		COMMAND_HANDLER_EX(IDC_PREF_LANGUAGE, CBN_SELCHANGE, OnLanguageChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_EXPORT, OnExport)
		COMMAND_ID_HANDLER_EX(IDC_PREF_IMPORT, OnImport)
	END_MSG_MAP()

private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		CUpDownCtrl(GetDlgItem(IDC_PREF_ROW_PADDING_SPIN)).SetRange(kMinRowPadding, kMaxRowPadding);
		CUpDownCtrl(GetDlgItem(IDC_PREF_COVER_SIZE_SPIN)).SetRange(kMinCoverSize, kMaxCoverSize);
		FillChoices();
		ShowValues();
		m_showing = false;
		return FALSE;
	}

	// The texts of the two drop-down lists, in the current language. ShowValues selects the saved items.
	void FillChoices() {
		const std::wstring missing = IsPlaybackStatisticsInstalled() ? L"" : TrW(" (needs Playback Statistics)");
		CComboBox rating(GetDlgItem(IDC_PREF_RATING));
		rating.ResetContent();
		rating.AddString(TrW("File tags").c_str());
		rating.AddString((TrW("Playback Statistics") + missing).c_str());
		rating.AddString((TrW("Both") + missing).c_str());
		// The list opens wide enough for the longest item, which the narrow box cuts off.
		CRect box;
		rating.GetWindowRect(box);
		rating.SetDroppedWidth(box.Width() * 2);
		// The order follows UiLanguage.
		CComboBox language(GetDlgItem(IDC_PREF_LANGUAGE));
		language.ResetContent();
		language.AddString(TrW("Automatic").c_str());
		language.AddString(L"English");
		language.AddString(L"한국어");
		language.GetWindowRect(box);
		language.SetDroppedWidth(box.Width() * 3 / 2);
	}

	void ShowValues() {
		const bool wasShowing = m_showing;
		m_showing = true;
		const ViewSettings& view = m_values.view;
		CheckDlgButton(IDC_PREF_TRACK_FONT_CUSTOM, view.customTrackFont ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_HEADER_FONT_CUSTOM, view.customHeaderFont ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_PRESET_PER_LIST, view.presetPerPlaylist ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_DIM_BRACKETS, view.dimBrackets ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_SORT_ON_CLICK, view.sortOnHeaderClick ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_DRAG_COLUMNS, view.dragColumns ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_SCROLL_PER_LIST, view.scrollPerPlaylist ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PREF_BOLD_PLAYING, view.boldPlaying ? BST_CHECKED : BST_UNCHECKED);
		// The spin buttons own the number in their edit box, so the number is set through them.
		CUpDownCtrl(GetDlgItem(IDC_PREF_ROW_PADDING_SPIN)).SetPos32(view.rowPadding);
		CUpDownCtrl(GetDlgItem(IDC_PREF_COVER_SIZE_SPIN)).SetPos32(view.coverSize);
		CComboBox(GetDlgItem(IDC_PREF_RATING)).SetCurSel(static_cast<int>(m_values.rating));
		CComboBox(GetDlgItem(IDC_PREF_LANGUAGE)).SetCurSel(static_cast<int>(m_values.language));
		m_showing = wasShowing;
		ShowFontsAndColors();
	}

	void ShowFontsAndColors() {
		const ViewSettings& view = m_values.view;
		SetDlgItemText(IDC_PREF_TRACK_FONT_NAME, DescribeFont(view.trackFont).c_str());
		SetDlgItemText(IDC_PREF_HEADER_FONT_NAME, DescribeFont(view.headerFont).c_str());
		GetDlgItem(IDC_PREF_TRACK_FONT_BUTTON).EnableWindow(view.customTrackFont);
		GetDlgItem(IDC_PREF_TRACK_FONT_NAME).EnableWindow(view.customTrackFont);
		GetDlgItem(IDC_PREF_HEADER_FONT_BUTTON).EnableWindow(view.customHeaderFont);
		GetDlgItem(IDC_PREF_HEADER_FONT_NAME).EnableWindow(view.customHeaderFont);
		GetDlgItem(IDC_PREF_HEADER_SWATCH).Invalidate();
		GetDlgItem(IDC_PREF_LABEL_SWATCH).Invalidate();
	}

	void Changed() {
		ShowFontsAndColors();
		m_callback->on_state_changed();
	}

	void OnCheckChanged(UINT, int id, CWindow) {
		const bool checked = IsDlgButtonChecked(id) == BST_CHECKED;
		ViewSettings& view = m_values.view;
		switch (id) {
		case IDC_PREF_TRACK_FONT_CUSTOM: view.customTrackFont = checked; break;
		case IDC_PREF_HEADER_FONT_CUSTOM: view.customHeaderFont = checked; break;
		case IDC_PREF_PRESET_PER_LIST: view.presetPerPlaylist = checked; break;
		case IDC_PREF_DIM_BRACKETS: view.dimBrackets = checked; break;
		case IDC_PREF_SORT_ON_CLICK: view.sortOnHeaderClick = checked; break;
		case IDC_PREF_DRAG_COLUMNS: view.dragColumns = checked; break;
		case IDC_PREF_SCROLL_PER_LIST: view.scrollPerPlaylist = checked; break;
		case IDC_PREF_BOLD_PLAYING: view.boldPlaying = checked; break;
		}
		Changed();
	}

	void OnNumberChanged(UINT, int id, CWindow) {
		if (m_showing) return;
		BOOL valid = FALSE;
		const int value = static_cast<int>(GetDlgItemInt(id, &valid, FALSE));
		if (!valid) return;
		if (id == IDC_PREF_ROW_PADDING) m_values.view.rowPadding = ClampRowPadding(value);
		else m_values.view.coverSize = ClampCoverSize(value);
		m_callback->on_state_changed();
	}

	void OnRatingChanged(UINT, int, CWindow) {
		const int index = CComboBox(GetDlgItem(IDC_PREF_RATING)).GetCurSel();
		if (index >= 0) m_values.rating = static_cast<RatingStorage>(index);
		m_callback->on_state_changed();
	}

	void OnLanguageChanged(UINT, int, CWindow) {
		const int index = CComboBox(GetDlgItem(IDC_PREF_LANGUAGE)).GetCurSel();
		if (index >= 0) m_values.language = static_cast<UiLanguage>(index);
		m_callback->on_state_changed();
	}

	void OnChooseFont(UINT, int id, CWindow) {
		LOGFONTW& font = id == IDC_PREF_TRACK_FONT_BUTTON ? m_values.view.trackFont : m_values.view.headerFont;
		LOGFONTW chosen = FontAtDpi(font, ScreenDpi());
		CHOOSEFONTW dialog = { sizeof(dialog) };
		dialog.hwndOwner = m_hWnd;
		dialog.lpLogFont = &chosen;
		dialog.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS;
		if (!ChooseFontW(&dialog)) return;
		font = FontAt96Dpi(chosen, ScreenDpi());
		Changed();
	}

	void OnChooseColor(UINT, int id, CWindow) {
		COLORREF& color = id == IDC_PREF_HEADER_COLOR ? m_values.view.headerColor : m_values.view.labelColor;
		static COLORREF customColors[16] = {};
		CHOOSECOLORW dialog = { sizeof(dialog) };
		dialog.hwndOwner = m_hWnd;
		dialog.rgbResult = color;
		dialog.lpCustColors = customColors;
		dialog.Flags = CC_RGBINIT | CC_FULLOPEN;
		if (!ChooseColorW(&dialog)) return;
		color = dialog.rgbResult;
		Changed();
	}

	// The settings as saved, not the unapplied changes on this page.
	void OnExport(UINT, int, CWindow) {
		const std::optional<std::wstring> path = AskSettingsFile(m_hWnd, true);
		if (!path) return;
		if (!WriteSettingsFile(*path, ExportSettingsText())) {
			MessageBox(TrW("The settings could not be saved to this file.").c_str(), kMessageTitle, MB_OK | MB_ICONWARNING);
			return;
		}
		MessageBox(TrW("The settings were saved.").c_str(), kMessageTitle, MB_OK | MB_ICONINFORMATION);
	}

	void OnImport(UINT, int, CWindow) {
		const std::optional<std::wstring> path = AskSettingsFile(m_hWnd, false);
		if (!path) return;
		const std::optional<std::string> text = ReadSettingsFile(*path);
		if (!text) {
			MessageBox(TrW("This file could not be read.").c_str(), kMessageTitle, MB_OK | MB_ICONWARNING);
			return;
		}
		if (!IsSettingsText(*text)) {
			MessageBox(TrW("This is not a GroupPlaylist settings file.").c_str(), kMessageTitle, MB_OK | MB_ICONWARNING);
			return;
		}
		const std::wstring question = TrW("Replace the current GroupPlaylist settings with the settings in this file? Changes on this page that were not applied are lost.");
		if (MessageBox(question.c_str(), kMessageTitle, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
		const UiLanguage language = GetUiLanguage();
		ImportSettingsText(*text);
		ReloadColumnsPage();
		ReloadGroupsPage();
		ReloadScrollingPage();
		m_values = SavedValues();
		if (GetUiLanguage() != language) {
			m_texts.Retranslate();
			FillChoices();
		}
		ShowValues();
		m_callback->on_state_changed();
		MessageBox(TrW("The settings were loaded.").c_str(), kMessageTitle, MB_OK | MB_ICONINFORMATION);
	}

	void OnDrawItem(int id, LPDRAWITEMSTRUCT item) {
		if (id != IDC_PREF_HEADER_SWATCH && id != IDC_PREF_LABEL_SWATCH) {
			SetMsgHandled(FALSE);
			return;
		}
		CDCHandle dc(item->hDC);
		dc.FillSolidRect(&item->rcItem, id == IDC_PREF_HEADER_SWATCH ? m_values.view.headerColor : m_values.view.labelColor);
		dc.FrameRect(&item->rcItem, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
	}

	const preferences_page_callback::ptr m_callback;
	PageValues m_values;
	// The cover size and column dragging as saved when the page last looked (TakePanelChanges).
	int m_savedCoverSize;
	bool m_savedDragColumns;
	// True while the page or a spin button writes a number into an edit box, so that write is not read back as an edit.
	// It starts true because the spin buttons write to their edit boxes while the dialog is being created.
	bool m_showing = true;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

class PreferencesPage : public preferences_page_impl<PreferencesDialog> {
public:
	const char* get_name() override { return "GroupPlaylist"; }
	GUID get_guid() override { return guid_preferences_page; }
	// Component settings live under Tools, as in other foobar2000 components.
	GUID get_parent_guid() override { return preferences_page::guid_tools; }
};

preferences_page_factory_t<PreferencesPage> g_preferences_page_factory;

PreferencesDialog* PreferencesDialog::s_open = nullptr;

} // namespace

void PanelSettingsChanged() {
	if (PreferencesDialog::s_open) PreferencesDialog::s_open->TakePanelChanges();
}

