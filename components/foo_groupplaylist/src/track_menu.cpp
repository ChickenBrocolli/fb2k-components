#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>

#include "resource.h"
#include "selection_order.h"
#include "track_menu.h"
#include "ui_language.h"

namespace {

enum : UINT {
	kPlayCommand = 1, kRemoveCommand, kCropCommand, kSortByCommand, kRandomizeCommand, kReverseCommand,
	kCutCommand, kCopyCommand, kPasteCommand,
	kFirstFixedSortCommand = 100,
	kFirstSharedCommand = 1000,
};

struct FixedSort {
	const char* label;
	const char* pattern;
};

const FixedSort kFixedSorts[] = {
	{ "Sort by album", "%album%" },
	{ "Sort by artist", "%artist%" },
	{ "Sort by file path", "%path%" },
	{ "Sort by title", "%title%" },
	{ "Sort by track number", "%tracknumber%" },
};

// The pattern last used in Sort by..., kept while foobar2000 runs.
std::string g_sortPattern = "%album artist% | %date% | %album% | %discnumber% | %tracknumber%";

class SortByDialog : public CDialogImpl<SortByDialog> {
public:
	enum { IDD = IDD_SORT_BY };

	explicit SortByDialog(std::string pattern) : m_pattern(std::move(pattern)) {}

	const std::string& Pattern() const { return m_pattern; }

	BEGIN_MSG_MAP_EX(SortByDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnButton)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnButton)
	END_MSG_MAP()

private:
	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		SetWindowText(TrW("Sort by").c_str());
		SetDlgItemText(IDC_SORT_PATTERN, pfc::stringcvt::string_os_from_utf8(m_pattern.c_str()));
		CenterWindow(GetParent());
		return TRUE;
	}

	void OnButton(UINT, int id, CWindow) {
		if (id == IDOK) {
			CString text;
			GetDlgItemText(IDC_SORT_PATTERN, text);
			m_pattern = pfc::stringcvt::string_utf8_from_os(text).get_ptr();
		}
		EndDialog(id);
	}

	std::string m_pattern;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

// Sort, Randomize and Reverse move only the selected tracks, among the places they hold.
void SortSelection(const char* pattern) {
	const auto playlists = playlist_manager::get();
	playlists->activeplaylist_undo_backup();
	playlists->activeplaylist_sort_by_format(pattern, true);
}

void ReverseSelection() {
	const auto playlists = playlist_manager::get();
	const size_t count = playlists->activeplaylist_get_item_count();
	pfc::bit_array_bittable mask(count);
	playlists->activeplaylist_get_selection_mask(mask);
	std::vector<bool> selected(count);
	for (size_t i = 0; i < count; ++i) selected[i] = mask.get(i);
	const std::vector<size_t> order = ReversedSelectionOrder(selected);
	playlists->activeplaylist_undo_backup();
	playlists->activeplaylist_reorder_items(order.data(), order.size());
}

void AskAndSortSelection(HWND owner) {
	SortByDialog dialog(g_sortPattern);
	if (dialog.DoModal(owner) != IDOK || dialog.Pattern().empty()) return;
	g_sortPattern = dialog.Pattern();
	SortSelection(g_sortPattern.c_str());
}

void RemoveSelection(bool crop) {
	const auto playlists = playlist_manager::get();
	playlists->activeplaylist_undo_backup();
	playlists->activeplaylist_remove_selection(crop);
}

// Paste takes only tracks foobar2000 put on the clipboard, so only those make Paste available.
bool ClipboardHoldsTracks() {
	pfc::com_ptr_t<IDataObject> clipboard;
	if (FAILED(OleGetClipboard(clipboard.receive_ptr()))) return false;
	DWORD effect = DROPEFFECT_COPY;
	bool native = false;
	return SUCCEEDED(ole_interaction::get()->check_dataobject(clipboard, effect, native)) && native;
}

} // namespace

