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

} // namespace

void RunColumnFitTests() {
	TheColumnsFillTheSpaceInTheirProportions();
	RoundingNeverChangesTheTotal();
	OneColumnTakesEverything();
	NoSpaceOrNoColumns();
	ColumnsWithoutWeightShareEqually();
	TheNeighborGivesWhatTheDraggedColumnTakes();
	NeitherColumnGoesBelowTheMinimum();
	AColumnAlreadyNarrowKeepsItsWidth();
}
