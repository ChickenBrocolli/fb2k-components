#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Fits dialog labels to their translated text. The dialogs are laid out for the English texts, so a shorter
// translation leaves a gap between a label (or a check box) and the control after it on the same row.
// Each label is narrowed to its text, and the controls that follow it closely on its row move left by the same
// amount, keeping their spacing. A control further away stays where it is, so a row with two groups keeps its
// right group in line with the rows below. A text longer than the English one leaves the row as it was.
// Standard library only, so the unit tests build without the foobar2000 SDK.

struct FitBox {
	int left, top, right, bottom;
	int Width() const { return right - left; }
	int Height() const { return bottom - top; }
};

struct FitLabel {
	size_t box;
	int neededWidth;  // the width the translated text needs, check box square included
};

struct FitResult {
	std::vector<int> leftShift;  // per box: how far it moves left
	std::vector<int> width;      // per box: its new width
};

namespace label_fit_detail {

// Two boxes are on one row when they overlap by at least half the height of the lower one.
inline bool SameRow(const FitBox& a, const FitBox& b) {
	const int overlap = (std::min)(a.bottom, b.bottom) - (std::max)(a.top, b.top);
	return overlap * 2 >= (std::min)(a.Height(), b.Height());
}

} // namespace label_fit_detail

// The boxes that follow box `from` on its row, nearest first: each starts at most maxGap after the end of the one
// before it. A box starting a little before that end still counts: dialog units round, and a spin box's arrows
// overlap the edge of their number box. A box much taller than the label (a list beside it) spans several rows and
// does not follow.
inline std::vector<size_t> RowFollowers(const std::vector<FitBox>& boxes, size_t from, int maxGap) {
	std::vector<size_t> chain;
	size_t current = from;
	const int tallest = boxes[from].Height() * 5 / 2;
	for (;;) {
		size_t next = boxes.size();
		for (size_t i = 0; i < boxes.size(); ++i) {
			if (i == from || std::find(chain.begin(), chain.end(), i) != chain.end()) continue;
			if (boxes[i].Height() > tallest) continue;
			const int gap = boxes[i].left - boxes[current].right;
			if (gap < -maxGap / 2 || gap > maxGap || boxes[i].left <= boxes[current].left) continue;
			if (!label_fit_detail::SameRow(boxes[current], boxes[i])) continue;
			if (next == boxes.size() || boxes[i].left < boxes[next].left) next = i;
		}
		if (next == boxes.size()) return chain;
		chain.push_back(next);
		current = next;
	}
}

// The labels are narrowed and their followers moved. A follower that is itself a label moves by the shrink of every
// label before it in its chain, and passes its own shrink on to the boxes after it.
// Labels stacked in a column, starting at one place with their first followers lined up at another (two font
// choices, each with its Choose button), shrink by the same amount, the least of them, so the followers stay in line.
inline FitResult FitLabels(const std::vector<FitBox>& boxes, std::vector<FitLabel> labels, int maxGap) {
	FitResult result;
	result.leftShift.assign(boxes.size(), 0);
	result.width.resize(boxes.size());
	for (size_t i = 0; i < boxes.size(); ++i) result.width[i] = boxes[i].Width();
	std::sort(labels.begin(), labels.end(), [&](const FitLabel& a, const FitLabel& b) { return boxes[a.box].left < boxes[b.box].left; });

	std::vector<std::vector<size_t>> followers(labels.size());
	std::vector<int> shrink(labels.size());
	for (size_t l = 0; l < labels.size(); ++l) {
		followers[l] = RowFollowers(boxes, labels[l].box, maxGap);
		shrink[l] = (std::max)(0, boxes[labels[l].box].Width() - labels[l].neededWidth);
	}
	const auto sameColumn = [&](size_t a, size_t b) {
		if (followers[a].empty() || followers[b].empty()) return false;
		const auto withinAPixel = [](int x, int y) { return x - y <= 1 && y - x <= 1; };
		return withinAPixel(boxes[labels[a].box].left, boxes[labels[b].box].left) &&
			withinAPixel(boxes[followers[a].front()].left, boxes[followers[b].front()].left);
	};
	std::vector<int> columnShrink = shrink;
	for (size_t a = 0; a < labels.size(); ++a) {
		for (size_t b = 0; b < labels.size(); ++b) {
			if (a != b && sameColumn(a, b)) columnShrink[a] = (std::min)(columnShrink[a], shrink[b]);
		}
	}

	for (size_t l = 0; l < labels.size(); ++l) {
		// A label with nothing after it on its row has no gap to close.
		if (followers[l].empty() || columnShrink[l] <= 0) continue;
		result.width[labels[l].box] = boxes[labels[l].box].Width() - columnShrink[l];
		for (const size_t follower : followers[l]) result.leftShift[follower] += columnShrink[l];
	}
	return result;
}
