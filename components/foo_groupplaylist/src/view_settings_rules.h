#pragma once

// Limits of the settings on the Preferences page. All sizes are pixels at 96 DPI.
// Standard library only, so the unit tests build without the foobar2000 SDK.

#include <cmath>
#include <cstdint>

#include "smooth_scroll_math.h"

constexpr int kDefaultCoverSize = 98;
constexpr int kMinCoverSize = 48;
constexpr int kMaxCoverSize = 300;

// The colour of the group header and label text before 0.9.2, as Windows stores it (0x00BBGGRR).
constexpr std::uint32_t kOrangeTextColor = 0x00227EE6;

// The default of a choice whose absence means something: foobar2000 saves a setting only when it differs from its
// default, so it reads back as this until the user's choice is saved once.
constexpr int kNeverSaved = -1;

// Whether the group header or label text uses its own colour rather than foobar2000's highlight colour.
// saved is the stored choice: 1 or 0, or kNeverSaved when it was never stored. Then the text keeps its own colour only if
// someone picked one before 0.9.2, so an orange that was never changed starts following foobar2000.
inline bool UsesOwnTextColor(int saved, std::uint32_t savedColor) {
	return saved < 0 ? savedColor != kOrangeTextColor : saved != 0;
}

// Where group headers and label rows show their track count and length, when that is on.
// The order follows the drop-down list on the Preferences page and the number in a settings file.
enum class TrackSummaryPlace { HeadersAndLabels, Headers, Labels };

// How the Like column draws a liked track: a heart in the star colour (or its own colour), or the red heart emoji.
// A track not liked shows a faded outline heart either way.
enum class LikeStyle { StarColour = 0, Emoji = 1 };
constexpr int kLikeStyleCount = 2;

inline LikeStyle LikeStyleFrom(int number) {
	return number == 1 ? LikeStyle::Emoji : LikeStyle::StarColour;
}

// The heart colour offered first when the user turns on their own: the red most players use.
constexpr std::uint32_t kHeartRedColor = 0x004628DC;
constexpr int kTrackSummaryPlaceCount = 3;

inline bool ShowsHeaderSummary(bool on, TrackSummaryPlace place) {
	return on && place != TrackSummaryPlace::Labels;
}

inline bool ShowsLabelSummary(bool on, TrackSummaryPlace place) {
	return on && place != TrackSummaryPlace::Headers;
}

// A place read from the saved settings or a settings file. A number outside the list means both.
inline TrackSummaryPlace TrackSummaryPlaceFrom(int number) {
	return number >= 0 && number < kTrackSummaryPlaceCount ? static_cast<TrackSummaryPlace>(number) : TrackSummaryPlace::HeadersAndLabels;
}

constexpr int kDefaultRowPadding = 0;
constexpr int kMinRowPadding = 0;
constexpr int kMaxRowPadding = 20;

inline int ClampSetting(int value, int minimum, int maximum) {
	return value < minimum ? minimum : value > maximum ? maximum : value;
}

inline int ClampCoverSize(int size) {
	return ClampSetting(size, kMinCoverSize, kMaxCoverSize);
}

// The cover column holds the cover and this margin on each side.
constexpr int kCoverInset = 4;

inline int CoverColumnWidth(int coverSize) {
	return coverSize + 2 * kCoverInset;
}

// The cover size a dragged cover column width asks for.
inline int CoverSizeForColumnWidth(int width) {
	return ClampCoverSize(width - 2 * kCoverInset);
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

// Pulled into the range and rounded to the decimals the page shows. Otherwise a typed 1.234 would scroll as 1.234,
// show as 1.23 and be saved in a preset as 1.23, and the page would never stop calling the preset changed.
inline double ClampInertiaValue(const InertiaSetting& setting, double value) {
	if (!(value >= setting.minimum)) return setting.minimum;  // also NaN
	if (value > setting.maximum) return setting.maximum;
	const double scale = std::pow(10.0, setting.decimals);
	return std::round(value * scale) / scale;
}

inline smooth_scroll::InertiaParams ClampInertiaParams(smooth_scroll::InertiaParams params) {
	for (const InertiaSetting& setting : kInertiaSettings) {
		params.*setting.field = ClampInertiaValue(setting, params.*setting.field);
	}
	return params;
}
