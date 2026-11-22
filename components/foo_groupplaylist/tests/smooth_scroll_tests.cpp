#include <limits>

#include "check.h"

#include "smooth_scroll_math.h"

using namespace smooth_scroll;

namespace {

void AMouseWheelIsAnimatedAndATouchpadIsNot() {
	WheelHistory wheel;
	CHECK(ShouldAnimateWheel(wheel, 120, 1000));
	CHECK(ShouldAnimateWheel(wheel, -120, 1030));
	CHECK(ShouldAnimateWheel(wheel, 240, 1040));

	WheelHistory touchpad;
	CHECK(!ShouldAnimateWheel(touchpad, 30, 1000));
	CHECK(!ShouldAnimateWheel(touchpad, 45, 1008));
	// A large touchpad message right after small ones is not animated either.
	CHECK(!ShouldAnimateWheel(touchpad, 150, 1016));
	// Nor one that comes long after the last, as a touchpad's do while painting is slow.
	CHECK(!ShouldAnimateWheel(touchpad, 3287, 1316));
	// A whole-notch message among a touchpad's is the touchpad too.
	CHECK(!ShouldAnimateWheel(touchpad, 240, 1400));
	// After 2.5 quiet seconds a whole notch is a mouse wheel again, and so is the next one.
	CHECK(ShouldAnimateWheel(touchpad, -120, 4000));
	CHECK(ShouldAnimateWheel(touchpad, -120, 4040));
}

void TheWheelDistanceFollowsTheWindowsSetting() {
	CHECK(WheelPixels(120, 3, 20, 600) == -60);
	CHECK(WheelPixels(-120, 3, 20, 600) == 60);
	CHECK(WheelPixels(240, 3, 20, 600) == -120);
	CHECK(WheelPixels(120, 0, 20, 600) == -20);
	CHECK(WheelPixels(120, kScrollOnePage, 20, 600) == -600);
	CHECK(WheelPixels(-120, kScrollOnePage, 20, 600) == 600);
}

int CoastedDistance(Momentum momentum, double fromMs) {
	double now = fromMs;
	while (!MomentumDone(momentum) && now < fromMs + 10000) {
		now += 7;
		Advance(momentum, now);
	}
	return static_cast<int>(std::lround(momentum.position));
}

void OnePushFromRestCoastsItsDistance() {
	Momentum momentum = StartMomentum(1000, 0.0);
	Push(momentum, 114, 0.0);
	const int end = CoastedDistance(momentum, 0.0);
	CHECK(end >= 1113 && end <= 1114);
}

void QuickPushesAddUpAndAccelerationPushesHarder() {
	InertiaParams noSpeedUp;
	noSpeedUp.accelerationStep = 0.0;
	Momentum plain = StartMomentum(0, 0.0);
	Momentum faster = StartMomentum(0, 0.0);
	for (int i = 0; i < 4; ++i) {
		Advance(plain, i * 40.0);
		Advance(faster, i * 40.0);
		Push(plain, 100, i * 40.0, noSpeedUp);
		Push(faster, 100, i * 40.0);
	}
	const int plainEnd = CoastedDistance(plain, 120.0);
	const int fasterEnd = CoastedDistance(faster, 120.0);
	CHECK(plainEnd >= 398 && plainEnd <= 400);
	CHECK(fasterEnd > plainEnd + 150);
	CHECK(AccelerationFactor(0) == 1.0);
	CHECK(AccelerationFactor(20) == 3.0);
}

void AnInertiaTurnTheOtherWayStopsTheCoasting() {
	Momentum momentum = StartMomentum(0, 0.0);
	Push(momentum, 100, 0.0);
	Advance(momentum, 30.0);
	const double before = momentum.position;
	Push(momentum, -100, 30.0);
	const int end = CoastedDistance(momentum, 30.0);
	CHECK(end >= static_cast<int>(before) - 101 && end <= static_cast<int>(before) - 99);
}


// The defaults are the feel the user picked from the test styles, kept as the Default preset.
void TheDefaultNumbersAreThePickedFeel() {
	const InertiaParams defaults;
	CHECK(defaults.tauMs == 90.0 && defaults.distanceScale == 1.0 && defaults.quickGapMs == 120.0);
	CHECK(defaults.accelerationStep == 0.4 && defaults.maxAcceleration == 3.0);
}

void TheDistanceScaleStretchesANotch() {
	InertiaParams params;
	params.distanceScale = 2.5;
	Momentum momentum = StartMomentum(0, 0.0, params);
	Push(momentum, 100, 0.0, params);
	const int end = CoastedDistance(momentum, 0.0);
	CHECK(end >= 249 && end <= 250);
}

void TheFrictionSetsTheGlideButNotTheDistance() {
	InertiaParams longGlide;
	longGlide.tauMs = 300.0;
	Momentum quick = StartMomentum(0, 0.0);
	Momentum slow = StartMomentum(0, 0.0, longGlide);
	Push(quick, 114, 0.0);
	Push(slow, 114, 0.0, longGlide);
	CHECK(CoastedDistance(slow, 0.0) >= 113 && CoastedDistance(slow, 0.0) <= 114);
	Advance(quick, 200.0);
	Advance(slow, 200.0);
	CHECK(quick.position > 100.0 && slow.position < 80.0);
}

// Changing the friction on the page while the view coasts keeps the distance that is left.
void ANewFrictionKeepsTheCoastingThatIsLeft() {
	Momentum momentum = StartMomentum(0, 0.0);
	Push(momentum, 100, 0.0);
	Advance(momentum, 50.0);
	const double left = momentum.velocityPxPerMs * momentum.tauMs;
	InertiaParams retuned;
	retuned.tauMs = 250.0;
	retuned.accelerationStep = 0.0;
	Push(momentum, 100, 500.0, retuned);  // a slow notch: no acceleration
	CHECK(momentum.tauMs == 250.0);
	CHECK(std::abs(momentum.velocityPxPerMs * momentum.tauMs - (left + 100.0)) < 1e-6);
}

void TheQuickGapDecidesWhichNotchesAccelerate() {
	InertiaParams wide;
	wide.quickGapMs = 200.0;
	Momentum narrowMomentum = StartMomentum(0, 0.0);
	Momentum wideMomentum = StartMomentum(0, 0.0, wide);
	for (int i = 0; i < 3; ++i) {
		Push(narrowMomentum, 100, i * 150.0);
		Push(wideMomentum, 100, i * 150.0, wide);
	}
	CHECK(narrowMomentum.streak == 0);
	CHECK(wideMomentum.streak == 2);
}

void TheStepAndTheMaximumShapeTheAcceleration() {
	InertiaParams params;
	params.accelerationStep = 1.0;
	params.maxAcceleration = 2.5;
	CHECK(AccelerationFactor(0, params) == 1.0);
	CHECK(AccelerationFactor(1, params) == 2.0);
	CHECK(AccelerationFactor(5, params) == 2.5);
	params.maxAcceleration = 1.0;
	CHECK(AccelerationFactor(5, params) == 1.0);
	params.accelerationStep = 0.0;
	params.maxAcceleration = 3.0;
	CHECK(AccelerationFactor(5, params) == 1.0);
	CHECK(AccelerationFactor(3) == 1.0 + 0.4 * 3);
}

// Only whole refreshes are skipped, so frames that take less than a refresh skip none: the 9 ms frames of a 60 Hz
// laptop kept their full frame rate only after ui_helpers's half-refresh threshold was dropped.
void PaintingSkipsOnlyTheWholeRefreshesItTakes() {
	CHECK(RefreshesToSkip(9.0, 16.7) == 0);
	CHECK(RefreshesToSkip(16.6, 16.7) == 0);
	CHECK(RefreshesToSkip(17.0, 16.7) == 1);
	CHECK(RefreshesToSkip(40.0, 16.7) == 2);
	CHECK(RefreshesToSkip(0.0, 16.7) == 0);
	// Before a refresh is measured the shortest one is the largest number, and nothing is skipped.
	CHECK(RefreshesToSkip(5.0, std::numeric_limits<double>::max()) == 0);
}

} // namespace

void RunSmoothScrollTests() {
	AMouseWheelIsAnimatedAndATouchpadIsNot();
	TheWheelDistanceFollowsTheWindowsSetting();
	OnePushFromRestCoastsItsDistance();
	QuickPushesAddUpAndAccelerationPushesHarder();
	AnInertiaTurnTheOtherWayStopsTheCoasting();
	TheDefaultNumbersAreThePickedFeel();
	TheDistanceScaleStretchesANotch();
	TheFrictionSetsTheGlideButNotTheDistance();
	ANewFrictionKeepsTheCoastingThatIsLeft();
	TheQuickGapDecidesWhichNotchesAccelerate();
	TheStepAndTheMaximumShapeTheAcceleration();
	PaintingSkipsOnlyTheWholeRefreshesItTakes();
}
