#include "stdafx.h"

#include "info_lines_popup.h"

#include <helpers/DarkMode.h>
#include <libPPUI/CListControl-Cells.h>

#include "checked_list.h"
#include "info_lines.h"
#include "playlist_view.h"
#include "preferences_pages.h"
#include "preset_store.h"
#include "resource.h"
#include "ui_language.h"

namespace {

class InfoLinesPopup : public CDialogImpl<InfoLinesPopup> {
public:
	enum { IDD = IDD_INFO_LINES };

	InfoLinesPopup(PlaylistView& view, size_t block, std::string presetName, CPoint at)
		: m_view(&view), m_block(block), m_presetName(std::move(presetName)), m_at(at) {}

	static InfoLinesPopup* s_open;

	const PlaylistView* View() const { return m_view; }

	void Close() {
		if (m_hWnd) DestroyWindow();
	}

	BEGIN_MSG_MAP_EX(InfoLinesPopup)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_ACTIVATE(OnActivate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_MOUSEMOVE(OnMouseMove)
		MSG_WM_LBUTTONUP(OnLButtonUp)
		MSG_WM_CAPTURECHANGED(OnCaptureChanged)
		NOTIFY_HANDLER_EX(IDC_INFO_LIST, LVN_ITEMCHANGED, OnListChanged)
		COMMAND_ID_HANDLER_EX(IDC_INFO_DEFAULT, OnDefault)
		COMMAND_ID_HANDLER_EX(IDC_INFO_EDIT, OnEditPattern)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()

private:
	void OnFinalMessage(HWND) override {
		delete this;
	}

	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(*this);
		m_texts.Translate(*this);
		m_list.Attach(GetDlgItem(IDC_INFO_LIST), { { L"", 50 }, { L"", 50 } });
		// The dark mode hooks put foobar2000's own list in place of the list view, and it tells nothing of a drag.
		// So the list's mouse is watched here (ListProc).
		SetWindowSubclass(GetDlgItem(IDC_INFO_LIST), ListProc, 0, reinterpret_cast<DWORD_PTR>(this));
		Fill();
		Place();
		modeless_dialog_manager::g_add(m_hWnd);
		GetDlgItem(IDC_INFO_LIST).SetFocus();
		return FALSE;
	}

	// The rows for the preset as saved now: its lines shown first, then the others. What each line gives for the cover
	// is read once here, and moves with its row.
	void Fill() {
		const std::vector<GroupPreset> presets = LoadGroupPresets();
		const std::optional<size_t> index = FindPresetByName(presets, m_presetName);
		m_choices.clear();
		if (index) {
			const GroupPreset& preset = presets[*index];
			m_choices = InfoChoices(preset.coverPattern, preset.showCoverText, PickedInfoLines(LoadInfoLineChoices(), preset.name));
		}
		m_values.clear();
		for (const InfoChoice& choice : m_choices) m_values.push_back(m_view->InfoValue(m_block, choice.entry));
		Show(0);
	}

	void Show(int select) {
		std::vector<std::vector<std::string>> rows;
		std::vector<bool> checks;
		for (size_t index = 0; index < m_choices.size(); ++index) {
			const InfoItem* item = InfoItemOfEntry(m_choices[index].entry);
			rows.push_back({ item ? Tr(item->name) : m_choices[index].entry, m_values[index] });
			checks.push_back(m_choices[index].shown);
		}
		m_list.Fill(rows, checks, select);
	}

