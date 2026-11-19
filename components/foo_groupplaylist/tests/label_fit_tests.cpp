#include "check.h"
#include "label_fit.h"

namespace {

constexpr int kGap = 12;

// "Show track count and length on" with its list box, as on the main preferences page.
void AShortTranslationPullsTheListBoxToTheCheckBox() {
	const std::vector<FitBox> boxes = { { 20, 100, 200, 115 }, { 205, 98, 400, 118 } };
	const FitResult fit = FitLabels(boxes, { { 0, 120 } }, kGap);
	CHECK(fit.width[0] == 120);
	CHECK(fit.leftShift[1] == 60);
	CHECK(fit.width[1] == 195);
}

void ALongerTranslationLeavesTheRowAlone() {
	const std::vector<FitBox> boxes = { { 20, 100, 200, 115 }, { 205, 98, 400, 118 } };
	const FitResult fit = FitLabels(boxes, { { 0, 230 } }, kGap);
	CHECK(fit.width[0] == 180);
	CHECK(fit.leftShift[1] == 0);
}

// "Extra row height [edit][spin] px": the edit, its arrows and the unit all move.
void EverythingCloselyAfterTheLabelMovesTogether() {
	const std::vector<FitBox> boxes = { { 20, 100, 120, 112 }, { 122, 98, 170, 114 }, { 170, 98, 180, 114 }, { 185, 100, 200, 112 } };
	const FitResult fit = FitLabels(boxes, { { 0, 60 } }, kGap);
	CHECK(fit.leftShift[1] == 40);
	CHECK(fit.leftShift[2] == 40);
	CHECK(fit.leftShift[3] == 40);
}

// "Own header colour [swatch][Choose]      Own label colour [swatch][Choose]": the right group stays in line.
void AGroupFurtherAlongTheRowStaysPut() {
	const std::vector<FitBox> boxes = {
		{ 20, 100, 140, 112 }, { 142, 99, 175, 113 }, { 180, 98, 240, 114 },
		{ 270, 100, 390, 112 }, { 392, 99, 425, 113 }, { 430, 98, 490, 114 },
	};
	const FitResult fit = FitLabels(boxes, { { 0, 90 }, { 3, 90 } }, kGap);
	CHECK(fit.leftShift[1] == 30 && fit.leftShift[2] == 30);
	CHECK(fit.leftShift[3] == 0);
	CHECK(fit.leftShift[4] == 30 && fit.leftShift[5] == 30);
}

void ABoxOnTheNextRowStaysPut() {
	const std::vector<FitBox> boxes = { { 20, 100, 200, 115 }, { 205, 130, 400, 150 } };
	const FitResult fit = FitLabels(boxes, { { 0, 120 } }, kGap);
	CHECK(fit.leftShift[1] == 0);
}

// A spin box's arrows overlap the right edge of their number box by a few pixels and still move with it.
void SpinArrowsOverlappingTheirBoxMoveWithIt() {
	const std::vector<FitBox> boxes = { { 20, 100, 120, 112 }, { 122, 98, 172, 114 }, { 169, 98, 182, 114 }, { 186, 100, 200, 112 } };
	const FitResult fit = FitLabels(boxes, { { 0, 60 } }, kGap);
	CHECK(fit.leftShift[2] == 40);
	CHECK(fit.leftShift[3] == 40);
}

// Two font choices, each with a Choose button lined up under the other: both shrink by the lesser amount.
void StackedLabelsKeepTheirButtonsInLine() {
	const std::vector<FitBox> boxes = {
		{ 20, 100, 180, 112 }, { 185, 98, 240, 114 },
		{ 20, 120, 180, 132 }, { 185, 118, 240, 134 },
	};
	const FitResult fit = FitLabels(boxes, { { 0, 100 }, { 2, 130 } }, kGap);
	CHECK(fit.leftShift[1] == 30);
	CHECK(fit.leftShift[3] == 30);
	CHECK(fit.width[0] == 130 && fit.width[2] == 130);
}

// A wrapping label in the column cannot shrink, so the whole column stays in line where it was.
void ALabelThatCannotShrinkHoldsItsColumn() {
	const std::vector<FitBox> boxes = {
		{ 20, 100, 120, 112 }, { 125, 98, 300, 114 },
		{ 20, 120, 120, 145 }, { 125, 118, 300, 150 },
	};
	const FitResult fit = FitLabels(boxes, { { 0, 50 }, { 2, 100 } }, kGap);
	CHECK(fit.leftShift[1] == 0);
	CHECK(fit.leftShift[3] == 0);
	CHECK(fit.width[0] == 100);
}

// A list beside a label spans several rows; it is not part of the label's row.
void ATallListBesideALabelStaysPut() {
	const std::vector<FitBox> boxes = { { 20, 100, 120, 112 }, { 125, 95, 400, 300 } };
	const FitResult fit = FitLabels(boxes, { { 0, 60 } }, kGap);
	CHECK(fit.leftShift[1] == 0);
}

// A label that follows another label moves with it and passes both shrinks on.
void ShrinksAddUpAlongAChain() {
	const std::vector<FitBox> boxes = { { 20, 100, 120, 112 }, { 125, 100, 225, 112 }, { 230, 98, 300, 114 } };
	const FitResult fit = FitLabels(boxes, { { 1, 70 }, { 0, 80 } }, kGap);
	CHECK(fit.leftShift[1] == 20);
	CHECK(fit.width[1] == 70);
	CHECK(fit.leftShift[2] == 50);
}

} // namespace

void RunLabelFitTests() {
	AShortTranslationPullsTheListBoxToTheCheckBox();
	ALongerTranslationLeavesTheRowAlone();
	EverythingCloselyAfterTheLabelMovesTogether();
	AGroupFurtherAlongTheRowStaysPut();
	ABoxOnTheNextRowStaysPut();
	SpinArrowsOverlappingTheirBoxMoveWithIt();
	StackedLabelsKeepTheirButtonsInLine();
	ALabelThatCannotShrinkHoldsItsColumn();
	ATallListBesideALabelStaysPut();
	ShrinksAddUpAlongAChain();
}
