#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include "checked_list.h"
#include "link_button.h"
#include "pattern_tools.h"
#include "playlist_view.h"
#include "preferences_pages.h"
#include "preset_store.h"
#include "resource.h"
#include "ui_language.h"

// Preferences > Tools > GroupPlaylist > Groups: the group presets.
// The page edits a copy. Apply saves it and updates every panel.

namespace {

// The preset the page selects when it opens. ShowGroupsPage sets it.
std::string g_presetToSelect;

std::vector<std::string> SplitNonEmptyLines(const char* text) {
	std::vector<std::string> lines;
	std::string line;
	for (const char* walk = text;; ++walk) {
		if (*walk == '\n' || *walk == '\0') {
			if (!line.empty()) lines.push_back(line);
			line.clear();
			if (*walk == '\0') break;
		} else if (*walk != '\r') {
			line += *walk;
		}
	}
	return lines;
}

std::string JoinLines(const std::vector<std::string>& lines) {
	std::string text;
	for (const std::string& line : lines) {
		if (!text.empty()) text += "\r\n";
		text += line;
	}
	return text;
}

// The line of a multi-line edit control that holds the caret.
std::string CaretLine(CEdit edit) {
	const int lineStart = edit.LineIndex(-1);
	std::wstring line(static_cast<size_t>(edit.LineLength(lineStart)) + 1, L'\0');
	const int copied = edit.GetLine(edit.LineFromChar(lineStart), line.data(), static_cast<int>(line.size()));
	line.resize(static_cast<size_t>((std::max)(copied, 0)));
	return pfc::stringcvt::string_utf8_from_os(line.c_str()).get_ptr();
}

int PatternFieldOfInsertButton(int buttonId) {
	switch (buttonId) {
	case IDC_PRESET_HEADER_INSERT: return IDC_PRESET_HEADER;
	case IDC_PRESET_COVER_INSERT: return IDC_PRESET_COVER;
	case IDC_PRESET_SORT_INSERT: return IDC_PRESET_SORT;
	default: return IDC_PRESET_SUBGROUPS;
	}
}

class GroupsDialog : public CDialogImpl<GroupsDialog>, public preferences_page_instance {
public:
	enum { IDD = IDD_GROUPS_PAGE };

	explicit GroupsDialog(preferences_page_callback::ptr callback) : m_callback(callback) {
		const std::vector<GroupPreset> presets = LoadGroupPresets();
		SetRows(presets);
		s_open = this;
		m_initialIndex = static_cast<int>(FindPresetByName(presets, g_presetToSelect).value_or(0));
		g_presetToSelect.clear();
	}

	t_uint32 get_state() override {
		t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
		if (SerializePresets(Presets()) != SerializePresets(LoadGroupPresets())) state |= preferences_state::changed;
		return state;
	}

	void apply() override {
		if (!CheckNames()) return;
		PlaylistView::ApplyGroupPresets(Presets(), Renames());
		for (Row& row : m_rows) row.originalName = row.preset.name;
		m_callback->on_state_changed();
	}

	~GroupsDialog() { if (s_open == this) s_open = nullptr; }

	// The page while it exists. foobar2000 keeps a page that was shown until Preferences closes.
	static GroupsDialog* s_open;

	// Settings were imported: the page drops its edits and shows the saved presets.
	void ReloadSaved() {
		SetRows(LoadGroupPresets());
		if (m_hWnd) FillList(0);
		m_callback->on_state_changed();
	}

	void reset() override {
		SetRows(DefaultGroupPresets());
		FillList(0);
		m_callback->on_state_changed();
	}

