#include "check.h"

#include <cmath>
#include <numeric>
#include <vector>

#include "effects.h"

namespace {

bool Near(double a, double b, double tolerance = 0.001) {
	return std::abs(a - b) <= tolerance;
}

int Sum(const std::vector<int>& widths) {
	return std::accumulate(widths.begin(), widths.end(), 0);
}

void ACoverFadesInFastThenSlowly() {
	CHECK(FadeInOpacity(1000, 1000, 150) == 0);
	CHECK(FadeInOpacity(1000, 1075, 150) > 200);
	CHECK(FadeInOpacity(1000, 1150, 150) == 255);
	CHECK(FadeInOpacity(1000, 5000, 150) == 255);
}

// A cover that replaces the same cover at another size, or one drawn with animations off, is not faded.
void ACoverWithoutArrivalIsThereAtOnce() {
	CHECK(FadeInOpacity(0, 1000, 150) == 255);
	CHECK(FadeInOpacity(1000, 1000, 0) == 255);
}

void APopGrowsAndComesBack() {
	CHECK(Near(PopScale(0, 0, 180, 0.3), 1));
	CHECK(Near(PopScale(0, 60, 180, 0.3), 1.3));
	CHECK(PopScale(0, 120, 180, 0.3) > 1);
	CHECK(PopScale(0, 120, 180, 0.3) < 1.3);
	CHECK(Near(PopScale(0, 180, 180, 0.3), 1));
	CHECK(Near(PopScale(0, 900, 180, 0.3), 1));
}

// The edges slide, so columns that keep their total fill the same space in every frame.
void SlidingWidthsKeepTheirTotal() {
	const WidthSlide slide({ 100, 200, 0 }, { 80, 150, 70 }, 0, 120);
	CHECK(slide.At(0) == std::vector<int>({ 100, 200, 0 }));
	for (double now = 0; now <= 130; now += 7) CHECK(Sum(slide.At(now)) == 300);
	CHECK(slide.At(120) == std::vector<int>({ 80, 150, 70 }));
	CHECK(!slide.Done(119));
	CHECK(slide.Done(120));
}

void AColumnFoldsToNothing() {
	const WidthSlide slide({ 50, 120 }, { 0, 170 }, 0, 100);
	const std::vector<int> middle = slide.At(50);
	CHECK(middle[0] > 0 && middle[0] < 50);
	CHECK(Sum(middle) == 170);
	CHECK(slide.At(100) == std::vector<int>({ 0, 170 }));
}

void WithoutDurationTheWidthsAreThereAtOnce() {
	const WidthSlide slide({ 10, 20 }, { 20, 10 }, 0, 0);
	CHECK(slide.At(0) == std::vector<int>({ 20, 10 }));
	CHECK(slide.Done(0));
}

} // namespace

void RunEffectsTests() {
	ACoverFadesInFastThenSlowly();
	ACoverWithoutArrivalIsThereAtOnce();
	APopGrowsAndComesBack();
	SlidingWidthsKeepTheirTotal();
	AColumnFoldsToNothing();
	WithoutDurationTheWidthsAreThereAtOnce();
}
