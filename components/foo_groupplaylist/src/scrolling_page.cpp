#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include <cmath>
#include <cwchar>

#include "playlist_view.h"
#include "preferences_pages.h"
#include "resource.h"
#include "ui_language.h"
#include "view_settings.h"
#include "view_settings_fields.h"

// Preferences > Tools > GroupPlaylist > Scrolling: header pinning, smooth scrolling, and its numbers and presets.
// A change shows in every panel at once (ShowPreferencesPreview, SetLiveInertiaParams), so it can be felt with the
// wheel while the page is open. Apply saves; a page closed without Apply puts the saved settings back.

namespace {

constexpr int kSettingCount = static_cast<int>(std::size(kInertiaSettings));

// One slider and one number box per setting, in the order of kInertiaSettings.
constexpr int kSliderIds[] = { IDC_SCROLL_SLIDER_0, IDC_SCROLL_SLIDER_1, IDC_SCROLL_SLIDER_2, IDC_SCROLL_SLIDER_3, IDC_SCROLL_SLIDER_4 };
constexpr int kValueIds[] = { IDC_SCROLL_VALUE_0, IDC_SCROLL_VALUE_1, IDC_SCROLL_VALUE_2, IDC_SCROLL_VALUE_3, IDC_SCROLL_VALUE_4 };
static_assert(std::size(kSliderIds) == static_cast<size_t>(kSettingCount) && std::size(kValueIds) == static_cast<size_t>(kSettingCount));

// The setting a control belongs to, or -1 for another control.
int SettingOfControl(const int (&ids)[kSettingCount], int id) {
	for (int index = 0; index < kSettingCount; ++index) {
		if (ids[index] == id) return index;
	}
	return -1;
}

// Everything the page edits.
struct ScrollValues {
	PageViewSettings<SettingsPage::Scrolling> view;
	smooth_scroll::InertiaParams params;
	std::string presetName;
	std::vector<ScrollPreset> userPresets;

	bool operator==(const ScrollValues& other) const {
		return view == other.view && params == other.params && presetName == other.presetName && userPresets == other.userPresets;
	}
};

ScrollValues SavedScrollValues() {
	return { { LoadViewSettings() }, LoadInertiaParams(), LoadScrollPresetName(), LoadUserScrollPresets() };
}

// The view settings with the page's part replaced.
ViewSettings WithScrollValues(ViewSettings view, const ScrollValues& values) {
	values.view.CopyTo(view);
	return view;
}

bool SameViewPart(const ScrollValues& a, const ScrollValues& b) {
	return a.view == b.view;
}

// A preset name as shown: built-in presets in the current language, the user's as typed.
std::wstring ShownPresetName(const std::string& name) {
	if (IsBuiltInScrollPreset(name)) return TrW(name.c_str());
	return pfc::stringcvt::string_wide_from_utf8(name.c_str()).get_ptr();
}

// A slider moves in steps of the last decimal its number is shown with.
double SliderScale(const InertiaSetting& setting) {
	return std::pow(10.0, setting.decimals);
}

int SliderPosition(const InertiaSetting& setting, double value) {
	return static_cast<int>(std::lround(value * SliderScale(setting)));
}

std::wstring FormatValue(const InertiaSetting& setting, double value) {
	wchar_t text[32];
	swprintf_s(text, L"%.*f", setting.decimals, value);
	return text;
}

// The number typed in a box, rounded to the decimals the box shows, or nothing while it is not a number inside the
// range (the user may still be typing).
std::optional<double> ParseValue(const InertiaSetting& setting, const std::wstring& text) {
	wchar_t* end = nullptr;
	const double value = std::wcstod(text.c_str(), &end);
	if (end == text.c_str()) return std::nullopt;
	while (*end == L' ') ++end;
	if (*end != 0) return std::nullopt;
	if (!(value >= setting.minimum && value <= setting.maximum)) return std::nullopt;
	return ClampInertiaValue(setting, value);
}

class ScrollingDialog : public CDialogImpl<ScrollingDialog>, public preferences_page_instance {
public:
	enum { IDD = IDD_SCROLL_PAGE };

	explicit ScrollingDialog(preferences_page_callback::ptr callback)
		: m_callback(callback), m_values(SavedScrollValues()), m_shown(m_values), m_lastSaved(m_values) { s_open = this; }
	~ScrollingDialog() { if (s_open == this) s_open = nullptr; }

	// The page while it exists, so a switch from the panel header menu or an import shows on it.
	static ScrollingDialog* s_open;

	// The page's part of the panels' preview while it is shown (ShowPreferencesPreview).
	ViewSettings WithEdits(const ViewSettings& view) const { return m_previewing ? WithScrollValues(view, m_values) : view; }

	t_uint32 get_state() override {
		t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
		if (!(m_values == SavedScrollValues())) state |= preferences_state::changed;
		return state;
	}