	BEGIN_MSG_MAP_EX(GroupsDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		NOTIFY_HANDLER_EX(IDC_PRESET_LIST, LVN_ITEMCHANGED, OnListChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_NAME, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_HEADER, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_COVER, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_SUBGROUPS, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_SORT, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_COVER_TEXT, BN_CLICKED, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_HIDE_UNIFORM, BN_CLICKED, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_PRESET_HEADER, EN_SETFOCUS, OnPatternFocus)
		COMMAND_HANDLER_EX(IDC_PRESET_COVER, EN_SETFOCUS, OnPatternFocus)
		COMMAND_HANDLER_EX(IDC_PRESET_SUBGROUPS, EN_SETFOCUS, OnPatternFocus)
		COMMAND_HANDLER_EX(IDC_PRESET_SORT, EN_SETFOCUS, OnPatternFocus)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_NEW, OnNew)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_DELETE, OnDelete)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_UP, OnMove)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_DOWN, OnMove)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_SYNTAX, OnSyntaxHelp)
		MSG_WM_DRAWITEM(OnDrawItem)
		MSG_WM_SETCURSOR(OnSetCursor)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_HEADER_INSERT, OnInsertField)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_COVER_INSERT, OnInsertField)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_SUBGROUP_INSERT, OnInsertField)
		COMMAND_ID_HANDLER_EX(IDC_PRESET_SORT_INSERT, OnInsertField)
	END_MSG_MAP()

