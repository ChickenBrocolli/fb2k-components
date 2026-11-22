#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// Timing of the panel's small animations: covers fading in, column widths sliding, a heart or star popping.
// Standard library only, so the unit tests build without the foobar2000 SDK.

inline double SteadyNowMs() {
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// How far along an animation that started at startMs is at nowMs, from 0 to 1. One without a duration is done.
inline double Progress(double startMs, double nowMs, double durationMs) {
	if (durationMs <= 0) return 1;
	return std::clamp((nowMs - startMs) / durationMs, 0.0, 1.0);
}

// Fast at first and slowing into the end.
inline double EaseOut(double t) {
	const double left = 1 - t;
	return 1 - left * left * left;
}

// A cover that arrived at arrivedMs comes in from see-through, from 0 to 255. One that arrived at 0 (it took the place
// of the same cover at another size, which was already shown) is there at once.
inline int FadeInOpacity(double arrivedMs, double nowMs, double durationMs) {
	if (arrivedMs <= 0) return 255;
	return static_cast<int>(std::lround(255 * EaseOut(Progress(arrivedMs, nowMs, durationMs))));
}

// A clicked heart or star grows by up to amount (0.3 is 30% larger) and goes back, quick out and slower back.
inline double PopScale(double startMs, double nowMs, double durationMs, double amount) {
	const double t = Progress(startMs, nowMs, durationMs);
	if (t >= 1) return 1;
	// Up in the first third, back in the rest.
	const double rise = 1.0 / 3;
	const double shape = t < rise ? EaseOut(t / rise) : 1 - EaseOut((t - rise) / (1 - rise));
	return 1 + amount * shape;
}

// Column widths sliding from one set to another. The edges between the columns slide, not each width on its own,
// so when both sets have the same total the columns fill the same space in every frame.
class WidthSlide {
public:
	WidthSlide(std::vector<int> from, std::vector<int> to, double startMs, double durationMs)
		: m_from(std::move(from)), m_to(std::move(to)), m_startMs(startMs), m_durationMs(durationMs) {
		m_to.resize(m_from.size(), 0);
	}

	std::vector<int> At(double nowMs) const {
		const double t = EaseOut(Progress(m_startMs, nowMs, m_durationMs));
		std::vector<int> widths(m_from.size());
		double fromEdge = 0;
		double toEdge = 0;
		int placed = 0;
		for (size_t index = 0; index < m_from.size(); ++index) {
			fromEdge += m_from[index];
			toEdge += m_to[index];
			const int edge = static_cast<int>(std::lround(fromEdge + (toEdge - fromEdge) * t));
			widths[index] = (std::max)(0, edge - placed);
			placed += widths[index];
		}
		return widths;
	}

	bool Done(double nowMs) const { return Progress(m_startMs, nowMs, m_durationMs) >= 1; }
	const std::vector<int>& To() const { return m_to; }

private:
	std::vector<int> m_from;
	std::vector<int> m_to;
	double m_startMs;
	double m_durationMs;
};
