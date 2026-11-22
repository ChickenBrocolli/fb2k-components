#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Width math for "Auto-size columns": the track columns together fill the panel, keeping their proportions.
// Plain integers and floats, so the unit tests build without the foobar2000 SDK.

// One pixel width per column. Only the ratios of the weights count, so the saved widths serve as weights.
// The widths add up to exactly available. Each column ends where its share of the running total ends,
// so the rounding is spread over the columns instead of piling up on the last one.
// When no column has a weight, they share equally. No space gives every column 0.
inline std::vector<int> FitColumnWidths(const std::vector<float>& weights, int available) {
	std::vector<int> widths(weights.size(), 0);
	if (weights.empty() || available <= 0) return widths;
	double total = 0;
	for (const float weight : weights) total += (std::max)(0.f, weight);
	double end = 0;
	int placed = 0;
	for (size_t index = 0; index < weights.size(); ++index) {
		const double share = total > 0 ? (std::max)(0.f, weights[index]) / total : 1.0 / static_cast<double>(weights.size());
		end += share * available;
		const int edge = index + 1 == weights.size() ? available : static_cast<int>(std::lround(end));
		widths[index] = edge - placed;
		placed = edge;
	}
	return widths;
}

struct SharedWidths {
	int dragged;
	int neighbor;
};

// The user drags the right edge of one column while the columns fill the panel: the column to its right
// gives or takes the difference, so the two together keep their width. Neither goes below minimum,
// except a column already narrower than that, which keeps what it has.
inline SharedWidths ShareWithNeighbor(int dragged, int neighbor, int requested, int minimum) {
	const int total = dragged + neighbor;
	const int lowest = (std::min)(minimum, dragged);
	const int highest = (std::max)(total - (std::min)(minimum, neighbor), lowest);
	const int width = std::clamp(requested, lowest, highest);
	return { width, total - width };
}

// How a pushed edge behaves (PushDivider).
struct PushFeel {
	// The narrowest a column gets.
	int minimum = 0;
	// When a column has given all it may, the edge stays put while the pointer goes this much further, then catches up
	// with the pointer as the next column starts to give: a small catch the hand feels.
	int catchPixels = 0;
	// The same when no column has any room left (the wall), held longer.
	int wallPixels = 0;
	// Past the wall the text columns give their content too, cut short with "…", the nearest first, down to minimum.
	// Columns of fixed content (stars, a heart) never do: a part of them would look broken.
	bool beyondWall = false;
};

// The user drags the edge right of column divider while the columns fill the panel, as books pushed along a shelf:
// the column on the side the edge moves away from takes the space, and the columns on the other side give it,
// the nearest first. Each gives only down to the width its content needs (fits), so nothing gets hidden; then the next
// one gives. The column touching the edge is the one the user grabbed: when its content is already cut, it gives down
// to the minimum. When nothing more can give, the edge stops at the wall, or goes on past it (PushFeel). The widths
// always add up to the same total.
// widths are those at the start of the drag and delta is how far the edge has moved since, so dragging back gives
// every column its width back, the last one squeezed first, and passes the same catches.
inline std::vector<int> PushDivider(const std::vector<int>& widths, const std::vector<int>& fits, const std::vector<bool>& fixedContent,
	size_t divider, int delta, const PushFeel& feel) {
	std::vector<int> result = widths;
	if (divider + 1 >= widths.size() || fits.size() != widths.size() || fixedContent.size() != widths.size() || delta == 0) return result;
	// The columns that give, the nearest first, and how much each can give before the wall and past it.
	std::vector<size_t> order;
	if (delta > 0) {
		for (size_t column = divider + 1; column < widths.size(); ++column) order.push_back(column);
	} else {
		for (size_t column = divider + 1; column-- > 0;) order.push_back(column);
	}
	std::vector<int> before(order.size());
	std::vector<int> past(order.size());
	for (size_t index = 0; index < order.size(); ++index) {
		const size_t column = order[index];
		const int width = widths[column];
		const bool touching = index == 0;
		const int floor = !touching ? (std::min)(width, fits[column]) : width >= fits[column] ? fits[column] : (std::min)(feel.minimum, width);
		before[index] = (std::max)(0, width - floor);
		if (feel.beyondWall && !fixedContent[column]) past[index] = (std::max)(0, floor - (std::min)(feel.minimum, floor));
	}
	int roomBefore = 0;
	int roomPast = 0;
	for (size_t index = 0; index < order.size(); ++index) {
		roomBefore += before[index];
		roomPast += past[index];
	}

	// How far the edge itself goes for the pointer's distance: held in each catch, at the wall, and at the end.
	const int distance = delta > 0 ? delta : -delta;
	const auto held = [&](const std::vector<int>& gives, int start, int room, int travelled) {
		// Inside the catch after a column that gave all it may, except after the last one.
		int boundary = start;
		int stop = -1;
		for (size_t index = 0; index < gives.size(); ++index) {
			if (gives[index] <= 0) continue;
			boundary += gives[index];
			if (boundary >= start + room) break;
			if (travelled >= boundary && travelled < boundary + feel.catchPixels) stop = boundary;
		}
		return stop;
	};
	int moved = 0;
	if (distance <= roomBefore) {
		const int stop = held(before, 0, roomBefore, distance);
		moved = stop >= 0 ? stop : distance;
	} else if (!feel.beyondWall || roomPast == 0 || distance < roomBefore + feel.wallPixels) {
		moved = roomBefore;
	} else {
		const int stop = held(past, roomBefore, roomPast, distance);
		moved = stop >= 0 ? stop : (std::min)(distance, roomBefore + roomPast);
	}

	int left = moved;
	for (size_t index = 0; index < order.size() && left > 0; ++index) {
		const int give = (std::min)(before[index], left);
		result[order[index]] -= give;
		left -= give;
	}
	for (size_t index = 0; index < order.size() && left > 0; ++index) {
		const int give = (std::min)(past[index], left);
		result[order[index]] -= give;
		left -= give;
	}
	result[delta > 0 ? divider : divider + 1] += moved - left;
	return result;
}

// Without catches and with nothing given past the wall.
inline std::vector<int> PushDivider(const std::vector<int>& widths, const std::vector<int>& fits, size_t divider, int delta, int minimum) {
	PushFeel feel;
	feel.minimum = minimum;
	return PushDivider(widths, fits, std::vector<bool>(widths.size(), false), divider, delta, feel);
}
