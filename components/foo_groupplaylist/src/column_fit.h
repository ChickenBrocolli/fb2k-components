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
