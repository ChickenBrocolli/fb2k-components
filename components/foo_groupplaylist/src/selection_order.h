#pragma once

#include <cstddef>
#include <vector>

// The order that reverses the selected tracks among the places they hold, for the Reverse item of the track menu.
// Tracks that are not selected stay where they are. Position i receives the track that was at order[i],
// as playlist_manager::reorder_items takes it.
inline std::vector<size_t> ReversedSelectionOrder(const std::vector<bool>& selected) {
	std::vector<size_t> order(selected.size());
	std::vector<size_t> places;
	for (size_t i = 0; i < selected.size(); ++i) {
		order[i] = i;
		if (selected[i]) places.push_back(i);
	}
	for (size_t j = 0; j < places.size(); ++j) order[places[j]] = places[places.size() - 1 - j];
	return order;
}
