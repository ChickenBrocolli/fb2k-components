#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include "checked_list.h"
#include "link_button.h"
#include "column_store.h"
#include "pattern_tools.h"
#include "playlist_view.h"
#include "preferences_pages.h"
#include "resource.h"
#include "ui_language.h"

// Preferences > Tools > GroupPlaylist > Columns.
// The list holds the built-in columns first, in menu order, then the custom columns.
// Built-in columns keep their place and cannot be removed, but their shown name, pattern and alignment can be changed.
// Restore default puts a built-in column back as it came.
// The page edits a copy. Apply saves it and updates every panel.

namespace {

const char* const kAlignmentLabels[] = { "Left", "Center", "Right" }; // ui-text

std::vector<std::string> BuiltInColumnNames() {
	std::vector<std::string> names;
	for (const ColumnDef& def : BuiltInColumnDefs()) names.push_back(def.name);
	return names;
}

// The names of the built-in columns that are not in the menu, in menu order.
std::vector<std::string> HiddenNames(const std::vector<ColumnDef>& builtInDefs) {
	std::vector<std::string> names;
	for (const ColumnDef& def : builtInDefs) {
		if (!def.showInMenu) names.push_back(def.name);
	}
	return names;
}

std::vector<std::string> SavedHiddenNames() {
	return HiddenNames(BuiltInColumnDefsForMenu(LoadHiddenBuiltInColumns()));
}

// What the Pattern column of the list says. The Rating column has no pattern.
std::string PatternText(const ColumnDef& def) {
	return def.kind == ColumnKind::Rating ? Tr("(stars you click to rate)") : def.pattern;
}

class ColumnsDialog : public CDialogImpl<ColumnsDialog>, public preferences_page_instance {
public:
	enum { IDD = IDD_COLUMNS_PAGE };

	explicit ColumnsDialog(preferences_page_callback::ptr callback)
		: m_callback(callback), m_reservedNames(BuiltInColumnNames()), m_builtInCount(m_reservedNames.size()) {
		SetRows(SavedBuiltInColumnDefs(), LoadCustomColumnDefs());
		s_open = this;
	}
	~ColumnsDialog() { if (s_open == this) s_open = nullptr; }

	// The page while it exists. foobar2000 keeps a page that was shown until Preferences closes.
	static ColumnsDialog* s_open;

	// Settings were imported: the page drops its edits and shows the saved columns.
	void ReloadSaved() {
		SetRows(SavedBuiltInColumnDefs(), LoadCustomColumnDefs());
		if (m_hWnd) FillList(FirstCustomRowOr(0));
		m_callback->on_state_changed();
	}

	t_uint32 get_state() override {
		t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
		const bool changed = SerializeColumnDefs(CustomColumns()) != SerializeColumnDefs(LoadCustomColumnDefs()) ||
			HiddenBuiltIns() != SavedHiddenNames() ||
			SerializeBuiltInEdits(BuiltInEdits()) != SerializeBuiltInEdits(BuiltInEditsFrom(BuiltInColumnDefs(), SavedBuiltInColumnDefs()));
		if (changed) state |= preferences_state::changed;
		return state;
	}

	void apply() override {
		if (!CheckNames()) return;
		SaveHiddenBuiltInColumns(HiddenBuiltIns());
		SaveBuiltInColumnEdits(BuiltInEdits());
		PlaylistView::ApplyCustomColumns(CustomColumns(), Renames());
		for (Row& row : m_rows) row.originalName = row.def.name;
		m_callback->on_state_changed();
	}

	void reset() override {
		SetRows(BuiltInColumnDefs(), DefaultCustomColumnDefs());
		FillList(FirstCustomRowOr(0));
		m_callback->on_state_changed();
	}

	BEGIN_MSG_MAP_EX(ColumnsDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		NOTIFY_HANDLER_EX(IDC_COLUMN_LIST, LVN_ITEMCHANGED, OnListChanged)
		COMMAND_HANDLER_EX(IDC_COLUMN_NAME, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_COLUMN_PATTERN, EN_CHANGE, OnFieldChanged)
		COMMAND_HANDLER_EX(IDC_COLUMN_ALIGN, CBN_SELCHANGE, OnFieldChanged)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_NEW, OnNew)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_DUPLICATE, OnDuplicate)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_DELETE, OnDelete)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_RESTORE, OnRestore)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_UP, OnMove)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_DOWN, OnMove)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_INSERT, OnInsertField)
		COMMAND_ID_HANDLER_EX(IDC_COLUMN_SYNTAX, OnSyntaxHelp)
		MSG_WM_DRAWITEM(OnDrawItem)
		MSG_WM_SETCURSOR(OnSetCursor)
	END_MSG_MAP()