	void apply() override {
		SaveInertiaParams(m_values.params);
		SaveScrollPresetName(m_values.presetName);
		SaveUserScrollPresets(m_values.userPresets);
		PlaylistView::ApplyViewSettings(WithScrollValues(LoadViewSettings(), m_values));
		m_shown = m_values;
		m_lastSaved = SavedScrollValues();
		m_callback->on_state_changed();
	}

	// The page's defaults. The user's presets stay.
	void reset() override {
		m_values.view.TakeFrom(DefaultViewSettings());
		const ScrollPreset preset = DefaultScrollPreset();
		m_values.params = preset.params;
		m_values.presetName = preset.name;
		ShowValues();
		Changed();
	}

	// Settings were imported: the page drops its edits and shows the saved settings, which the panels show already.
	void ReloadSaved() {
		m_values = m_shown = m_lastSaved = SavedScrollValues();
		SetLiveInertiaParams(m_values.params);
		if (m_hWnd) ShowValues();
		m_callback->on_state_changed();
	}

	// The panel header menu saved a new smooth scrolling, pinning or scroll preset setting. The page takes what
	// changed since it last looked, so a later Apply does not put the old setting back. A switch the page does not
	// show (column dragging, say) changes none of them, so an edit on the page that was not applied yet stays.
	void TakeMenuToggles() {
		if (!m_hWnd) return;
		const ScrollValues saved = SavedScrollValues();
		bool taken = m_values.view.TakeChanged(saved.view.fields, m_lastSaved.view.fields);
		if (!(saved.params == m_lastSaved.params) || saved.presetName != m_lastSaved.presetName) {
			m_values.params = saved.params;
			m_values.presetName = saved.presetName;
			taken = true;
		}
		m_lastSaved = saved;
		// The menu may have set the scroll numbers. The panels scroll with the page's own again, as before the menu
		// was used, and ApplyViewSettings shows the page's smooth scrolling and pinning after this.
		SetLiveInertiaParams(m_values.params);
		m_shown.view.TakeFrom(m_values.view.fields);
		if (!taken) return;
		ShowValues();
		m_callback->on_state_changed();
	}

	BEGIN_MSG_MAP_EX(ScrollingDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_HSCROLL(OnSlider)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_SMOOTH, OnCheckChanged)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_PIN, OnCheckChanged)
		COMMAND_HANDLER_EX(IDC_SCROLL_PRESET, CBN_SELCHANGE, OnPresetPicked)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_SAVE_NEW, OnSaveAsNew)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_OVERWRITE, OnOverwrite)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_DELETE, OnDelete)
		COMMAND_ID_HANDLER_EX(IDC_SCROLL_RESET, OnResetButton)
		COMMAND_CODE_HANDLER_EX(EN_CHANGE, OnNumberTyped)
		COMMAND_CODE_HANDLER_EX(EN_KILLFOCUS, OnNumberLeft)
	END_MSG_MAP()

