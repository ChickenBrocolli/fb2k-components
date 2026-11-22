#include "check.h"

#include <cmath>
#include <vector>

#include "column_drag.h"

namespace {

using Order = std::vector<int>;

// A swap at the neighbour's middle, with no give before it.
ColumnDragFeel Half(int slack) {
	ColumnDragFeel feel;
	feel.slack = slack;
	return feel;
}

void AColumnSwapsWhenItCoversHalfItsNeighbour() {
	ColumnDrag drag({ 0, 1, 2 }, { 100, 100, 100 }, 0, 50, Half(0));
	CHECK(!drag.MoveTo(99));
	CHECK(drag.Order() == Order({ 0, 1, 2 }));
	CHECK(drag.MoveTo(101));
	CHECK(drag.Order() == Order({ 1, 0, 2 }));
	CHECK(drag.LeftOf(0) == 100);
	CHECK(drag.DraggedLeft() == 51);
	CHECK(drag.StartOrder() == Order({ 0, 1, 2 }));
}

void TheSlackKeepsAHandOnTheLineFromSwappingBack() {
	ColumnDrag drag({ 0, 1, 2 }, { 100, 100, 100 }, 0, 50, Half(6));
	CHECK(!drag.MoveTo(105));
	CHECK(drag.MoveTo(107));
	CHECK(drag.Order() == Order({ 1, 0, 2 }));
	CHECK(!drag.MoveTo(100));
	CHECK(!drag.MoveTo(95));
	CHECK(drag.MoveTo(93));
	CHECK(drag.Order() == Order({ 0, 1, 2 }));
}

// The dragged column stays inside the header, so the swap with the last column must be reachable from there.
void AWideColumnPassesANarrowLastOne() {
	ColumnDrag drag({ 0, 1 }, { 200, 50 }, 0, 100, Half(6));
	CHECK(drag.MoveTo(10000));
	CHECK(drag.Order() == Order({ 1, 0 }));
	CHECK(drag.DraggedLeft() == 50);
	CHECK(drag.MoveTo(-10000));
	CHECK(drag.Order() == Order({ 0, 1 }));
	CHECK(drag.DraggedLeft() == 0);
}

void AFastMovePassesSeveralColumnsAtOnce() {
	ColumnDrag drag({ 0, 1, 2, 3 }, { 100, 100, 100, 100 }, 0, 50, Half(6));
	CHECK(drag.MoveTo(1000));
	CHECK(drag.Order() == Order({ 1, 2, 3, 0 }));
	ColumnDrag back({ 0, 1, 2, 3 }, { 100, 100, 100, 100 }, 3, 350, Half(6));
	CHECK(back.MoveTo(-1000));
	CHECK(back.Order() == Order({ 3, 0, 1, 2 }));
}

// Column 1 has no width, as the info column without cover text: it stays right after column 0, the cover.
void AColumnWithNoWidthStaysAfterTheColumnBeforeIt() {
	ColumnDrag passing({ 0, 1, 2, 3 }, { 100, 0, 100, 100 }, 2, 150, Half(6));
	CHECK(passing.MoveTo(0));
	CHECK(passing.Order() == Order({ 2, 0, 1, 3 }));
	ColumnDrag cover({ 0, 1, 2, 3 }, { 100, 0, 100, 100 }, 0, 50, Half(6));
	CHECK(cover.MoveTo(1000));
	CHECK(cover.Order() == Order({ 2, 3, 0, 1 }));
}

void AColumnWithNoWidthAtTheStartStaysBeforeTheFirstColumn() {
	ColumnDrag drag({ 0, 1, 2 }, { 0, 100, 100 }, 2, 150, Half(6));
	CHECK(drag.MoveTo(0));
	CHECK(drag.Order() == Order({ 2, 0, 1 }));
}

void ADeeperSwapNeedsMoreOfTheNeighbourCovered() {
	ColumnDragFeel feel;
	feel.swapDepth = 0.6;
	ColumnDrag drag({ 0, 1, 2 }, { 100, 100, 100 }, 0, 50, feel);
	CHECK(!drag.MoveTo(110));
	CHECK(drag.MoveTo(111));
	CHECK(drag.Order() == Order({ 1, 0, 2 }));
	CHECK(!drag.MoveTo(91));
	CHECK(drag.MoveTo(89));
	CHECK(drag.Order() == Order({ 0, 1, 2 }));
}

// The neighbour gives way by a share of how far it is covered, up to a limit, and the others stay where they are.
void TheNeighbourGivesWayALittleBeforeItSwaps() {
	ColumnDragFeel feel;
	feel.swapDepth = 0.6;
	feel.resistShare = 0.25;
	feel.resistMax = 12;
	ColumnDrag drag({ 0, 1, 2 }, { 100, 100, 100 }, 0, 50, feel);
	CHECK(drag.Resistance(1) == 0);
	CHECK(!drag.MoveTo(90));
	CHECK(drag.Resistance(1) == 10);
	CHECK(drag.Resistance(2) == 0);
	CHECK(drag.Resistance(0) == 0);
	CHECK(!drag.MoveTo(110));
	CHECK(drag.Resistance(1) == 12);
	// Just passed, it holds still while the dragged column goes on to its new place, so it is not pushed into the
	// column beyond. Once the dragged column got there, coming back is met with the give again.
	CHECK(drag.MoveTo(111));
	CHECK(drag.Resistance(1) == 0);
	CHECK(drag.Resistance(2) == 0);
	CHECK(!drag.MoveTo(150));
	CHECK(drag.Resistance(1) == 0);
	CHECK(!drag.MoveTo(120));
	CHECK(drag.Resistance(1) == -8);
}

bool Near(double a, double b, double within = 1e-6) {
	return std::fabs(a - b) < within;
}

void AMovedColumnSlidesHomeSlowingDown() {
	ColumnSlides slides(3, 150);
	slides.Moved(1, 100, 0);
	CHECK(Near(slides.Offset(1, 0), -100));
	CHECK(Near(slides.Offset(1, 75), -12.5));
	CHECK(Near(slides.Offset(1, 150), 0));
	CHECK(Near(slides.Offset(0, 75), 0));
	CHECK(slides.Running(149));
	CHECK(!slides.Running(150));
}

// A column moved again while sliding goes on from where it is drawn, so it never jumps.
void AColumnMovedWhileSlidingGoesOnFromWhereItIs() {
	ColumnSlides slides(2, 150);
	slides.Moved(1, 100, 0);
	slides.Moved(1, -100, 75);
	CHECK(Near(slides.Offset(1, 75), 87.5));
	CHECK(Near(slides.Offset(1, 225), 0));
}

// With overshoot a column goes about 3% past its place before it settles there.
void WithOvershootAColumnGoesALittlePastItsPlace() {
	ColumnSlides slides(1, 100, 0.9);
	slides.StartFrom(0, 100, 0);
	CHECK(Near(slides.Offset(0, 0), 100));
	CHECK(Near(slides.Offset(0, 68.42), -2.99, 0.05));
	CHECK(Near(slides.Offset(0, 100), 0));
}

void WithoutAnimationAColumnIsAtItsPlaceAtOnce() {
	ColumnSlides slides(2, 0);
	slides.Moved(1, 100, 0);
	CHECK(Near(slides.Offset(1, 0), 0));
	CHECK(!slides.Running(0));
}

// A click that slips a few pixels stays a click; a title held a moment, or flung far at once, starts to move.
void AColumnStartsToMoveWhenHeldOrDraggedFar() {
	CHECK(!ColumnDragStarts(100, 6, 150, 12));
	CHECK(ColumnDragStarts(150, 6, 150, 12));
	CHECK(!ColumnDragStarts(0, 12, 150, 12));
	CHECK(ColumnDragStarts(0, 13, 150, 12));
}

} // namespace

void RunColumnDragTests() {
	AColumnStartsToMoveWhenHeldOrDraggedFar();
	AColumnSwapsWhenItCoversHalfItsNeighbour();
	TheSlackKeepsAHandOnTheLineFromSwappingBack();
	AWideColumnPassesANarrowLastOne();
	AFastMovePassesSeveralColumnsAtOnce();
	AColumnWithNoWidthStaysAfterTheColumnBeforeIt();
	AColumnWithNoWidthAtTheStartStaysBeforeTheFirstColumn();
	ADeeperSwapNeedsMoreOfTheNeighbourCovered();
	TheNeighbourGivesWayALittleBeforeItSwaps();
	WithOvershootAColumnGoesALittlePastItsPlace();
	AMovedColumnSlidesHomeSlowingDown();
	AColumnMovedWhileSlidingGoesOnFromWhereItIs();
	WithoutAnimationAColumnIsAtItsPlaceAtOnce();
}
