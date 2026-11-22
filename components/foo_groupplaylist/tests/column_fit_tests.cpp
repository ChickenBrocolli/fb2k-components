#include "check.h"

#include <numeric>

#include "column_fit.h"

namespace {

int Sum(const std::vector<int>& widths) {
	return std::accumulate(widths.begin(), widths.end(), 0);
}

void TheColumnsFillTheSpaceInTheirProportions() {
	const std::vector<int> widths = FitColumnWidths({ 50.f, 300.f, 50.f }, 800);
	CHECK(widths == std::vector<int>({ 100, 600, 100 }));
	const std::vector<int> narrow = FitColumnWidths({ 50.f, 300.f, 50.f }, 200);
	CHECK(narrow == std::vector<int>({ 25, 150, 25 }));
}

// Three equal columns in 100 pixels cannot be equal; the extra pixel goes to one column, and the sum stays exact.
void RoundingNeverChangesTheTotal() {
	const std::vector<int> widths = FitColumnWidths({ 1.f, 1.f, 1.f }, 100);
	CHECK(Sum(widths) == 100);
	for (const int width : widths) CHECK(width == 33 || width == 34);
	for (int available = 1; available < 2000; available += 37) {
		CHECK(Sum(FitColumnWidths({ 28.f, 50.f, 300.f, 61.5f, 88.f }, available)) == available);
	}
}

void OneColumnTakesEverything() {
	CHECK(FitColumnWidths({ 120.f }, 640) == std::vector<int>({ 640 }));
}

void NoSpaceOrNoColumns() {
	CHECK(FitColumnWidths({}, 500).empty());
	CHECK(FitColumnWidths({ 10.f, 20.f }, 0) == std::vector<int>({ 0, 0 }));
	CHECK(FitColumnWidths({ 10.f, 20.f }, -40) == std::vector<int>({ 0, 0 }));
}

// Widths saved as zero, for example by a column dragged shut, still get a share when no column has a width.
void ColumnsWithoutWeightShareEqually() {
	CHECK(FitColumnWidths({ 0.f, 0.f }, 300) == std::vector<int>({ 150, 150 }));
	CHECK(FitColumnWidths({ 0.f, 100.f }, 300) == std::vector<int>({ 0, 300 }));
}

void TheNeighborGivesWhatTheDraggedColumnTakes() {
	const SharedWidths wider = ShareWithNeighbor(100, 200, 150, 20);
	CHECK(wider.dragged == 150 && wider.neighbor == 150);
	const SharedWidths narrower = ShareWithNeighbor(100, 200, 40, 20);
	CHECK(narrower.dragged == 40 && narrower.neighbor == 260);
}

void NeitherColumnGoesBelowTheMinimum() {
	const SharedWidths wide = ShareWithNeighbor(100, 200, 400, 20);
	CHECK(wide.dragged == 280 && wide.neighbor == 20);
	const SharedWidths shut = ShareWithNeighbor(100, 200, 0, 20);
	CHECK(shut.dragged == 20 && shut.neighbor == 280);
}

// A column already narrower than the minimum is not pushed wider, and its neighbor is not squeezed for it.
void AColumnAlreadyNarrowKeepsItsWidth() {
	const SharedWidths small = ShareWithNeighbor(10, 200, 5, 20);
	CHECK(small.dragged == 10 && small.neighbor == 200);
	const SharedWidths smallNeighbor = ShareWithNeighbor(100, 10, 150, 20);
	CHECK(smallNeighbor.dragged == 100 && smallNeighbor.neighbor == 10);
}

// Track, Title (its text is cut), Rating, Length, Like, with the widths their content needs.
const std::vector<int> kShelf = { 50, 300, 150, 120, 380 };
const std::vector<int> kShelfFits = { 30, 500, 100, 40, 50 };

// Widening the title squeezes the rating to its stars, then the length to its text, then the like column.
void WideningAColumnSqueezesTheNextOnesInTurn() {
	CHECK(PushDivider(kShelf, kShelfFits, 1, 60, 16) == std::vector<int>({ 50, 360, 100, 110, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, 1, 200, 16) == std::vector<int>({ 50, 500, 100, 40, 310 }));
}

void WhenEverythingFitsItsContentTheEdgeStops() {
	const std::vector<int> widths = PushDivider(kShelf, kShelfFits, 1, 2000, 16);
	CHECK(widths == std::vector<int>({ 50, 760, 100, 40, 50 }));
	CHECK(Sum(widths) == Sum(kShelf));
}

// The widths come from the start of the drag, so going back gives every column its width back.
void DraggingBackGivesTheWidthsBack() {
	CHECK(PushDivider(kShelf, kShelfFits, 1, 0, 16) == kShelf);
	CHECK(PushDivider(kShelf, kShelfFits, 1, 60, 16) == std::vector<int>({ 50, 360, 100, 110, 380 }));
}

// Moving the edge left squeezes the columns on its left, the nearest first; the cut title keeps its width as it does
// not touch the edge, and the track column gives its spare room.
void MovingTheEdgeLeftSqueezesTheColumnsOnTheLeft() {
	const std::vector<int> widths = PushDivider(kShelf, kShelfFits, 2, -100, 16);
	CHECK(widths == std::vector<int>({ 30, 300, 100, 190, 380 }));
	CHECK(Sum(widths) == Sum(kShelf));
}

// The title touches the edge right of the track column: its text is already cut, so it gives down to the minimum.
void AColumnAlreadyCutGivesDownToTheMinimumWhenItTouchesTheEdge() {
	CHECK(PushDivider(kShelf, kShelfFits, 0, 200, 16) == std::vector<int>({ 250, 100, 150, 120, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, 0, 400, 16) == std::vector<int>({ 450, 16, 100, 54, 380 }));
	// Shrinking the title by its own right edge.
	CHECK(PushDivider(kShelf, kShelfFits, 1, -250, 16) == std::vector<int>({ 50, 50, 400, 120, 380 }));
}

void ThePanelEdgeDoesNotMove() {
	CHECK(PushDivider(kShelf, kShelfFits, 4, 50, 16) == kShelf);
	CHECK(PushDivider({ 100 }, { 50 }, 0, 50, 16) == std::vector<int>({ 100 }));
}

// Rating and Like hold stars and a heart.
const std::vector<bool> kShelfFixed = { false, false, true, false, true };

PushFeel DragFeel() {
	PushFeel feel;
	feel.minimum = 16;
	feel.catchPixels = 8;
	feel.wallPixels = 24;
	feel.beyondWall = true;
	return feel;
}

// When the rating has given all it may, the edge holds for 8 pixels of the pointer, then catches up with it.
void TheEdgeCatchesWhenAColumnHasGivenAll() {
	const PushFeel feel = DragFeel();
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 50, feel) == std::vector<int>({ 50, 350, 100, 120, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 57, feel) == std::vector<int>({ 50, 350, 100, 120, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 58, feel) == std::vector<int>({ 50, 358, 100, 112, 380 }));
}

// At the wall the edge holds for 24 pixels; past it only the text columns give their content, down to the minimum.
void PastTheWallOnlyTextColumnsAreCut() {
	const PushFeel feel = DragFeel();
	const std::vector<int> atWall = { 50, 760, 100, 40, 50 };
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 470, feel) == atWall);
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 483, feel) == atWall);
	const std::vector<int> past = PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 490, feel);
	CHECK(past == std::vector<int>({ 50, 784, 100, 16, 50 }));
	CHECK(Sum(past) == Sum(kShelf));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 5000, feel) == past);
}