private:
	struct Row {
		ColumnDef def;
		// The name the column had when it was last saved. Empty for a column added on this page.
		std::string originalName;
	};

	void SetRows(const std::vector<ColumnDef>& builtInDefs, const std::vector<ColumnDef>& customDefs) {
		m_rows.clear();
		for (const ColumnDef& def : builtInDefs) m_rows.push_back({ def, def.name });
		for (const ColumnDef& def : customDefs) m_rows.push_back({ def, def.name });
	}

	bool IsBuiltIn(int index) const { return index >= 0 && static_cast<size_t>(index) < m_builtInCount; }

	int FirstCustomRowOr(int fallback) const {
		return m_rows.size() > m_builtInCount ? static_cast<int>(m_builtInCount) : fallback;
	}

	std::vector<ColumnDef> CustomColumns() const {
		std::vector<ColumnDef> defs;
		for (size_t index = m_builtInCount; index < m_rows.size(); ++index) defs.push_back(m_rows[index].def);
		return defs;
	}

	std::vector<ColumnDef> BuiltInEdits() const {
		std::vector<ColumnDef> defs;
		for (size_t index = 0; index < m_builtInCount && index < m_rows.size(); ++index) defs.push_back(m_rows[index].def);
		return BuiltInEditsFrom(BuiltInColumnDefs(), defs);
	}

	// The built-in column as it came. The rows of the built-in columns are in the same order as BuiltInColumnDefs.
	ColumnDef OriginalBuiltIn(int index) const {
		return BuiltInColumnDefs()[static_cast<size_t>(index)];
	}

	bool IsEditedRow(int index) const {
		return IsBuiltIn(index) && IsEditedBuiltIn(m_rows[index].def, OriginalBuiltIn(index));
	}

	std::string KindText(int index) const {
		return Tr(!IsBuiltIn(index) ? "Custom" : IsEditedRow(index) ? "Built-in (edited)" : "Built-in");
	}

	std::vector<std::string> HiddenBuiltIns() const {
		std::vector<ColumnDef> defs;
		for (size_t index = 0; index < m_builtInCount && index < m_rows.size(); ++index) defs.push_back(m_rows[index].def);
		return HiddenNames(defs);
	}

	// Old and new name of every saved custom column that was renamed, so panels keep showing it.
	std::vector<std::pair<std::string, std::string>> Renames() const {
		std::vector<std::pair<std::string, std::string>> renames;
		for (size_t index = m_builtInCount; index < m_rows.size(); ++index) {
			const Row& row = m_rows[index];
			if (!row.originalName.empty() && row.originalName != row.def.name) renames.push_back({ row.originalName, row.def.name });
		}
		return renames;
	}

	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		m_list.Attach(GetDlgItem(IDC_COLUMN_LIST), { { TrW("Name").c_str(), 28 }, { TrW("Pattern").c_str(), 46 }, { TrW("Kind").c_str(), 26 } });
		CComboBox alignment(GetDlgItem(IDC_COLUMN_ALIGN));
		for (const char* label : kAlignmentLabels) alignment.AddString(TrW(label).c_str());
		// The custom columns are what people come here to change, so the page opens on the first of them.
		FillList(FirstCustomRowOr(0));
		return FALSE;
	}

	void FillList(int select) {
		std::vector<std::vector<std::string>> texts;
		std::vector<bool> checks;
		for (size_t index = 0; index < m_rows.size(); ++index) {
			const ColumnDef& def = m_rows[index].def;
			texts.push_back({ ColumnTitle(def), PatternText(def), KindText(static_cast<int>(index)) });
			checks.push_back(def.showInMenu);
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
		const bool custom = hasRow && !IsBuiltIn(index);
		// Only text columns take a pattern: Duration draws the time played or left, and Rating's stars come from code.
		// The stars are drawn from the left, so Rating keeps its alignment too.
		const bool patternEditable = hasRow && m_rows[index].def.kind == ColumnKind::Text;
		const bool alignmentEditable = hasRow && m_rows[index].def.kind != ColumnKind::Rating;
		m_loadingFields = true;
		uSetDlgItemText(m_hWnd, IDC_COLUMN_NAME, hasRow ? ColumnTitle(m_rows[index].def).c_str() : "");
		uSetDlgItemText(m_hWnd, IDC_COLUMN_PATTERN, hasRow ? PatternText(m_rows[index].def).c_str() : "");
		CComboBox(GetDlgItem(IDC_COLUMN_ALIGN)).SetCurSel(hasRow ? static_cast<int>(m_rows[index].def.alignment) : -1);
		m_loadingFields = false;
		for (const int id : { IDC_COLUMN_NAME, IDC_COLUMN_PATTERN }) GetDlgItem(id).EnableWindow(hasRow);
		CEdit(GetDlgItem(IDC_COLUMN_PATTERN)).SetReadOnly(!patternEditable);
		GetDlgItem(IDC_COLUMN_INSERT).EnableWindow(patternEditable);
		GetDlgItem(IDC_COLUMN_ALIGN).EnableWindow(alignmentEditable);
		GetDlgItem(IDC_COLUMN_DELETE).EnableWindow(custom);
		GetDlgItem(IDC_COLUMN_RESTORE).EnableWindow(IsEditedRow(index));
		// A copy of the Rating column would be an empty text column, because its stars come from code, not a pattern.
		GetDlgItem(IDC_COLUMN_DUPLICATE).EnableWindow(hasRow && m_rows[index].def.kind != ColumnKind::Rating);
		GetDlgItem(IDC_COLUMN_UP).EnableWindow(custom && index > static_cast<int>(m_builtInCount));
		GetDlgItem(IDC_COLUMN_DOWN).EnableWindow(custom && index + 1 < static_cast<int>(m_rows.size()));
		UpdatePreview();
	}

	void UpdatePreview() {
		const int index = SelectedIndex();
		std::string text;
		if (index >= 0) {
			const ColumnDef& def = m_rows[index].def;
			if (def.kind == ColumnKind::Rating) text = Tr("The main page sets where ratings are kept.");
			else text = DescribePatternPreview(def.pattern.c_str());
		}
		uSetDlgItemText(m_hWnd, IDC_COLUMN_PREVIEW, text.c_str());
	}

	LRESULT OnListChanged(LPNMHDR header) {
		switch (m_list.Classify(header)) {
		case CheckedList::Change::Selection:
			LoadFields();
			break;
		case CheckedList::Change::Check: {
			const int index = reinterpret_cast<const NMLISTVIEW*>(header)->iItem;
			if (index >= 0 && index < static_cast<int>(m_rows.size())) {
				m_rows[index].def.showInMenu = m_list.IsChecked(index);
				m_callback->on_state_changed();
			}
			break;
		}
		default:
			break;
		}
		return 0;
	}

	void OnFieldChanged(UINT, int id, CWindow) {
		const int index = SelectedIndex();
		if (m_loadingFields || index < 0) return;
		ColumnDef& def = m_rows[index].def;
		pfc::string8 text;
		if (id == IDC_COLUMN_NAME) {
			uGetDlgItemText(m_hWnd, IDC_COLUMN_NAME, text);
			// A built-in column keeps its name, which panels find it by, and shows the given one.
			// An empty name or the built-in one shows the built-in name in the current language again.
			if (IsBuiltIn(index)) {
				const std::string given = text.c_str();
				def.title = given.empty() || given == def.name || given == Tr(def.name.c_str()) ? "" : given;
			} else {
				def.name = text.c_str();
			}
			m_list.SetText(index, 0, ColumnTitle(def));
		} else if (id == IDC_COLUMN_PATTERN) {
			if (def.kind != ColumnKind::Text) return;
			uGetDlgItemText(m_hWnd, IDC_COLUMN_PATTERN, text);
			def.pattern = text.c_str();
			m_list.SetText(index, 1, def.pattern);
			UpdatePreview();
		} else {
			if (def.kind == ColumnKind::Rating) return;
			const int alignment = CComboBox(GetDlgItem(IDC_COLUMN_ALIGN)).GetCurSel();
			if (alignment >= 0) def.alignment = static_cast<ColumnAlignment>(alignment);
		}
		m_list.SetText(index, 2, KindText(index));
		GetDlgItem(IDC_COLUMN_RESTORE).EnableWindow(IsEditedRow(index));
		m_callback->on_state_changed();
	}

	// base, then "base 2", "base 3" and so on, until no column has the name.
	std::string UnusedName(const std::string& base) const {
		for (int number = 1;; ++number) {
			const std::string candidate = number == 1 ? base : base + " " + std::to_string(number);
			bool taken = false;
			for (const Row& row : m_rows) taken = taken || row.def.name == candidate;
			if (!taken) return candidate;
		}
	}

	// A new custom column goes below the selected custom column, or at the end.
	void InsertCustom(ColumnDef def) {
		const int selected = SelectedIndex();
		const int index = selected >= 0 && !IsBuiltIn(selected) ? selected + 1 : static_cast<int>(m_rows.size());
		def.kind = ColumnKind::Text;
		def.showInMenu = true;
		m_rows.insert(m_rows.begin() + index, Row{ def, "" });
		FillList(index);
		m_callback->on_state_changed();
		CEdit name(GetDlgItem(IDC_COLUMN_NAME));
		name.SetFocus();
		name.SetSel(0, -1);
	}

	void OnNew(UINT, int, CWindow) {
		InsertCustom({ UnusedName(Tr("New column")), "", ColumnAlignment::Left, 100 });
	}

	void OnDuplicate(UINT, int, CWindow) {
		const int index = SelectedIndex();
		if (index < 0 || m_rows[index].def.kind == ColumnKind::Rating) return;
		ColumnDef copy = m_rows[index].def;
		copy.name = UnusedName(ColumnTitle(copy) + Tr(" copy"));
		copy.title.clear();
		InsertCustom(copy);
	}

	// The selected built-in column as it came, still in or out of the menu as the user ticked it.
	void OnRestore(UINT, int, CWindow) {
		const int index = SelectedIndex();
		if (!IsEditedRow(index)) return;
		ColumnDef original = OriginalBuiltIn(index);
		original.showInMenu = m_rows[index].def.showInMenu;
		m_rows[index].def = original;
		FillList(index);
		m_callback->on_state_changed();
	}

	void OnDelete(UINT, int, CWindow) {
		const int index = SelectedIndex();
		if (index < 0 || IsBuiltIn(index)) return;
		m_rows.erase(m_rows.begin() + index);
		FillList((std::min)(index, static_cast<int>(m_rows.size()) - 1));
		m_callback->on_state_changed();
	}

	// Custom columns move among themselves. The built-in columns keep the menu order.
	void OnMove(UINT, int id, CWindow) {
		const int index = SelectedIndex();
		const int step = id == IDC_COLUMN_UP ? -1 : 1;
		if (IsBuiltIn(index) || IsBuiltIn(index + step)) return;
		const int moved = MoveItem(m_rows, index, step);
		if (moved == index) return;
		FillList(moved);
		m_callback->on_state_changed();
	}

	void OnInsertField(UINT, int, CWindow) {
		RunInsertFieldMenu(GetDlgItem(IDC_COLUMN_INSERT), CEdit(GetDlgItem(IDC_COLUMN_PATTERN)));
	}

	// Opens the title formatting reference that ships with foobar2000. A link, not a button, because it opens help and changes nothing.
	void OnSyntaxHelp(UINT, int, CWindow) {
		standard_commands::main_titleformat_help();
	}

	void OnDrawItem(int id, LPDRAWITEMSTRUCT item) {
		if (id == IDC_COLUMN_SYNTAX) DrawLinkButton(*item, m_hWnd);
		else SetMsgHandled(FALSE);
	}

	BOOL OnSetCursor(CWindow window, UINT, UINT) {
		if (SetLinkCursor(window, GetDlgItem(IDC_COLUMN_SYNTAX))) return TRUE;
		SetMsgHandled(FALSE);
		return FALSE;
	}

	// A bad name is shown and nothing is saved, so the user can fix it and press Apply again.
	bool CheckNames() {
		const ColumnNameCheck check = CheckColumnNames(CustomColumns(), m_reservedNames);
		if (check.problem == ColumnNameProblem::None) return true;
		const std::wstring message = TrW(
			check.problem == ColumnNameProblem::Empty ? "Every column needs a name. The columns were not saved." :
			check.problem == ColumnNameProblem::BuiltIn ? "This name belongs to a built-in column. Choose another name. The columns were not saved." :
			"Two columns have this name. Choose another name. The columns were not saved.");
		if (m_hWnd != NULL) {
			m_list.Select(static_cast<int>(m_builtInCount + check.index));
			LoadFields();
			MessageBox(message.c_str(), TrW("Columns").c_str(), MB_OK | MB_ICONWARNING);
			GetDlgItem(IDC_COLUMN_NAME).SetFocus();
		}
		return false;
	}

	const preferences_page_callback::ptr m_callback;
	const std::vector<std::string> m_reservedNames;
	const size_t m_builtInCount;
	std::vector<Row> m_rows;
	CheckedList m_list;
	// True while the page itself writes to the fields, so those writes are not read back as edits.
	bool m_loadingFields = false;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

class ColumnsPage : public preferences_page_impl<ColumnsDialog> {
public:
	const char* get_name() override { return Tr("Columns"); }
	GUID get_guid() override { return guid_columns_page; }
	GUID get_parent_guid() override { return guid_preferences_page; }
};

preferences_page_factory_t<ColumnsPage> g_columns_page_factory;

ColumnsDialog* ColumnsDialog::s_open = nullptr;

} // namespace

void ReloadColumnsPage() {
	if (ColumnsDialog::s_open) ColumnsDialog::s_open->ReloadSaved();
}

void ShowColumnsPage() {
	ui_control::get()->show_preferences(guid_columns_page);
}
