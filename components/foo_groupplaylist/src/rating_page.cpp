#include "stdafx.h"

// Preferences > Tools > GroupPlaylist > Ratings and likes: where ratings and likes are kept, the star colour, and how
// the Like column draws its hearts. Like the main page, it edits a copy that the panels show at once; Apply saves it.

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include "playlist_view.h"
#include "preferences_pages.h"
#include "rating_store.h"
#include "resource.h"
#include "ui_language.h"
#include "view_settings.h"
#include "view_settings_fields.h"

namespace {

// The page's part of the view settings (SettingsPage::Ratings), and where ratings and where likes are kept.
struct RatingValues {
	RatingStorage storage = RatingStorage::Statistics;
	LikeStorage likes = LikeStorage::Store;
	PageViewSettings<SettingsPage::Ratings> view;

	bool operator==(const RatingValues& other) const {
		return storage == other.storage && likes == other.likes && view == other.view;
	}

	static RatingValues From(const ViewSettings& view, RatingStorage storage, LikeStorage likes) { return { storage, likes, { view } }; }

	void CopyTo(ViewSettings& to) const { view.CopyTo(to); }
};

// The rating choices in the order the list shows them, the same order as the like choices. The saved numbers of
// RatingStorage stay as they were, so only the list's order differs from them.
constexpr RatingStorage kRatingChoices[] = { RatingStorage::Statistics, RatingStorage::FileTags, RatingStorage::Both };

int RatingChoiceOf(RatingStorage storage) {
	for (int index = 0; index < static_cast<int>(std::size(kRatingChoices)); ++index) {
		if (kRatingChoices[index] == storage) return index;
	}
	return 0;
}

RatingValues SavedRatingValues() {
	return RatingValues::From(LoadViewSettings(), GetRatingStorage(), GetLikeStorage());
}

COLORREF FoobarHighlightColor() {
	const ui_config_manager::ptr colors = ui_config_manager::tryGet();
	return colors.is_valid() ? colors->getSysColor(COLOR_HOTLIGHT) : GetSysColor(COLOR_HOTLIGHT);
}

class RatingDialog : public CDialogImpl<RatingDialog>, public preferences_page_instance {
public:
	enum { IDD = IDD_RATING_PAGE };

	explicit RatingDialog(preferences_page_callback::ptr callback) : m_callback(callback), m_values(SavedRatingValues()) { s_open = this; }
	~RatingDialog() { if (s_open == this) s_open = nullptr; }

	static RatingDialog* s_open;

	ViewSettings WithEdits(ViewSettings view) const {
		if (m_previewing) m_values.CopyTo(view);
		return view;
	}

	// Settings were imported: the page shows them and drops its edits.
	void ReloadSaved() {
		m_values = SavedRatingValues();
		if (m_hWnd) ShowValues();
		m_callback->on_state_changed();
	}

	t_uint32 get_state() override {
		t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
		if (!(m_values == SavedRatingValues())) state |= preferences_state::changed;
		return state;
	}

	// Only this page's part is saved; the other pages' unapplied edits stay unapplied and still show.
	void apply() override {
		if (m_values.storage != GetRatingStorage() || m_values.likes != GetLikeStorage()) PlaylistView::ApplyRatingStorage(m_values.storage, m_values.likes);
		ViewSettings view = LoadViewSettings();
		m_values.CopyTo(view);
		PlaylistView::ApplyViewSettings(view);
		m_callback->on_state_changed();
	}

	void reset() override {
		RatingValues defaults = RatingValues::From(DefaultViewSettings(), RatingStorage::Statistics, LikeStorage::Store);
		m_values = defaults;
		ShowValues();
		ShowPreferencesPreview();
		m_callback->on_state_changed();
	}

	BEGIN_MSG_MAP_EX(RatingDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_DRAWITEM(OnDrawItem)
		COMMAND_HANDLER_EX(IDC_PREF_RATING, CBN_SELCHANGE, OnStorageChanged)
		COMMAND_HANDLER_EX(IDC_PREF_LIKE_STORAGE, CBN_SELCHANGE, OnStorageChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_STAR_COLOR_CUSTOM, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_LIKE_COLOR_CUSTOM, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_LIKE_STYLE_ACCENT, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_LIKE_STYLE_EMOJI, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_PREF_STAR_COLOR, OnChooseColor)
		COMMAND_ID_HANDLER_EX(IDC_LIKE_COLOR, OnChooseColor)
	END_MSG_MAP()

private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		// The order of kRatingChoices: the store first, then file tags, then both, as for likes. Without Playback
		// Statistics ratings use the file tags whatever is picked (rating_store.h), and the first choice says why.
		CComboBox storage(GetDlgItem(IDC_PREF_RATING));
		storage.AddString(TrW(IsPlaybackStatisticsInstalled() ? "Playback Statistics" : "Playback Statistics (not installed)").c_str());
		storage.AddString(TrW("File tags").c_str());
		storage.AddString(TrW("Both").c_str());
		// Likes have their own choice: Playback Statistics has none. The order follows LikeStorage.
		CComboBox likes(GetDlgItem(IDC_PREF_LIKE_STORAGE));
		likes.AddString(TrW("GroupPlaylist store").c_str());
		likes.AddString(TrW("File tags").c_str());
		likes.AddString(TrW("Both").c_str());
		ShowValues();
		m_previewing = true;
		return FALSE;
	}

