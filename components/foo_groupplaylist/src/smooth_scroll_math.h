#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>

// The arithmetic of smooth scrolling, kept apart from the window code so the unit tests build without Windows.
// The display-timed animation it drives is adapted from the smooth scroll helper of ui_helpers by Reupen Shah
// (BSD-3-Clause, see THIRD-PARTY-NOTICES.txt).
//
// Inertia: each wheel notch gives the view a push; the view coasts and slows down by friction. A notch from rest
// moves exactly its distance (times distanceScale), quick notches add up their speed and push harder, and the view
// coasts on after them. The numbers are set on the Scrolling preferences page.

namespace smooth_scroll {

inline constexpr int kWheelDelta = 120;  // WHEEL_DELTA

// The view moves at a speed that friction lowers by the factor e every tau milliseconds.
// A push of d pixels adds d / tau to the speed, so from rest the view coasts exactly d pixels.
// The page calls tau the glide length; it acts as the weight of what a notch pushes.
struct InertiaParams {
	double tauMs = 90.0;  // friction: how long and soft the glide is
	double distanceScale = 1.0;  // times the distance of a notch (the Windows setting)
	double quickGapMs = 120.0;  // notches closer together than this, in the same direction, are quick
	double accelerationStep = 0.4;  // each quick notch in a row pushes this much harder
	double maxAcceleration = 3.0;  // never harder than this many times a single notch

	bool operator==(const InertiaParams&) const = default;
};

inline constexpr double kInertiaTauMs = InertiaParams{}.tauMs;

struct Momentum {
	double position = 0;
	double velocity = 0;  // pixels per millisecond
	double lastMs = 0;
	double lastPushMs = -1e9;
	int streak = 0;  // quick pushes in a row
	double tauMs = kInertiaTauMs;
};

inline Momentum StartMomentum(int currentPosition, double nowMs, const InertiaParams& params = {}) {
	Momentum momentum;
	momentum.position = currentPosition;
	momentum.lastMs = nowMs - 8.0;
	momentum.tauMs = params.tauMs;
	return momentum;
}

// How much harder a push counts after streak quick notches in a row.
inline double AccelerationFactor(int streak, const InertiaParams& params = {}) {
	return (std::max)(1.0, (std::min)(params.maxAcceleration, 1.0 + params.accelerationStep * streak));
}

inline void Push(Momentum& momentum, int delta, double nowMs, const InertiaParams& params = {}) {
	if (momentum.velocity * delta < 0) {
		// A turn the other way stops the coasting first, and its notch is a first notch again.
		momentum.velocity = 0;
		momentum.lastPushMs = -1e9;
	}
	if (params.tauMs != momentum.tauMs) {
		// The friction was changed on the page: the coasting that is left keeps its distance.
		momentum.velocity *= momentum.tauMs / params.tauMs;
		momentum.tauMs = params.tauMs;
	}
	momentum.streak = nowMs - momentum.lastPushMs < params.quickGapMs ? momentum.streak + 1 : 0;
	momentum.lastPushMs = nowMs;
	momentum.velocity += delta * params.distanceScale * AccelerationFactor(momentum.streak, params) / momentum.tauMs;
}

// Moves the view to nowMs. The distance is the exact integral of the slowing speed, so the frame rate does not matter.
inline void Advance(Momentum& momentum, double nowMs) {
	const double elapsed = (std::max)(0.0, nowMs - momentum.lastMs);
	const double decay = std::exp(-elapsed / momentum.tauMs);
	momentum.position += momentum.velocity * momentum.tauMs * (1.0 - decay);
	momentum.velocity *= decay;
	momentum.lastMs = nowMs;
}

// Done when less than half a pixel of coasting is left.
inline bool MomentumDone(const Momentum& momentum) {
	return std::abs(momentum.velocity) * momentum.tauMs < 0.5;
}

// What the last wheel messages were, to tell a mouse wheel from a precision touchpad.
struct WheelHistory {
	std::optional<uint64_t> lastTickMs;
	std::optional<int> lastDelta;
};

// Precision touchpads send many small wheel messages, which already scroll smoothly, and those are not animated.
// Which device sent a message is not in the message, so it is guessed from its size: a mouse wheel moves in whole
// notches (multiples of 120). A whole-notch message among a touchpad's messages (within 2.5 seconds of one) is the
// touchpad too. The time between messages is not used: when painting is slow, a touchpad's messages arrive far apart.
inline bool ShouldAnimateWheel(WheelHistory& history, int wheelDelta, uint64_t nowTickMs) {
	const std::optional<uint64_t> lastTick = history.lastTickMs;
	const std::optional<int> lastDelta = history.lastDelta;
	history.lastTickMs = nowTickMs;
	history.lastDelta = wheelDelta;

	if (wheelDelta == 0 || wheelDelta % kWheelDelta != 0) return false;
	if (!lastTick || nowTickMs - *lastTick >= 2500) return true;
	return !lastDelta || *lastDelta % kWheelDelta == 0;
}

// Pixels the view moves for a wheel message, as the list control moves it without smooth scrolling:
// the Windows setting's lines per notch times the row height, or a page when the setting is "one screen".
// A turn away from the user (a positive delta) moves the view up, to smaller positions.
inline constexpr unsigned kScrollOnePage = 0xFFFFFFFFu;  // WHEEL_PAGESCROLL

inline int WheelPixels(int wheelDelta, unsigned linesPerNotch, int rowHeight, int pageHeight) {
	if (linesPerNotch == kScrollOnePage) {
		const int pages = (std::max)(1, std::abs(wheelDelta) / kWheelDelta);
		return (wheelDelta > 0 ? -1 : 1) * pages * pageHeight;
	}
	const long long lines = (std::max)(1u, linesPerNotch);
	return static_cast<int>(-static_cast<long long>(wheelDelta) * lines * rowHeight / kWheelDelta);
}

} // namespace smooth_scroll