	// At the pointer, kept on the screen it is on.
	void Place() {
		CRect window;
		GetWindowRect(&window);
		MONITORINFO monitor = { sizeof(monitor) };
		GetMonitorInfo(MonitorFromPoint(m_at, MONITOR_DEFAULTTONEAREST), &monitor);
		const CRect work(monitor.rcWork);
		int left = static_cast<int>(m_at.x);
		int top = static_cast<int>(m_at.y);
		if (left + window.Width() > work.right) left = (std::max)(static_cast<int>(work.left), static_cast<int>(m_at.x) - window.Width());
		if (top + window.Height() > work.bottom) top = (std::max)(static_cast<int>(work.top), static_cast<int>(work.bottom) - window.Height());
		SetWindowPos(HWND_TOP, left, top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
	}

	// Closing inside the activation message would pull the window away while Windows still switches to the other one.
	void OnActivate(UINT state, BOOL, CWindow) {
		if (state == WA_INACTIVE) PostMessage(WM_COMMAND, IDCANCEL);
	}

	void OnDestroy() {
		RemoveWindowSubclass(GetDlgItem(IDC_INFO_LIST), ListProc, 0);
		modeless_dialog_manager::g_remove(m_hWnd);
		if (s_open == this) s_open = nullptr;
		SetMsgHandled(FALSE);
	}

	// The last ticked row keeps its tick (MayUntick): the list tells of a change only after it, so the tick goes back.
	LRESULT OnListChanged(LPNMHDR header) {
		if (m_reticking || m_list.Classify(header) != CheckedList::Change::Check) return 0;
		const int index = reinterpret_cast<const NMLISTVIEW*>(header)->iItem;
		if (index < 0 || static_cast<size_t>(index) >= m_choices.size()) return 0;
		const bool checked = m_list.IsChecked(index);
		if (!checked && !MayUntick(m_choices, static_cast<size_t>(index))) {
			pfc::vartoggle_t<bool> reticking(m_reticking, true);
			CListViewCtrl(GetDlgItem(IDC_INFO_LIST)).SetCheckState(index, TRUE);
			GetDlgItem(IDC_INFO_LIST).Invalidate();
			return 0;
		}
		m_choices[static_cast<size_t>(index)].shown = checked;
		// The greyed box moves to the row left alone, or goes when a second row is ticked.
		GetDlgItem(IDC_INFO_LIST).Invalidate();
		PlaylistView::SetPresetInfoLines(m_presetName, ShownEntries(m_choices));
		return 0;
	}

	// A row pressed and moved up or down past the system's drag distance is dragged: it follows the pointer, and the lines
	// beside the covers follow its new place at once. The popup holds the mouse until the button comes up.
	static LRESULT CALLBACK ListProc(HWND list, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR popupPointer) {
		InfoLinesPopup* const popup = reinterpret_cast<InfoLinesPopup*>(popupPointer);
		const CPoint point(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
		if (message == WM_PAINT) {
			const LRESULT result = DefSubclassProc(list, message, wParam, lParam);
			popup->PaintLockedTick(CListViewCtrl(list));
			return result;
		}
		if (message == WM_LBUTTONDOWN) {
			popup->m_pressPoint = point;
			popup->m_pressRow = popup->RowAt(CListViewCtrl(list), point.y);
		} else if (message == WM_LBUTTONUP) {
			popup->m_pressRow = -1;
		} else if (message == WM_MOUSEMOVE && (wParam & MK_LBUTTON) && popup->m_pressRow >= 0 && popup->m_dragRow < 0
			&& std::abs(point.y - popup->m_pressPoint.y) > GetSystemMetrics(SM_CYDRAG)) {
			popup->m_dragRow = popup->m_pressRow;
			popup->m_pressRow = -1;
			popup->SetCapture();
			return 0;
		}
		return DefSubclassProc(list, message, wParam, lParam);
	}

	void OnMouseMove(UINT, CPoint point) {
		if (m_dragRow < 0) return;
		CListViewCtrl list(GetDlgItem(IDC_INFO_LIST));
		MapWindowPoints(list, &point, 1);
		const int target = RowAt(list, point.y);
		if (target < 0 || target == m_dragRow) return;
		const std::vector<std::string> before = ShownEntries(m_choices);
		MoveRow(m_choices, static_cast<size_t>(m_dragRow), static_cast<size_t>(target));
		MoveRow(m_values, static_cast<size_t>(m_dragRow), static_cast<size_t>(target));
		m_dragRow = target;
		Show(target);
		if (ShownEntries(m_choices) != before) PlaylistView::SetPresetInfoLines(m_presetName, ShownEntries(m_choices));
	}

	// The only ticked row, which cannot be unticked (MayUntick), or -1.
	int LockedRow() const {
		int locked = -1;
		for (size_t index = 0; index < m_choices.size(); ++index) {
			if (!m_choices[index].shown) continue;
			if (locked >= 0) return -1;
			locked = static_cast<int>(index);
		}
		return locked;
	}

	// The list cannot grey one check box, so the greyed ticked box is drawn over the locked row's box after the list has
	// painted, the same way the list draws its boxes (a square at the row's left), in the list's own theme, light or dark.
	void PaintLockedTick(CListViewCtrl list) const {
		const int row = LockedRow();
		CRect rowRect;
		if (row < 0 || !list.GetItemRect(row, &rowRect, LVIR_BOUNDS)) return;
		CClientDC dc(list);
		const HTHEME theme = OpenThemeData(list, L"BUTTON");
		RenderCheckbox(theme, CDCHandle(dc), CRect(rowRect.left, rowRect.top, rowRect.left + rowRect.Height(), rowRect.bottom),
			CListCell::cellState_pressed | CListCell::cellState_disabled, false);
		if (theme) CloseThemeData(theme);
	}

	// The row under y in the list's coordinates; above the first row the first, below the last the last.
	int RowAt(CListViewCtrl list, int y) const {
		const int count = list.GetItemCount();
		CRect first;
		if (count == 0 || !list.GetItemRect(0, &first, LVIR_BOUNDS) || first.Height() <= 0) return -1;
		const int row = y < first.top ? 0 : (y - first.top) / first.Height();
		return std::clamp(row, 0, count - 1);
	}

	void OnLButtonUp(UINT, CPoint) {
		if (m_dragRow >= 0) ReleaseCapture();
	}

	void OnCaptureChanged(CWindow) {
		m_dragRow = -1;
	}

	// Back to the lines of the preset's cover pattern.
	void OnDefault(UINT, int, CWindow) {
		PlaylistView::SetPresetInfoLines(m_presetName, std::nullopt);
		Fill();
	}

	void OnEditPattern(UINT, int, CWindow) {
		const std::string presetName = m_presetName;
		Close();
		ShowGroupsPage(presetName);
	}

	void OnCancel(UINT, int, CWindow) {
		Close();
	}

	PlaylistView* m_view;
	size_t m_block;
	std::string m_presetName;
	CPoint m_at;
	std::vector<InfoChoice> m_choices;
	std::vector<std::string> m_values;
	// The row pressed, until a drag starts or the button comes up, and where; the row being dragged. -1 for none.
	int m_pressRow = -1;
	CPoint m_pressPoint;
	int m_dragRow = -1;
	// True while the popup itself puts back a tick, so the change is not read as the user's.
	bool m_reticking = false;
	CheckedList m_list;
	fb2k::CDarkModeHooks m_dark;
	DialogTexts m_texts;
};

InfoLinesPopup* InfoLinesPopup::s_open = nullptr;

} // namespace

void ShowInfoLinesPopup(PlaylistView& view, size_t block, CPoint screenPoint) {
	if (InfoLinesPopup::s_open) InfoLinesPopup::s_open->Close();
	if (view.IsUngrouped() || view.ActivePreset() >= view.Presets().size()) return;
	auto* popup = new InfoLinesPopup(view, block, view.Presets()[view.ActivePreset()].name, screenPoint);
	if (!popup->Create(view.m_hWnd)) {
		delete popup;
		return;
	}
	InfoLinesPopup::s_open = popup;
	popup->ShowWindow(SW_SHOW);
}

void CloseInfoLinesPopupOf(const PlaylistView* view) {
	if (InfoLinesPopup::s_open && InfoLinesPopup::s_open->View() == view) InfoLinesPopup::s_open->Close();
}