	// Not applied: the panels show the saved settings again.
	void OnDestroy() {
		const bool edited = !(m_values == SavedRatingValues());
		m_previewing = false;
		if (edited) ShowPreferencesPreview();
		SetMsgHandled(FALSE);
	}

	void ShowValues() {
		CComboBox(GetDlgItem(IDC_PREF_RATING)).SetCurSel(RatingChoiceOf(m_values.storage));
		CComboBox(GetDlgItem(IDC_PREF_LIKE_STORAGE)).SetCurSel(static_cast<int>(m_values.likes));
		CheckDlgButton(IDC_PREF_STAR_COLOR_CUSTOM, m_values.view.fields.customStarColor ? BST_CHECKED : BST_UNCHECKED);
		CheckRadioButton(IDC_LIKE_STYLE_ACCENT, IDC_LIKE_STYLE_EMOJI, m_values.view.fields.likeStyle == LikeStyle::Emoji ? IDC_LIKE_STYLE_EMOJI : IDC_LIKE_STYLE_ACCENT);
		CheckDlgButton(IDC_LIKE_COLOR_CUSTOM, m_values.view.fields.customLikeColor ? BST_CHECKED : BST_UNCHECKED);
		ShowEnabled();
	}

	// The emoji heart has its own colour, so the heart colour only applies to the hearts in the star colour.
	void ShowEnabled() {
		const bool emoji = m_values.view.fields.likeStyle == LikeStyle::Emoji;
		GetDlgItem(IDC_PREF_STAR_COLOR).EnableWindow(m_values.view.fields.customStarColor);
		GetDlgItem(IDC_LIKE_COLOR_CUSTOM).EnableWindow(!emoji);
		GetDlgItem(IDC_LIKE_SWATCH).EnableWindow(!emoji);
		GetDlgItem(IDC_LIKE_COLOR).EnableWindow(!emoji && m_values.view.fields.customLikeColor);
		GetDlgItem(IDC_PREF_STAR_SWATCH).Invalidate();
		GetDlgItem(IDC_LIKE_SWATCH).Invalidate();
	}

	void Changed() {
		ShowEnabled();
		ShowPreferencesPreview();
		m_callback->on_state_changed();
	}

	// Where ratings and likes are kept changes the columns' patterns, so it waits for Apply, as on the main page before.
	void OnStorageChanged(UINT, int id, CWindow) {
		const int index = CComboBox(GetDlgItem(id)).GetCurSel();
		if (index >= 0 && index < static_cast<int>(std::size(kRatingChoices)) && id == IDC_PREF_RATING) m_values.storage = kRatingChoices[index];
		if (index >= 0 && id == IDC_PREF_LIKE_STORAGE) m_values.likes = static_cast<LikeStorage>(index);
		m_callback->on_state_changed();
	}

	void OnCheckChanged(UINT, int id, CWindow) {
		const bool checked = IsDlgButtonChecked(id) == BST_CHECKED;
		switch (id) {
		case IDC_PREF_STAR_COLOR_CUSTOM: m_values.view.fields.customStarColor = checked; break;
		case IDC_LIKE_COLOR_CUSTOM: m_values.view.fields.customLikeColor = checked; break;
		case IDC_LIKE_STYLE_ACCENT: if (checked) m_values.view.fields.likeStyle = LikeStyle::StarColour; break;
		case IDC_LIKE_STYLE_EMOJI: if (checked) m_values.view.fields.likeStyle = LikeStyle::Emoji; break;
		}
		Changed();
	}

	void OnChooseColor(UINT, int id, CWindow) {
		COLORREF& color = id == IDC_PREF_STAR_COLOR ? m_values.view.fields.starColor : m_values.view.fields.likeColor;
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

	// Each swatch shows the colour in use: the star colour follows foobar2000's highlight colour without its own,
	// and the heart colour follows the star colour.
	void OnDrawItem(int id, LPDRAWITEMSTRUCT item) {
		if (id != IDC_PREF_STAR_SWATCH && id != IDC_LIKE_SWATCH) {
			SetMsgHandled(FALSE);
			return;
		}
		const COLORREF star = m_values.view.fields.customStarColor ? m_values.view.fields.starColor : FoobarHighlightColor();
		const COLORREF color = id == IDC_PREF_STAR_SWATCH ? star : m_values.view.fields.customLikeColor ? m_values.view.fields.likeColor : star;
		CDCHandle dc(item->hDC);
		dc.FillSolidRect(&item->rcItem, color);
		dc.FrameRect(&item->rcItem, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
	}

	const preferences_page_callback::ptr m_callback;
	RatingValues m_values;
	bool m_previewing = false;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

RatingDialog* RatingDialog::s_open = nullptr;

class RatingPage : public preferences_page_impl<RatingDialog> {
public:
	const char* get_name() override { return Tr("Ratings and likes"); }
	GUID get_guid() override { return guid_rating_page; }
	GUID get_parent_guid() override { return guid_preferences_page; }
};

preferences_page_factory_t<RatingPage> g_ratingPageFactory;

} // namespace

ViewSettings WithRatingPageEdits(const ViewSettings& view) {
	return RatingDialog::s_open ? RatingDialog::s_open->WithEdits(view) : view;
}

void ReloadRatingPage() {
	if (RatingDialog::s_open) RatingDialog::s_open->ReloadSaved();
}