private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		for (int i = 0; i < kSettingCount; ++i) {
			const InertiaSetting& setting = kInertiaSettings[i];
			CTrackBarCtrl slider(GetDlgItem(kSliderIds[i]));
			slider.SetRange(SliderPosition(setting, setting.minimum), SliderPosition(setting, setting.maximum));
			slider.SetPageSize((std::max)(1, (slider.GetRangeMax() - slider.GetRangeMin()) / 20));
		}
		CEdit(GetDlgItem(IDC_SCROLL_NEW_NAME)).SetLimitText(60);
		ShowValues();
		m_previewing = true;
		return FALSE;
	}

	void OnDestroy() {
		// Not applied: the panels show and scroll with the saved settings again.
		SetLiveInertiaParams(LoadInertiaParams());
		m_previewing = false;
		if (!SameViewPart(m_shown, SavedScrollValues())) ShowPreferencesPreview();
		SetMsgHandled(FALSE);
	}

	void ShowValues() {
		CheckDlgButton(IDC_SCROLL_SMOOTH, m_values.view.fields.smoothScroll ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_SCROLL_PIN, m_values.view.fields.pinHeaders ? BST_CHECKED : BST_UNCHECKED);
		for (int i = 0; i < kSettingCount; ++i) {
			CTrackBarCtrl(GetDlgItem(kSliderIds[i])).SetPos(SliderPosition(kInertiaSettings[i], m_values.params.*kInertiaSettings[i].field));
			ShowNumber(i);
		}
		FillPresets();
		ShowState();
	}

	void ShowNumber(int index) {
		const bool wasShowing = m_showing;
		m_showing = true;
		SetDlgItemText(kValueIds[index], FormatValue(kInertiaSettings[index], m_values.params.*kInertiaSettings[index].field).c_str());
		m_showing = wasShowing;
	}

	std::vector<ScrollPreset> Presets() const { return AllScrollPresets(m_values.userPresets); }

	bool IsCustom() const { return IsCustomScroll(Presets(), m_values.presetName, m_values.params); }

	// Numbers that belong to no preset are listed last as "Custom (unsaved)", and the name box offers a free
	// "Custom preset <number>" for them, unless a name was typed there already. An offered name that was not used goes again.
	void FillPresets() {
		CComboBox combo(GetDlgItem(IDC_SCROLL_PRESET));
		combo.ResetContent();
		const std::vector<ScrollPreset> presets = Presets();
		for (const ScrollPreset& preset : presets) combo.AddString(ShownPresetName(preset.name).c_str());
		m_listedCustom = IsCustom();
		if (m_listedCustom) {
			combo.AddString(TrW("Custom (unsaved)").c_str());
			combo.SetCurSel(static_cast<int>(presets.size()));
		} else {
			combo.SetCurSel(static_cast<int>(*FindScrollPreset(presets, m_values.presetName)));
		}

		CString typed;
		GetDlgItemText(IDC_SCROLL_NEW_NAME, typed);
		const bool offeredStill = !m_offeredName.empty() && std::wstring(typed) == m_offeredName;
		if (m_listedCustom && (typed.IsEmpty() || offeredStill)) {
			std::vector<std::string> shownBuiltIns;
			for (const ScrollPreset& preset : BuiltInScrollPresets()) shownBuiltIns.push_back(Tr(preset.name.c_str()));
			m_offeredName = pfc::stringcvt::string_wide_from_utf8(FreeScrollPresetName(Tr("Custom preset"), m_values.userPresets, shownBuiltIns).c_str()).get_ptr();
			SetDlgItemText(IDC_SCROLL_NEW_NAME, m_offeredName.c_str());
		} else if (!m_listedCustom && offeredStill) {
			SetDlgItemText(IDC_SCROLL_NEW_NAME, L"");
			m_offeredName.clear();
		}
	}

	// Which controls can be used. Overwrite saves custom numbers into the user's preset they started from;
	// Remove acts on the preset shown in the list, so it waits until a preset is shown there.
	void ShowState() {
		const bool custom = IsCustom();
		const bool fromUserPreset = FindScrollPreset(m_values.userPresets, m_values.presetName).has_value();

		const bool on = m_values.view.fields.smoothScroll;

		const int controls[] = { IDC_SCROLL_PRESET, IDC_SCROLL_NEW_NAME, IDC_SCROLL_SAVE_NEW, IDC_SCROLL_RESET };
		for (const int id : controls) GetDlgItem(id).EnableWindow(on);
		GetDlgItem(IDC_SCROLL_DELETE).EnableWindow(on && fromUserPreset && !custom);
		GetDlgItem(IDC_SCROLL_OVERWRITE).EnableWindow(on && fromUserPreset && custom);
		for (int i = 0; i < kSettingCount; ++i) {
			GetDlgItem(kSliderIds[i]).EnableWindow(on);
			GetDlgItem(kValueIds[i]).EnableWindow(on);
		}
	}

	// Every change shows in the panels at once. The panels are redrawn only when their settings changed.
	void Changed() {
		SetLiveInertiaParams(m_values.params);
		if (IsCustom() != m_listedCustom) FillPresets();
		if (!SameViewPart(m_shown, m_values)) {
			ShowPreferencesPreview();
			m_shown.view.TakeFrom(m_values.view.fields);
		}
		ShowState();
		m_callback->on_state_changed();
	}

	void OnCheckChanged(UINT, int id, CWindow) {
		const bool checked = IsDlgButtonChecked(id) == BST_CHECKED;
		if (id == IDC_SCROLL_SMOOTH) m_values.view.fields.smoothScroll = checked;
		else m_values.view.fields.pinHeaders = checked;
		Changed();
	}

	void OnSlider(UINT, UINT, CScrollBar bar) {
		const int index = SettingOfControl(kSliderIds, bar.GetDlgCtrlID());
		if (index < 0 || index >= kSettingCount) return;
		const InertiaSetting& setting = kInertiaSettings[index];
		const double value = ClampInertiaValue(setting, CTrackBarCtrl(bar.m_hWnd).GetPos() / SliderScale(setting));
		if (value == m_values.params.*setting.field) return;
		m_values.params.*setting.field = value;
		ShowNumber(index);
		Changed();
	}

	void OnNumberTyped(UINT, int id, CWindow box) {
		const int index = SettingOfControl(kValueIds, id);
		if (m_showing || index < 0 || index >= kSettingCount) return;
		CString text;
		box.GetWindowText(text);
		const InertiaSetting& setting = kInertiaSettings[index];
		const std::optional<double> value = ParseValue(setting, std::wstring(text));
		if (!value || *value == m_values.params.*setting.field) return;
		m_values.params.*setting.field = *value;
		CTrackBarCtrl(GetDlgItem(kSliderIds[index])).SetPos(SliderPosition(setting, *value));
		Changed();
	}

	// A box left with a half-typed or out-of-range number shows the number in use again.
	void OnNumberLeft(UINT, int id, CWindow) {
		const int index = SettingOfControl(kValueIds, id);
		if (index >= 0 && index < kSettingCount) ShowNumber(index);
	}

	void OnPresetPicked(UINT, int, CWindow) {
		const int index = CComboBox(GetDlgItem(IDC_SCROLL_PRESET)).GetCurSel();
		const std::vector<ScrollPreset> presets = Presets();
		if (index < 0 || index >= static_cast<int>(presets.size())) return;
		m_values.presetName = presets[index].name;
		m_values.params = presets[index].params;
		ShowValues();
		Changed();
	}

	void OnSaveAsNew(UINT, int, CWindow) {
		CString typed;
		GetDlgItemText(IDC_SCROLL_NEW_NAME, typed);
		const std::string name = pfc::stringcvt::string_utf8_from_wide(typed).get_ptr();
		std::vector<std::string> shownBuiltIns;
		for (const ScrollPreset& preset : BuiltInScrollPresets()) shownBuiltIns.push_back(Tr(preset.name.c_str()));
		const ScrollPresetNameProblem problem = CheckNewScrollPresetName(name, m_values.userPresets, shownBuiltIns);
		if (problem != ScrollPresetNameProblem::None) {
			const std::wstring message = problem == ScrollPresetNameProblem::Empty ? TrW("Type a name for the new preset.")
				: TrW("A preset with this name already exists. Choose another name.");
			MessageBox(message.c_str(), L"GroupPlaylist", MB_OK | MB_ICONINFORMATION);
			GetDlgItem(IDC_SCROLL_NEW_NAME).SetFocus();
			return;
		}
		m_values.userPresets.push_back({ name, m_values.params });
		m_values.presetName = name;
		SetDlgItemText(IDC_SCROLL_NEW_NAME, L"");
		FillPresets();
		Changed();
	}

	void OnOverwrite(UINT, int, CWindow) {
		const std::optional<size_t> index = FindScrollPreset(m_values.userPresets, m_values.presetName);
		if (!index) return;
		m_values.userPresets[*index].params = m_values.params;
		FillPresets();
		Changed();
	}

	// The numbers in use stay. Unless they match the default preset, they are custom then.
	void OnDelete(UINT, int, CWindow) {
		const std::optional<size_t> index = FindScrollPreset(m_values.userPresets, m_values.presetName);
		if (!index) return;
		m_values.userPresets.erase(m_values.userPresets.begin() + static_cast<std::ptrdiff_t>(*index));
		m_values.presetName = kDefaultScrollPresetName;
		FillPresets();
		Changed();
	}

	void OnResetButton(UINT, int, CWindow) {
		const ScrollPreset preset = DefaultScrollPreset();
		m_values.presetName = preset.name;
		m_values.params = preset.params;
		ShowValues();
		Changed();
	}

	const preferences_page_callback::ptr m_callback;
	ScrollValues m_values;
	// The smooth scrolling and pinning the panels show now; only those fields are kept up to date.
	ScrollValues m_shown;
	// The settings as saved when the page last looked, so the page can tell what the header menu changed.
	ScrollValues m_lastSaved;
	// True while the page writes a number into a box, so that write is not read back as typing.
	bool m_showing = false;
	// True from the page's creation until it closes, the time its edits show in the panels.
	bool m_previewing = false;
	// Whether the preset list ends with "Custom (unsaved)", and the name the page offered for those numbers.
	bool m_listedCustom = false;
	std::wstring m_offeredName;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

ScrollingDialog* ScrollingDialog::s_open = nullptr;

class ScrollingPage : public preferences_page_impl<ScrollingDialog> {
public:
	const char* get_name() override { return Tr("Scrolling"); }
	GUID get_guid() override { return guid_scrolling_page; }
	GUID get_parent_guid() override { return guid_preferences_page; }
};

preferences_page_factory_t<ScrollingPage> g_scrolling_page_factory;

} // namespace

void ReloadScrollingPage() {
	if (ScrollingDialog::s_open) ScrollingDialog::s_open->ReloadSaved();
}

void ShowScrollingPage() {
	ui_control::get()->show_preferences(guid_scrolling_page);
}

void TakeScrollingPageChanges() {
	if (ScrollingDialog::s_open) ScrollingDialog::s_open->TakeMenuToggles();
}

ViewSettings WithScrollingPageEdits(const ViewSettings& view) {
	return ScrollingDialog::s_open ? ScrollingDialog::s_open->WithEdits(view) : view;
}
