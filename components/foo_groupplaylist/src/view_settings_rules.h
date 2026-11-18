#pragma once

// Limits of the settings on the Preferences page. All sizes are pixels at 96 DPI.
// Standard library only, so the unit tests build without the foobar2000 SDK.

#include "smooth_scroll_math.h"

constexpr int kDefaultCoverSize = 98;
constexpr int kMinCoverSize = 48;
constexpr int kMaxCoverSize = 300;

constexpr int kDefaultRowPadding = 0;
constexpr int kMinRowPadding = 0;
constexpr int kMaxRowPadding = 20;

inline int ClampSetting(int value, int minimum, int maximum) {
	return value < minimum ? minimum : value > maximum ? maximum : value;
}

inline int ClampCoverSize(int size) {
	return ClampSetting(size, kMinCoverSize, kMaxCoverSize);
}

inline int ClampRowPadding(int padding) {
	return ClampSetting(padding, kMinRowPadding, kMaxRowPadding);
}

// The numbers of the Inertia + acceleration scroll style on the Scrolling page (smooth_scroll::InertiaParams).
// Each has a range and the number of decimals it is shown with; the page's slider moves in steps of the last decimal.
struct InertiaSetting {
	double smooth_scroll::InertiaParams::* field;
	double minimum;
	double maximum;
	int decimals;
};

inline constexpr InertiaSetting kInertiaSettings[] = {
	{ &smooth_scroll::InertiaParams::tauMs, 30.0, 400.0, 0 },
	{ &smooth_scroll::InertiaParams::distanceScale, 0.25, 4.0, 2 },
	{ &smooth_scroll::InertiaParams::quickGapMs, 40.0, 400.0, 0 },
	{ &smooth_scroll::InertiaParams::accelerationStep, 0.0, 2.0, 2 },
	{ &smooth_scroll::InertiaParams::maxAcceleration, 1.0, 10.0, 1 },
};

inline double ClampInertiaValue(const InertiaSetting& setting, double value) {
	if (!(value >= setting.minimum)) return setting.minimum;  // also NaN
	return value > setting.maximum ? setting.maximum : value;
}

inline smooth_scroll::InertiaParams ClampInertiaParams(smooth_scroll::InertiaParams params) {
	for (const InertiaSetting& setting : kInertiaSettings) {
		params.*setting.field = ClampInertiaValue(setting, params.*setting.field);
	}
	return params;
}
