#pragma once

#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>

// The rules of the rating column. A cell holds an empty gap and then five star slots of equal width.
// The pointer shows the rating it points at, and a click saves that rating: a star sets 1 to 5,
// the gap sets 0, which removes the rating.
// Standard library only, so the unit tests build without the foobar2000 SDK.

constexpr int kMaxRating = 5;

// Reads the value a rating pattern produced. Text that is not a number means no rating.
// Other programs store ratings on larger scales, so a value above five shows as five stars.
inline int ParseRating(const std::string& text) {
	const int value = std::atoi(text.c_str());
	return value < 0 ? 0 : value > kMaxRating ? kMaxRating : value;
}

// The width of the gap before the first star. Half a slot keeps it wide enough to point at,
// and it grows with the font as the slots do.
inline int RatingGapWidth(int slotWidth, int minimumGap) {
	return slotWidth / 2 > minimumGap ? slotWidth / 2 : minimumGap;
}

// The rating a point stands for: 0 in the gap, 1 to kMaxRating on a star, nothing past the last star.
// offset is the distance from the left edge of the cell.
inline std::optional<int> RatingAtOffset(int offset, int gapWidth, int slotWidth) {
	if (slotWidth <= 0 || offset < 0) return std::nullopt;
	if (offset < gapWidth) return 0;
	const int star = (offset - gapWidth) / slotWidth + 1;
	if (star > kMaxRating) return std::nullopt;
	return star;
}

// The rating under the pointer in one cell of the rating column.
struct StarPosition {
	size_t item;
	size_t subItem;
	int rating;
	bool operator==(const StarPosition&) const = default;
};

// The preview shown while the pointer is over a rating cell: that cell shows the rating the pointer
// stands for, whatever rating is saved. Moving away shows the saved rating again.
class StarHover {
public:
	// Pass the rating under the pointer, or nothing when the pointer is on no rating cell.
	// Returns true when a cell may now look different. Repaint the cells of Position() before and after the call.
	bool MoveTo(const std::optional<StarPosition>& at) {
		if (at == m_at) return false;
		m_at = at;
		return true;
	}

	const std::optional<StarPosition>& Position() const { return m_at; }

	int ShownRating(size_t item, size_t subItem, int savedRating) const {
		if (!m_at || m_at->item != item || m_at->subItem != subItem) return savedRating;
		return m_at->rating;
	}

private:
	std::optional<StarPosition> m_at;
};
