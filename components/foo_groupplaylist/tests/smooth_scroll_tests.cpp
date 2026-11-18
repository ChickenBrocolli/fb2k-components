#include "check.h"

#include "smooth_scroll_math.h"

using namespace smooth_scroll;

namespace {

void ANotchEasesOutAndEndsOnTheTarget() {
	const Animation animation = StartAnimation(1000, 60, 0.0);
	CHECK(animation.TargetPosition() == 1060);
	// Ease-out: more than half of the way is done at half the time.
	const int half = AnimatedPosition(animation, animation.startMs + kDefaultDurationMs / 2);
	CHECK(half > 1030 && half < 1060);
	CHECK(AnimatedPosition(animation, animation.startMs + kDefaultDurationMs) == 1060);
	CHECK(AnimatedPosition(animation, animation.startMs + 10 * kDefaultDurationMs) == 1060);
	CHECK(AnimatedPosition(animation, animation.startMs - 5) == 1000);
}

void AnUpwardNotchMovesToSmallerPositions() {
	const Animation animation = StartAnimation(1000, -60, 0.0);
	const int mid = AnimatedPosition(animation, animation.startMs + 100);
	CHECK(mid < 1000 && mid > 940);
	CHECK(AnimatedPosition(animation, animation.startMs + kDefaultDurationMs) == 940);
}

void ANotchDuringAnAnimationAddsItsDistance() {
	Animation animation = StartAnimation(1000, 60, 0.0);
	// The view has reached 1040 when the second notch comes.
	ContinueAnimation(animation, 1040, 60, 100.0);
	CHECK(animation.TargetPosition() == 1120);
	CHECK(animation.easing == Easing::Linear);
	CHECK(animation.durationMs == kDefaultDurationMs / 2);
	CHECK(AnimatedPosition(animation, 100.0 + kDefaultDurationMs / 2) == 1120);
	// A third notch of the same size extends the even movement without restarting it.
	ContinueAnimation(animation, 1080, 60, 150.0);
	CHECK(animation.TargetPosition() == 1180);
	CHECK(animation.startMs == 100.0);
	CHECK(animation.durationMs == 150.0 - 100.0 + kDefaultDurationMs / 2);
}

void ATurnTheOtherWayRestartsFromTheCurrentPosition() {
	Animation animation = StartAnimation(1000, 60, 0.0);
	ContinueAnimation(animation, 1040, -60, 100.0);
	CHECK(animation.startPosition == 1040);
	CHECK(animation.TargetPosition() == 1000);
}

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
}

void TheWheelDistanceFollowsTheWindowsSetting() {
	CHECK(WheelPixels(120, 3, 20, 600) == -60);
	CHECK(WheelPixels(-120, 3, 20, 600) == 60);
	CHECK(WheelPixels(240, 3, 20, 600) == -120);
	CHECK(WheelPixels(120, 0, 20, 600) == -20);
	CHECK(WheelPixels(120, kScrollOnePage, 20, 600) == -600);
	CHECK(WheelPixels(-120, kScrollOnePage, 20, 600) == 600);
}

} // namespace

void RunSmoothScrollTests() {
	ANotchEasesOutAndEndsOnTheTarget();
	AnUpwardNotchMovesToSmallerPositions();
	ANotchDuringAnAnimationAddsItsDistance();
	ATurnTheOtherWayRestartsFromTheCurrentPosition();
	AMouseWheelIsAnimatedAndATouchpadIsNot();
	TheWheelDistanceFollowsTheWindowsSetting();
}
