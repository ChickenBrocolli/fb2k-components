#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// How a dragged column meets the column next to it (ColumnDrag).
struct ColumnDragFeel {
	// The share of the neighbour the dragged column must cover before the two swap. 0.5 swaps at its middle;
	// more makes the neighbour hold out longer.
	double swapDepth = 0.5;
	// Pixels past that point as well. A swap back needs the same past the other side, so a hand resting on the line
	// does not swap them to and fro. At most a quarter of the neighbour is used.
	int slack = 0;
	// Until the swap, the neighbour gives way by this share of how far the dragged column covers it, at most resistMax
	// pixels: it holds out a little before it gives in.
	double resistShare = 0;
	int resistMax = 0;
};

// A column title pressed and moved past the system's drag distance: whether its column starts to move now. A click
// often moves the mouse a few pixels on the way; a title held for holdMs, or dragged more than escape pixels at once,
// is meant to move. Until then the press stays a click, and lets go as one (a sort).
inline bool ColumnDragStarts(unsigned heldMs, int distance, unsigned holdMs, int escape) {
	return heldMs >= holdMs || distance > escape;
}

// A column title dragged along the header: where the dragged column is drawn, and the order the columns take on the way.
// The dragged column follows the pointer, kept inside the header. When it covers enough of the column next to it
// (ColumnDragFeel), the two swap places, so the order on screen is always the order a drop would give.
// A column with no width (the hidden info column) has nothing to grab and rides along with the column before it,
// so it stays right after the cover.
// Standard library only, so the unit tests build without the foobar2000 SDK.
class ColumnDrag {
public:
	// order: the columns from left to right, as the header's order array. widths: each column's width, indexed by column.
	// grabX: where the pointer pressed, in the same coordinates as the columns (0 is the left edge of the first column).
	ColumnDrag(std::vector<int> order, std::vector<int> widths, int dragged, int grabX, ColumnDragFeel feel)
		: m_start(order), m_order(std::move(order)), m_widths(std::move(widths)), m_dragged(dragged), m_feel(feel) {
		m_grabOffset = grabX - LeftOf(dragged);
		m_draggedLeft = LeftOf(dragged);
	}

	// Moves the dragged column with the pointer. Returns whether the order changed.
	bool MoveTo(int pointerX) {
		const int width = UnitWidth(m_dragged);
		m_draggedLeft = std::clamp(pointerX - m_grabOffset, 0, (std::max)(0, TotalWidth() - width));
		const int right = m_draggedLeft + width;
		bool changed = false;
		for (size_t guard = 0; guard < 2 * m_order.size(); ++guard) {
			const std::vector<Unit> units = Units();
			const size_t at = UnitIndexOf(units, m_dragged);
			// Moving right, the right edge goes into the next column from its left; moving left, the left edge goes into
			// the previous column from its right.
			if (at + 1 < units.size() && right > units[at + 1].left + Depth(units[at + 1]) + Slack(units[at + 1])) {
				Swap(units[at], units[at + 1]);
				m_passedSide = -1;
			} else if (at > 0 && m_draggedLeft < units[at - 1].left + units[at - 1].width - Depth(units[at - 1]) - Slack(units[at - 1])) {
				Swap(units[at - 1], units[at]);
				m_passedSide = 1;
			} else {
				break;
			}
			changed = true;
		}
		// Once the dragged column reaches its new place, the column it passed may hold out again.
		const int fromPlace = m_draggedLeft - LeftOf(m_dragged);
		if ((m_passedSide < 0 && fromPlace >= 0) || (m_passedSide > 0 && fromPlace <= 0)) m_passedSide = 0;
		return changed;
	}

	// How far the column gives way to the dragged one before they swap: right of its place for the next column,
	// left for the previous one, nothing for the others. The column just passed does not: the dragged column still
	// covers it on its way to its new place, and pushing it on would push it into the column beyond.
	int Resistance(int column) const {
		if (m_feel.resistShare <= 0 || m_feel.resistMax <= 0) return 0;
		const std::vector<Unit> units = Units();
		const size_t at = UnitIndexOf(units, m_dragged);
		const auto giveWay = [&](int covered) {
			return covered <= 0 ? 0 : (std::min)(static_cast<int>(std::lround(covered * m_feel.resistShare)), m_feel.resistMax);
		};
		if (at + 1 < units.size() && m_passedSide <= 0 && Contains(units[at + 1], column)) {
			return giveWay(m_draggedLeft + units[at].width - units[at + 1].left);
		}
		if (at > 0 && m_passedSide >= 0 && Contains(units[at - 1], column)) {
			return -giveWay(units[at - 1].left + units[at - 1].width - m_draggedLeft);
		}
		return 0;
	}

	// The order a drop gives.
	const std::vector<int>& Order() const { return m_order; }
	// The order before the drag, for a cancelled drag.
	const std::vector<int>& StartOrder() const { return m_start; }
	int Dragged() const { return m_dragged; }
	// Where the dragged column is drawn: under the pointer, kept inside the header.
	int DraggedLeft() const { return m_draggedLeft; }

