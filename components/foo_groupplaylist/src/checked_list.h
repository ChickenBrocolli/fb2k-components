#pragma once

#include <string>
#include <utility>
#include <vector>

// The list at the top of the Columns and Groups preferences pages. One row per item, with a check box
// that says whether the item is listed in the panel header's menu.
//
// Attach after the dark mode hooks are added: they replace the dialog's list view with their own list,
// which takes the same messages.
class CheckedList {
public:
	// One column per pair of title and share of the list's width in percent.
	void Attach(HWND list, const std::vector<std::pair<const wchar_t*, int>>& columns) {
		m_list = list;
		m_list.SetExtendedListViewStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
		CRect client;
		m_list.GetClientRect(&client);
		const int width = client.Width() - GetSystemMetrics(SM_CXVSCROLL);
		int index = 0;
		for (const auto& column : columns) m_list.InsertColumn(index++, column.first, LVCFMT_LEFT, width * column.second / 100);
	}

	// Each row is its column texts. Notifications sent while filling are not reported to the page.
	void Fill(const std::vector<std::vector<std::string>>& rows, const std::vector<bool>& checks, int select) {
		m_filling = true;
		m_list.SetRedraw(FALSE);
		m_list.DeleteAllItems();
		for (size_t row = 0; row < rows.size(); ++row) {
			const int index = static_cast<int>(row);
			m_list.InsertItem(index, Wide(rows[row].empty() ? std::string() : rows[row][0]).c_str());
			for (size_t column = 1; column < rows[row].size(); ++column) {
				m_list.SetItemText(index, static_cast<int>(column), Wide(rows[row][column]).c_str());
			}
			m_list.SetCheckState(index, checks[row] ? TRUE : FALSE);
		}
		if (!rows.empty()) Select((std::max)(0, (std::min)(select, static_cast<int>(rows.size()) - 1)));
		m_list.SetRedraw(TRUE);
		m_list.Invalidate();
		m_filling = false;
	}

	void SetText(int row, int column, const std::string& text) {
		m_list.SetItemText(row, column, Wide(text).c_str());
	}

	// -1 when no row is selected.
	int Selected() const {
		return m_list.GetNextItem(-1, LVNI_SELECTED);
	}

	void Select(int row) {
		m_list.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.EnsureVisible(row, FALSE);
	}

	bool IsChecked(int row) const {
		return m_list.GetCheckState(row) != FALSE;
	}

	enum class Change { None, Selection, Check };

	// What an LVN_ITEMCHANGED notification from the list means for the page.
	Change Classify(LPNMHDR header) const {
		if (m_filling) return Change::None;
		const NMLISTVIEW* change = reinterpret_cast<const NMLISTVIEW*>(header);
		if ((change->uChanged & LVIF_STATE) == 0) return Change::None;
		const UINT flipped = change->uNewState ^ change->uOldState;
		if ((flipped & LVIS_STATEIMAGEMASK) != 0) return Change::Check;
		if ((flipped & LVIS_SELECTED) != 0) return Change::Selection;
		return Change::None;
	}

	static std::wstring Wide(const std::string& text) {
		return std::wstring(pfc::stringcvt::string_os_from_utf8(text.c_str()).get_ptr());
	}

private:
	CListViewCtrl m_list;
	bool m_filling = false;
};

// Moves the item at index one place up (step -1) or down (step 1). Returns where it went,
// or index itself when it is already at that end.
template <typename T>
int MoveItem(std::vector<T>& items, int index, int step) {
	const int target = index + step;
	if (index < 0 || target < 0 || target >= static_cast<int>(items.size())) return index;
	std::swap(items[static_cast<size_t>(index)], items[static_cast<size_t>(target)]);
	return target;
}
