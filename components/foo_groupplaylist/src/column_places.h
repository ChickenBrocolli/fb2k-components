#pragma once

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

// Where the cover and info columns sit among the track columns. The user drags them anywhere, as the album art
// column of the Default UI playlist. A place is a position in the whole header, counted from the left.
// Standard library only, so the unit tests build without the foobar2000 SDK.

// The list columns as the view adds them: the cover, the info text, then the track columns in their saved order.
constexpr size_t kCoverColumn = 0;
constexpr size_t kInfoColumn = 1;
constexpr size_t kFirstTrackColumn = 2;

// The same numbers as a header's column order array holds them.
constexpr int kCoverOrderColumn = static_cast<int>(kCoverColumn);
constexpr int kInfoOrderColumn = static_cast<int>(kInfoColumn);
constexpr int kFirstTrackOrderColumn = static_cast<int>(kFirstTrackColumn);
constexpr bool IsTrackColumn(int column) { return column >= kFirstTrackOrderColumn; }

struct BlockColumnPlaces {
	size_t cover = kCoverColumn;
	size_t info = kInfoColumn;

	bool operator==(const BlockColumnPlaces&) const = default;
};

// Places inside the header and apart, as a settings file edited by hand may not have them.
// A place past the end goes to the end. Two equal places keep the cover first.
inline BlockColumnPlaces NormalizePlaces(size_t trackCount, BlockColumnPlaces places) {
	const size_t last = trackCount + 1;
	places.cover = (std::min)(places.cover, last);
	places.info = (std::min)(places.info, last);
	if (places.cover == places.info) {
		if (places.cover == last) places.cover = last - 1;
		else places.info = places.cover + 1;
	}
	return places;
}

// The list columns in screen order: the track columns in their order, with the cover and info columns at their places.
inline std::vector<int> ScreenOrder(size_t trackCount, BlockColumnPlaces places) {
	places = NormalizePlaces(trackCount, places);
	std::vector<int> order;
	for (size_t index = 0; index < trackCount; ++index) order.push_back(static_cast<int>(kFirstTrackColumn + index));
	// The one further left goes in first, so the other's place counts it.
	std::pair<size_t, int> first(places.cover, kCoverOrderColumn), second(places.info, kInfoOrderColumn);
	if (second.first < first.first) std::swap(first, second);
	order.insert(order.begin() + static_cast<std::ptrdiff_t>(first.first), first.second);
	order.insert(order.begin() + static_cast<std::ptrdiff_t>(second.first), second.second);
	return order;
}

inline BlockColumnPlaces PlacesFromOrder(const std::vector<int>& order) {
	BlockColumnPlaces places;
	for (size_t position = 0; position < order.size(); ++position) {
		if (order[position] == kCoverOrderColumn) places.cover = position;
		else if (order[position] == kInfoOrderColumn) places.info = position;
	}
	return places;
}

// The places after the track columns change: kept says which of the old track columns stay, and added new ones
// are put right after the last track column on screen. So a cover at the right end stays at the right end.
inline BlockColumnPlaces PlacesAfterTrackChange(BlockColumnPlaces places, const std::vector<bool>& kept, size_t added) {
	std::vector<int> order;
	for (const int column : ScreenOrder(kept.size(), places)) {
		if (!IsTrackColumn(column) || kept[static_cast<size_t>(column - kFirstTrackOrderColumn)]) order.push_back(column);
	}
	size_t afterLastTrack = order.size();
	for (size_t position = order.size(); position > 0; --position) {
		if (IsTrackColumn(order[position - 1])) {
			afterLastTrack = position;
			break;
		}
	}
	order.insert(order.begin() + static_cast<std::ptrdiff_t>(afterLastTrack), added, kFirstTrackOrderColumn);
	return PlacesFromOrder(order);
}

// The widest run of track columns side by side on screen, as the first and last screen positions of the run.
// The labels inside a group ("Disc 2") go over it, clear of the cover and info columns. The leftmost run wins a tie.
inline std::optional<std::pair<size_t, size_t>> WidestTrackRun(const std::vector<int>& order, const std::vector<int>& widths) {
	std::optional<std::pair<size_t, size_t>> widest;
	int widestWidth = -1;
	size_t position = 0;
	while (position < order.size()) {
		if (!IsTrackColumn(order[position])) {
			++position;
			continue;
		}
		const size_t first = position;
		int width = 0;
		while (position < order.size() && IsTrackColumn(order[position])) width += widths[position++];
		if (width > widestWidth) {
			widestWidth = width;
			widest = std::make_pair(first, position - 1);
		}
	}
	return widest;
}