	// The left edge of a column's place in the current order.
	int LeftOf(int column) const { return LeftIn(m_order, column); }
	int LeftIn(const std::vector<int>& order, int column) const {
		int x = 0;
		for (const int walk : order) {
			if (walk == column) return x;
			x += WidthOf(walk);
		}
		return x;
	}

private:
	// A column and the columns with no width after it, which move with it.
	struct Unit {
		size_t first;
		size_t count;
		int left;
		int width;
	};

	int WidthOf(int column) const {
		return column >= 0 && static_cast<size_t>(column) < m_widths.size() ? (std::max)(0, m_widths[column]) : 0;
	}

	int TotalWidth() const {
		int total = 0;
		for (const int column : m_order) total += WidthOf(column);
		return total;
	}

	// Columns with no width at the very start belong to the first column that has one.
	std::vector<Unit> Units() const {
		std::vector<Unit> units;
		int x = 0;
		for (size_t index = 0; index < m_order.size(); ++index) {
			const int width = WidthOf(m_order[index]);
			if (width > 0 || units.empty()) {
				units.push_back({ index, 1, x, width });
			} else {
				++units.back().count;
			}
			x += width;
		}
		// A unit of nothing but empty columns at the start joins the next one.
		if (units.size() > 1 && units.front().width == 0) {
			units[1].first = 0;
			units[1].count += units.front().count;
			units[1].left = 0;
			units.erase(units.begin());
		}
		return units;
	}

	bool Contains(const Unit& unit, int column) const {
		for (size_t walk = unit.first; walk < unit.first + unit.count; ++walk) {
			if (m_order[walk] == column) return true;
		}
		return false;
	}

	size_t UnitIndexOf(const std::vector<Unit>& units, int column) const {
		for (size_t index = 0; index < units.size(); ++index) {
			if (Contains(units[index], column)) return index;
		}
		return 0;
	}

	int UnitWidth(int column) const {
		const std::vector<Unit> units = Units();
		return units.empty() ? 0 : units[UnitIndexOf(units, column)].width;
	}

	int Depth(const Unit& unit) const { return static_cast<int>(std::lround(unit.width * m_feel.swapDepth)); }
	int Slack(const Unit& unit) const { return (std::min)(m_feel.slack, unit.width / 4); }

	// left comes right before right in the order.
	void Swap(const Unit& left, const Unit& right) {
		const auto begin = m_order.begin() + static_cast<std::ptrdiff_t>(left.first);
		std::rotate(begin, begin + static_cast<std::ptrdiff_t>(left.count), begin + static_cast<std::ptrdiff_t>(left.count + right.count));
	}

	std::vector<int> m_start;
	std::vector<int> m_order;
	std::vector<int> m_widths;
	int m_dragged;
	ColumnDragFeel m_feel;
	// The side of the column the dragged one passed last (-1 left, 1 right) until the dragged one reaches its place.
	int m_passedSide = 0;
	int m_grabOffset = 0;
	int m_draggedLeft = 0;
};

// Columns that changed places slide to their new places instead of jumping: each starts drawn where it was and its
// offset shrinks to nothing, quickly at first and slowing down at the end. With overshoot above 0 a column goes a
// little past its place and settles back (ease-out back; 0.9 goes about 3% past, 0 does not and is ease-out cubic).
class ColumnSlides {
public:
	ColumnSlides(size_t columns, double durationMs, double overshoot = 0)
		: m_slides(columns), m_duration(durationMs), m_overshoot(overshoot) {}

	// The column's place moved by moved pixels (new left minus old left). It goes on being drawn where it is
	// and slides from there, also when it was still sliding from an earlier move.
	void Moved(size_t column, int moved, double nowMs) {
		StartFrom(column, Offset(column, nowMs) - moved, nowMs);
	}

	// The column is drawn offset from its place by offset pixels now, and slides home from there.
	void StartFrom(size_t column, double offset, double nowMs) {
		if (column >= m_slides.size()) return;
		m_slides[column] = { offset, nowMs };
	}

	// How far from its place the column is drawn at nowMs.
	double Offset(size_t column, double nowMs) const {
		if (column >= m_slides.size() || m_duration <= 0) return 0;
		const Slide& slide = m_slides[column];
		const double progress = std::clamp((nowMs - slide.start) / m_duration, 0.0, 1.0);
		const double remaining = 1.0 - progress;
		return slide.from * ((m_overshoot + 1) * remaining * remaining * remaining - m_overshoot * remaining * remaining);
	}

	// Whether any column is still on its way at nowMs.
	bool Running(double nowMs) const {
		if (m_duration <= 0) return false;
		for (const Slide& slide : m_slides) {
			if (slide.from != 0 && nowMs - slide.start < m_duration) return true;
		}
		return false;
	}

private:
	struct Slide {
		double from = 0;
		double start = 0;
	};
	std::vector<Slide> m_slides;
	double m_duration;
	double m_overshoot;
};
