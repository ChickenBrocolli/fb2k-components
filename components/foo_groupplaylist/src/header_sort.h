#pragma once

#include <string>

// What the panel remembers of the last sort by a click on a track column header.
// A second click on the same column reverses the order, and picking a group preset afterwards
// puts the playlist back in the preset's order. Both hold only for the playlist that was sorted,
// and only until something else changes its order, such as a drag, another sort, added or removed tracks or Ctrl+Z.
// The playlist id is a template parameter so the unit tests build without the foobar2000 SDK.
template <class PlaylistId>
class HeaderSortMemory {
public:
	// A click on the header of column in the active playlist. Returns whether to sort in reverse.
	bool Click(const std::string& column, const PlaylistId& active) {
		m_descending = SortedByColumn(active) && column == m_column && !m_descending;
		m_column = column;
		m_playlist = active;
		return m_descending;
	}

	// Whether the active playlist is still in the order of a header click.
	bool SortedByColumn(const PlaylistId& active) const { return !m_column.empty() && m_playlist == active; }

	void Forget() {
		m_column.clear();
		m_descending = false;
	}

private:
	std::string m_column;
	bool m_descending = false;
	PlaylistId m_playlist{};
};
