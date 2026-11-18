#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>

// The arithmetic of smooth scrolling, kept apart from the window code so the unit tests build without Windows.
// Adapted from the smooth scroll helper of ui_helpers by Reupen Shah (BSD-3-Clause, see THIRD-PARTY-NOTICES.txt),
// the code Columns UI scrolls with.
//
// A wheel notch starts an animation that moves the view by the notch's distance in 250 ms, quickly at first
// and slowly at the end (cubic ease-out). A notch that arrives while an animation runs adds its distance and
// turns the rest into an even (linear) movement of half the time, so a spinning wheel keeps the view moving
// and moves it faster the faster it spins.

namespace smooth_scroll {

inline constexpr double kDefaultDurationMs = 250.0;
inline constexpr int kWheelDelta = 120;  // WHEEL_DELTA

enum class Easing { Linear, CubicEaseOut };

// Times are milliseconds on one steady clock.
struct Animation {
	double startMs = 0;
	int startPosition = 0;
	int targetDelta = 0;
	int lastDelta = 0;
	double durationMs = kDefaultDurationMs;
	Easing easing = Easing::CubicEaseOut;

	int TargetPosition() const { return startPosition + targetDelta; }
};

// A new animation from the current position. clampedDelta is the distance that stays inside the view.
// It starts 8 ms in the past, so the first frame already moves.
inline Animation StartAnimation(int currentPosition, int clampedDelta, double nowMs, double durationMs = kDefaultDurationMs) {
	return { nowMs - 8.0, currentPosition, clampedDelta, clampedDelta, durationMs, Easing::CubicEaseOut };
}

// Adds a notch's distance to a running animation.
inline void ContinueAnimation(Animation& animation, int currentPosition, int delta, double nowMs, double durationMs = kDefaultDurationMs) {
	if (animation.easing == Easing::Linear && delta == animation.lastDelta) {
		animation.targetDelta += delta;
		animation.durationMs = nowMs - animation.startMs + durationMs / 2.0;
		return;
	}
	animation.targetDelta += animation.startPosition - currentPosition + delta;
	animation.lastDelta = delta;
	animation.startPosition = currentPosition;
	animation.durationMs = durationMs / 2.0;
	animation.startMs = nowMs;
	animation.easing = Easing::Linear;
}

// Where the view should be at nowMs, never past the start or the target.
inline int AnimatedPosition(const Animation& animation, double nowMs) {
	const double time = std::clamp((nowMs - animation.startMs) / animation.durationMs, 0.0, 1.0);
	const double progress = animation.easing == Easing::Linear ? time : 1.0 - std::pow(1.0 - time, 3);
	const int position = animation.startPosition + static_cast<int>(std::lround(animation.targetDelta * progress));
	const int low = animation.startPosition + (std::min)(animation.targetDelta, 0);
	const int high = animation.startPosition + (std::max)(animation.targetDelta, 0);
	return std::clamp(position, low, high);
}

// What the last wheel messages were, to tell a mouse wheel from a precision touchpad.
struct WheelHistory {
	std::optional<uint64_t> lastTickMs;
	std::optional<int> lastDelta;
};

// Precision touchpads send many small wheel messages, which already scroll smoothly, and those are not animated.
// Which device sent a message is not in the message, so it is guessed from the size and timing of the messages.
inline bool ShouldAnimateWheel(WheelHistory& history, int wheelDelta, uint64_t nowTickMs) {
	const std::optional<uint64_t> lastTick = history.lastTickMs;
	const std::optional<int> lastDelta = history.lastDelta;
	history.lastTickMs = nowTickMs;
	history.lastDelta = wheelDelta;

	if (std::abs(wheelDelta) < kWheelDelta) return false;
	const bool coarse = wheelDelta % kWheelDelta == 0;
	if (!lastTick || nowTickMs - *lastTick >= 2500) return coarse;
	if (coarse && (!lastDelta || *lastDelta % kWheelDelta == 0)) return true;
	return nowTickMs - *lastTick > 50;
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
