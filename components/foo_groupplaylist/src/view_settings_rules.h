#pragma once

// Limits of the settings on the Preferences page. All sizes are pixels at 96 DPI.
// Standard library only, so the unit tests build without the foobar2000 SDK.

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