TrackMenuCommand RunTrackMenu(HWND owner, CPoint screenPoint, bool canPlay) {
	const auto playlists = playlist_manager::get();
	const size_t selected = playlists->activeplaylist_get_selection_count(SIZE_MAX);
	// An autoplaylist or another locked playlist refuses some changes. Their items are greyed, as in the Edit menu.
	const t_uint32 locked = playlists->activeplaylist_lock_get_filter_mask();
	const bool canRemove = selected > 0 && (locked & playlist_lock::filter_remove) == 0;
	const bool canSort = selected > 1 && (locked & playlist_lock::filter_reorder) == 0;
	const bool canPaste = (locked & playlist_lock::filter_add) == 0 && ClipboardHoldsTracks();
	const auto item = [](bool enabled) -> UINT { return MF_STRING | (enabled ? 0 : MF_GRAYED); };
	const auto label = [](const char* english, const wchar_t* key = nullptr) {
		std::wstring text = TrW(english);
		if (key) text += std::wstring(L"\t") + key;
		return text;
	};

	CMenu sort;
	sort.CreatePopupMenu();
	sort.AppendMenu(MF_STRING, kSortByCommand, label("Sort by...").c_str());
	sort.AppendMenu(MF_STRING, kRandomizeCommand, label("Randomize").c_str());
	sort.AppendMenu(MF_STRING, kReverseCommand, label("Reverse").c_str());
	sort.AppendMenu(MF_SEPARATOR);
	for (size_t index = 0; index < std::size(kFixedSorts); ++index) {
		sort.AppendMenu(MF_STRING, kFirstFixedSortCommand + static_cast<UINT>(index), label(kFixedSorts[index].label).c_str());
	}

	CMenu menu;
	menu.CreatePopupMenu();
	menu.AppendMenu(item(canPlay), kPlayCommand, label("Play").c_str());
	if (canPlay) menu.SetMenuDefaultItem(kPlayCommand);
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(item(canRemove), kRemoveCommand, label("Remove", L"Delete").c_str());
	menu.AppendMenu(item(canRemove), kCropCommand, label("Crop").c_str());
	const UINT sortPosition = static_cast<UINT>(menu.GetMenuItemCount());
	menu.AppendMenu(MF_POPUP, sort.Detach(), label("Sort").c_str());
	menu.AppendMenu(MF_SEPARATOR);
	menu.AppendMenu(item(canRemove), kCutCommand, label("Cut", L"Ctrl+X").c_str());
	menu.AppendMenu(item(selected > 0), kCopyCommand, label("Copy", L"Ctrl+C").c_str());
	menu.AppendMenu(item(canPaste), kPasteCommand, label("Paste", L"Ctrl+V").c_str());

	const contextmenu_manager::ptr shared = contextmenu_manager::g_create();
	shared->init_context_playlist(0);
	contextmenu_node* const root = shared->get_root();
	if (root && root->get_num_children() > 0) {
		menu.AppendMenu(MF_SEPARATOR);
		shared->win32_build_menu(menu, kFirstSharedCommand, -1);
	}
	menu_helpers::win32_auto_mnemonics(menu);
	// win32_auto_mnemonics rewrites a submenu's item without its state, so Sort is greyed after it.
	if (!canSort) menu.EnableMenuItem(sortPosition, MF_BYPOSITION | MF_GRAYED);

	const UINT command = static_cast<UINT>(menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, owner));
	if (command >= kFirstSharedCommand) {
		shared->execute_by_id(command - kFirstSharedCommand);
		return TrackMenuCommand::None;
	}
	if (command >= kFirstFixedSortCommand && command < kFirstFixedSortCommand + std::size(kFixedSorts)) {
		SortSelection(kFixedSorts[command - kFirstFixedSortCommand].pattern);
		return TrackMenuCommand::None;
	}
	switch (command) {
	case kPlayCommand: return TrackMenuCommand::Play;
	case kRemoveCommand: RemoveSelection(false); break;
	case kCropCommand: RemoveSelection(true); break;
	case kSortByCommand: AskAndSortSelection(owner); break;
	case kRandomizeCommand: SortSelection(nullptr); break;
	case kReverseCommand: ReverseSelection(); break;
	case kCutCommand: return TrackMenuCommand::Cut;
	case kCopyCommand: return TrackMenuCommand::Copy;
	case kPasteCommand: return TrackMenuCommand::Paste;
	}
	return TrackMenuCommand::None;
}