void WithoutGoingPastTheWallTheEdgeStopsThere() {
	PushFeel feel = DragFeel();
	feel.beyondWall = false;
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 5000, feel) == std::vector<int>({ 50, 760, 100, 40, 50 }));
}

// Going back passes the same catches and gives the widths back.
void GoingBackPassesTheSameCatches() {
	const PushFeel feel = DragFeel();
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 55, feel) == std::vector<int>({ 50, 350, 100, 120, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 20, feel) == std::vector<int>({ 50, 320, 130, 120, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 1, 0, feel) == kShelf);
}

void TheCatchesWorkMovingLeftToo() {
	const PushFeel feel = DragFeel();
	// The rating touches the edge and gives 50; then the edge holds; the cut title gives nothing; the track column 20.
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 2, -54, feel) == std::vector<int>({ 50, 300, 100, 170, 380 }));
	CHECK(PushDivider(kShelf, kShelfFits, kShelfFixed, 2, -60, feel) == std::vector<int>({ 40, 300, 100, 180, 380 }));
}

} // namespace

void RunColumnFitTests() {
	TheEdgeCatchesWhenAColumnHasGivenAll();
	PastTheWallOnlyTextColumnsAreCut();
	WithoutGoingPastTheWallTheEdgeStopsThere();
	GoingBackPassesTheSameCatches();
	TheCatchesWorkMovingLeftToo();
	WideningAColumnSqueezesTheNextOnesInTurn();
	WhenEverythingFitsItsContentTheEdgeStops();
	DraggingBackGivesTheWidthsBack();
	MovingTheEdgeLeftSqueezesTheColumnsOnTheLeft();
	AColumnAlreadyCutGivesDownToTheMinimumWhenItTouchesTheEdge();
	ThePanelEdgeDoesNotMove();
	TheColumnsFillTheSpaceInTheirProportions();
	RoundingNeverChangesTheTotal();
	OneColumnTakesEverything();
	NoSpaceOrNoColumns();
	ColumnsWithoutWeightShareEqually();
	TheNeighborGivesWhatTheDraggedColumnTakes();
	NeitherColumnGoesBelowTheMinimum();
	AColumnAlreadyNarrowKeepsItsWidth();
}