private:
	struct Row {
		GroupPreset preset;
		// The name the preset had when it was last saved. Empty for a preset added on this page.
		std::string originalName;
	};

	void SetRows(const std::vector<GroupPreset>& presets) {
		m_rows.clear();
		for (const GroupPreset& preset : presets) m_rows.push_back({ preset, preset.name });
	}

	std::vector<GroupPreset> Presets() const {
		std::vector<GroupPreset> presets;
		for (const Row& row : m_rows) presets.push_back(row.preset);
		return presets;
	}

	// Old and new name of every saved preset that was renamed, so panels keep using it.
	std::vector<std::pair<std::string, std::string>> Renames() const {
		std::vector<std::pair<std::string, std::string>> renames;
		for (const Row& row : m_rows) {
			if (!row.originalName.empty() && row.originalName != row.preset.name) renames.push_back({ row.originalName, row.preset.name });
		}
		return renames;
	}

	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		m_list.Attach(GetDlgItem(IDC_PRESET_LIST), { { TrW("Name").c_str(), 40 }, { TrW("Group header").c_str(), 60 } });
		FillList(m_initialIndex);
		return FALSE;
	}

	void FillList(int select) {
		std::vector<std::vector<std::string>> texts;
		std::vector<bool> checks;
		for (const Row& row : m_rows) {
			texts.push_back({ row.preset.name, row.preset.headerPattern });
			checks.push_back(row.preset.showInMenu);
		}
		m_list.Fill(texts, checks, select);
		LoadFields();
	}

	int SelectedIndex() const {
		const int index = m_list.Selected();
		return index >= 0 && index < static_cast<int>(m_rows.size()) ? index : -1;
	}

	void LoadFields() {
		const int index = SelectedIndex();
		const bool hasRow = index >= 0;
		const GroupPreset shown = hasRow ? m_rows[index].preset : GroupPreset{};
		m_loadingFields = true;
		uSetDlgItemText(m_hWnd, IDC_PRESET_NAME, shown.name.c_str());
		uSetDlgItemText(m_hWnd, IDC_PRESET_HEADER, shown.headerPattern.c_str());
		uSetDlgItemText(m_hWnd, IDC_PRESET_COVER, shown.coverPattern.c_str());
		uSetDlgItemText(m_hWnd, IDC_PRESET_SUBGROUPS, JoinLines(shown.subgroupPatterns).c_str());
		uSetDlgItemText(m_hWnd, IDC_PRESET_SORT, shown.sortPattern.c_str());
		CheckDlgButton(IDC_PRESET_COVER_TEXT, shown.showCoverText ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(IDC_PRESET_HIDE_UNIFORM, shown.hideUniformLabels ? BST_CHECKED : BST_UNCHECKED);
		m_loadingFields = false;
		for (const int id : { IDC_PRESET_NAME, IDC_PRESET_HEADER, IDC_PRESET_HEADER_INSERT, IDC_PRESET_COVER, IDC_PRESET_COVER_INSERT,
		                      IDC_PRESET_COVER_TEXT, IDC_PRESET_SUBGROUPS, IDC_PRESET_SUBGROUP_INSERT, IDC_PRESET_HIDE_UNIFORM,
		                      IDC_PRESET_SORT, IDC_PRESET_SORT_INSERT }) {
			GetDlgItem(id).EnableWindow(hasRow);
		}
		// A panel always needs a preset to show, so the last one stays.
		GetDlgItem(IDC_PRESET_DELETE).EnableWindow(m_rows.size() > 1 && hasRow);
		GetDlgItem(IDC_PRESET_UP).EnableWindow(index > 0);
		GetDlgItem(IDC_PRESET_DOWN).EnableWindow(hasRow && index + 1 < static_cast<int>(m_rows.size()));
		UpdatePreview();
	}

	// Two lines: what the pattern field the user touched last does, and what it shows for the focused track.
	void UpdatePreview() {
		const int index = SelectedIndex();
		std::string text;
		if (index >= 0) {
			const GroupPreset& preset = m_rows[index].preset;
			std::string pattern = preset.headerPattern;
			const char* meaning = "Header: tracks with the same value share one header."; // ui-text
			if (m_previewField == IDC_PRESET_COVER) {
				pattern = preset.coverPattern;
				meaning = "Cover: tracks with the same value share one cover."; // ui-text
			} else if (m_previewField == IDC_PRESET_SUBGROUPS) {
				pattern = CaretLine(CEdit(GetDlgItem(IDC_PRESET_SUBGROUPS)));
				meaning = "Labels: a label row starts where the value changes."; // ui-text
			} else if (m_previewField == IDC_PRESET_SORT) {
				pattern = preset.sortPattern;
				meaning = "Sort: the order this preset sorts the playlist in."; // ui-text
			}
			text = std::string(Tr(meaning)) + "\n" + DescribePatternPreview(pattern.c_str());
		}
		uSetDlgItemText(m_hWnd, IDC_PRESET_PREVIEW, text.c_str());
	}

	LRESULT OnListChanged(LPNMHDR header) {
		switch (m_list.Classify(header)) {
		case CheckedList::Change::Selection:
			LoadFields();
			break;
		case CheckedList::Change::Check: {
			const int index = reinterpret_cast<const NMLISTVIEW*>(header)->iItem;
			if (index >= 0 && index < static_cast<int>(m_rows.size())) {
				m_rows[index].preset.showInMenu = m_list.IsChecked(index);
				m_callback->on_state_changed();
			}
			break;
		}
		default:
			break;
		}
		return 0;
	}

	void OnPatternFocus(UINT, int id, CWindow) {
		m_previewField = id;
		UpdatePreview();
	}

	void OnFieldChanged(UINT, int id, CWindow) {
		const int index = SelectedIndex();
		if (m_loadingFields || index < 0) return;
		GroupPreset& preset = m_rows[index].preset;
		if (id == IDC_PRESET_COVER_TEXT) {
			preset.showCoverText = IsDlgButtonChecked(IDC_PRESET_COVER_TEXT) == BST_CHECKED;
		} else if (id == IDC_PRESET_HIDE_UNIFORM) {
			preset.hideUniformLabels = IsDlgButtonChecked(IDC_PRESET_HIDE_UNIFORM) == BST_CHECKED;
		} else {
			pfc::string8 text;
			uGetDlgItemText(m_hWnd, id, text);
			switch (id) {
			case IDC_PRESET_NAME:
				preset.name = text.c_str();
				m_list.SetText(index, 0, preset.name);
				break;
			case IDC_PRESET_HEADER:
				preset.headerPattern = text.c_str();
				m_list.SetText(index, 1, preset.headerPattern);
				break;
			case IDC_PRESET_COVER: preset.coverPattern = text.c_str(); break;
			case IDC_PRESET_SORT: preset.sortPattern = text.c_str(); break;
			default: preset.subgroupPatterns = SplitNonEmptyLines(text.c_str()); break;
			}
			if (id != IDC_PRESET_NAME) {
				m_previewField = id;
				UpdatePreview();
			}
		}
		m_callback->on_state_changed();
	}

	void OnInsertField(UINT, int id, CWindow) {
		RunInsertFieldMenu(GetDlgItem(id), CEdit(GetDlgItem(PatternFieldOfInsertButton(id))));
	}

	std::string UnusedName() const {
		for (int number = 1;; ++number) {
			const std::string base = Tr("New preset");
			const std::string candidate = number == 1 ? base : base + " " + std::to_string(number);
			bool taken = false;
			for (const Row& row : m_rows) taken = taken || SamePresetName(row.preset.name, candidate);
			if (!taken) return candidate;
		}
	}

	// A new preset starts as a copy of the selected one, because most presets differ from another in one field.
	// It goes below the selected one.
	void OnNew(UINT, int, CWindow) {
		const int selected = SelectedIndex();
		GroupPreset preset = selected >= 0 ? m_rows[selected].preset : GroupPreset{ "", "%path_sort%", "[%album%]", "", false, {} };
		preset.name = UnusedName();
		preset.showInMenu = true;
		const int index = selected >= 0 ? selected + 1 : static_cast<int>(m_rows.size());
		m_rows.insert(m_rows.begin() + index, Row{ preset, "" });
		FillList(index);
		m_callback->on_state_changed();
		CEdit name(GetDlgItem(IDC_PRESET_NAME));
		name.SetFocus();
		name.SetSel(0, -1);
	}

	void OnDelete(UINT, int, CWindow) {
		const int index = SelectedIndex();
		if (index < 0 || m_rows.size() <= 1) return;
		m_rows.erase(m_rows.begin() + index);
		FillList(index);
		m_callback->on_state_changed();
	}

	void OnMove(UINT, int id, CWindow) {
		const int index = SelectedIndex();
		const int moved = MoveItem(m_rows, index, id == IDC_PRESET_UP ? -1 : 1);
		if (moved == index) return;
		FillList(moved);
		m_callback->on_state_changed();
	}

	// Opens the title formatting reference that ships with foobar2000. A link, not a button, because it opens help and changes nothing.
	void OnSyntaxHelp(UINT, int, CWindow) {
		standard_commands::main_titleformat_help();
	}

	void OnDrawItem(int id, LPDRAWITEMSTRUCT item) {
		if (id == IDC_PRESET_SYNTAX) DrawLinkButton(*item, m_hWnd);
		else SetMsgHandled(FALSE);
	}

	BOOL OnSetCursor(CWindow window, UINT, UINT) {
		if (SetLinkCursor(window, GetDlgItem(IDC_PRESET_SYNTAX))) return TRUE;
		SetMsgHandled(FALSE);
		return FALSE;
	}

	// A bad name is shown and nothing is saved, so the user can fix it and press Apply again.
	bool CheckNames() {
		const PresetNameCheck check = CheckPresetNames(Presets());
		if (check.problem == PresetNameProblem::None) return true;
		const std::wstring message = TrW(check.problem == PresetNameProblem::Empty
			? "Every preset needs a name. The presets were not saved."
			: "Two presets have this name. Choose another name. The presets were not saved.");
		if (m_hWnd != NULL) {
			m_list.Select(static_cast<int>(check.index));
			LoadFields();
			MessageBox(message.c_str(), TrW("Groups").c_str(), MB_OK | MB_ICONWARNING);
			GetDlgItem(IDC_PRESET_NAME).SetFocus();
		}
		return false;
	}

	const preferences_page_callback::ptr m_callback;
	std::vector<Row> m_rows;
	int m_initialIndex = 0;
	CheckedList m_list;
	// The pattern field the preview describes: the one the user touched last.
	int m_previewField = IDC_PRESET_HEADER;
	// True while the page itself writes to the fields, so those writes are not read back as edits.
	bool m_loadingFields = false;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

class GroupsPage : public preferences_page_impl<GroupsDialog> {
public:
	const char* get_name() override { return Tr("Groups"); }
	GUID get_guid() override { return guid_groups_page; }
	GUID get_parent_guid() override { return guid_preferences_page; }
};

preferences_page_factory_t<GroupsPage> g_groups_page_factory;

} // namespace

GroupsDialog* GroupsDialog::s_open = nullptr;

void ReloadGroupsPage() {
	if (GroupsDialog::s_open) GroupsDialog::s_open->ReloadSaved();
}

void ShowGroupsPage(const std::string& selectPreset) {
	g_presetToSelect = selectPreset;
	ui_control::get()->show_preferences(guid_groups_page);
}
