#include "check.h"
#include "rating_cell.h"

namespace {

void ARatingPatternValueReadsAsZeroToFiveStars() {
	CHECK(ParseRating("3") == 3);
	CHECK(ParseRating("5") == 5);
	CHECK(ParseRating("0") == 0);
	CHECK(ParseRating("") == 0);
	CHECK(ParseRating("?") == 0);
	CHECK(ParseRating("-2") == 0);
	CHECK(ParseRating("100") == 5);
}

void TheGapIsHalfASlotButNeverNarrowerThanTheMinimum() {
	CHECK(RatingGapWidth(14, 4) == 7);
	CHECK(RatingGapWidth(6, 4) == 4);
}

// A cell with a 7 pixel gap and 14 pixel slots: gap 0..6, star 1 at 7..20, star 5 at 63..76.
void TheGapBeforeTheFirstStarStandsForNoRating() {
	CHECK(RatingAtOffset(0, 7, 14) == 0);
	CHECK(RatingAtOffset(6, 7, 14) == 0);
}

void EachStarOwnsOneSlot() {
	CHECK(RatingAtOffset(7, 7, 14) == 1);
	CHECK(RatingAtOffset(20, 7, 14) == 1);
	CHECK(RatingAtOffset(21, 7, 14) == 2);
	CHECK(RatingAtOffset(76, 7, 14) == 5);
}

void APointPastTheLastStarOrOutsideTheCellStandsForNothing() {
	CHECK(!RatingAtOffset(77, 7, 14).has_value());
	CHECK(!RatingAtOffset(500, 7, 14).has_value());
	CHECK(!RatingAtOffset(-1, 7, 14).has_value());
	CHECK(!RatingAtOffset(10, 7, 0).has_value());
}

void ThePointedCellShowsThePointedRatingWhateverIsSaved() {
	StarHover hover;
	CHECK(hover.MoveTo(StarPosition{ 4, 7, 2 }));
	CHECK(hover.ShownRating(4, 7, 0) == 2);
	CHECK(hover.ShownRating(4, 7, 5) == 2);
	CHECK(hover.MoveTo(StarPosition{ 4, 7, 5 }));
	CHECK(hover.ShownRating(4, 7, 1) == 5);
}

void PointingAtTheGapShowsNoStarsEvenWhenARatingIsSaved() {
	StarHover hover;
	hover.MoveTo(StarPosition{ 4, 7, 0 });
	CHECK(hover.ShownRating(4, 7, 3) == 0);
}

void OtherCellsShowTheirSavedRating() {
	StarHover hover;
	hover.MoveTo(StarPosition{ 4, 7, 2 });
	CHECK(hover.ShownRating(5, 7, 3) == 3);
	CHECK(hover.ShownRating(4, 8, 3) == 3);
}

void MovingAwayShowsTheSavedRatingAgain() {
	StarHover hover;
	hover.MoveTo(StarPosition{ 4, 7, 2 });
	CHECK(hover.MoveTo(std::nullopt));
	CHECK(hover.ShownRating(4, 7, 3) == 3);
	CHECK(!hover.Position().has_value());
}

void StayingOnTheSameRatingNeedsNoRepaint() {
	StarHover hover;
	CHECK(!hover.MoveTo(std::nullopt));
	hover.MoveTo(StarPosition{ 4, 7, 2 });
	CHECK(!hover.MoveTo(StarPosition{ 4, 7, 2 }));
}

} // namespace

void RunRatingCellTests() {
	ARatingPatternValueReadsAsZeroToFiveStars();
	TheGapIsHalfASlotButNeverNarrowerThanTheMinimum();
	TheGapBeforeTheFirstStarStandsForNoRating();
	EachStarOwnsOneSlot();
	APointPastTheLastStarOrOutsideTheCellStandsForNothing();
	ThePointedCellShowsThePointedRatingWhateverIsSaved();
	PointingAtTheGapShowsNoStarsEvenWhenARatingIsSaved();
	OtherCellsShowTheirSavedRating();
	MovingAwayShowsTheSavedRatingAgain();
	StayingOnTheSameRatingNeedsNoRepaint();
}
