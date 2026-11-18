#include "stdafx.h"

#include "view_settings.h"

namespace {

// Each setting has its own configuration entry, so a setting added later leaves the saved ones alone.
// The entries share one GUID that differs in its last byte.
GUID SettingGuid(uint8_t index) {
	GUID guid = { 0xae83014e, 0xb07b, 0x4023, { 0x95, 0x85, 0x7e, 0xa1, 0x12, 0x23, 0xe3, 0x00 } };
	guid.Data4[7] = index;
	return guid;
}

LOGFONTW FontNamed(const wchar_t* face, int pointsAt96Dpi, LONG weight) {
	LOGFONTW font = {};
	font.lfHeight = -MulDiv(pointsAt96Dpi, 96, 72);
	font.lfWeight = weight;
	font.lfCharSet = DEFAULT_CHARSET;
	font.lfQuality = CLEARTYPE_QUALITY;
	wcsncpy_s(font.lfFaceName, face, _TRUNCATE);
	return font;
}

const ViewSettings kDefaults = [] {
	ViewSettings settings;
	settings.trackFont = FontNamed(L"Segoe UI", 9, FW_NORMAL);
	settings.headerFont = FontNamed(L"Segoe UI", 11, FW_BOLD);
	return settings;
}();

cfg_bool cfg_smoothScroll(SettingGuid(1), kDefaults.smoothScroll);
cfg_bool cfg_customTrackFont(SettingGuid(2), kDefaults.customTrackFont);
cfg_struct_t<LOGFONTW> cfg_trackFont(SettingGuid(3), kDefaults.trackFont);
cfg_bool cfg_customHeaderFont(SettingGuid(4), kDefaults.customHeaderFont);
cfg_struct_t<LOGFONTW> cfg_headerFont(SettingGuid(5), kDefaults.headerFont);
cfg_uint cfg_headerColor(SettingGuid(6), kDefaults.headerColor);
cfg_uint cfg_labelColor(SettingGuid(7), kDefaults.labelColor);
cfg_int cfg_rowPadding(SettingGuid(8), kDefaults.rowPadding);
cfg_int cfg_coverSize(SettingGuid(9), kDefaults.coverSize);
cfg_bool cfg_presetPerPlaylist(SettingGuid(10), kDefaults.presetPerPlaylist);
cfg_bool cfg_dimBrackets(SettingGuid(11), kDefaults.dimBrackets);
cfg_bool cfg_sortOnHeaderClick(SettingGuid(12), kDefaults.sortOnHeaderClick);
cfg_bool cfg_scrollPerPlaylist(SettingGuid(13), kDefaults.scrollPerPlaylist);
cfg_bool cfg_boldPlaying(SettingGuid(14), kDefaults.boldPlaying);
cfg_bool cfg_durationCountsDown(SettingGuid(15), false);
cfg_bool cfg_pinHeaders(SettingGuid(16), kDefaults.pinHeaders);

} // namespace

ViewSettings DefaultViewSettings() {
	return kDefaults;
}

ViewSettings LoadViewSettings() {
	ViewSettings settings;
	settings.smoothScroll = cfg_smoothScroll;
	settings.customTrackFont = cfg_customTrackFont;
	settings.trackFont = cfg_trackFont.get();
	settings.customHeaderFont = cfg_customHeaderFont;
	settings.headerFont = cfg_headerFont.get();
	settings.headerColor = static_cast<COLORREF>(cfg_headerColor.get());
	settings.labelColor = static_cast<COLORREF>(cfg_labelColor.get());
	settings.rowPadding = ClampRowPadding(static_cast<int>(cfg_rowPadding.get()));
	settings.coverSize = ClampCoverSize(static_cast<int>(cfg_coverSize.get()));
	settings.presetPerPlaylist = cfg_presetPerPlaylist;
	settings.dimBrackets = cfg_dimBrackets;
	settings.sortOnHeaderClick = cfg_sortOnHeaderClick;
	settings.scrollPerPlaylist = cfg_scrollPerPlaylist;
	settings.boldPlaying = cfg_boldPlaying;
	settings.pinHeaders = cfg_pinHeaders;
	return settings;
}

void SaveViewSettings(const ViewSettings& settings) {
	cfg_smoothScroll = settings.smoothScroll;
	cfg_customTrackFont = settings.customTrackFont;
	cfg_trackFont = settings.trackFont;
	cfg_customHeaderFont = settings.customHeaderFont;
	cfg_headerFont = settings.headerFont;
	cfg_headerColor = static_cast<t_uint32>(settings.headerColor);
	cfg_labelColor = static_cast<t_uint32>(settings.labelColor);
	cfg_rowPadding = ClampRowPadding(settings.rowPadding);
	cfg_coverSize = ClampCoverSize(settings.coverSize);
	cfg_presetPerPlaylist = settings.presetPerPlaylist;
	cfg_dimBrackets = settings.dimBrackets;
	cfg_sortOnHeaderClick = settings.sortOnHeaderClick;
	cfg_scrollPerPlaylist = settings.scrollPerPlaylist;
	cfg_boldPlaying = settings.boldPlaying;
	cfg_pinHeaders = settings.pinHeaders;
}

bool GetDurationCountsDown() {
	return cfg_durationCountsDown;
}

void SetDurationCountsDown(bool countsDown) {
	cfg_durationCountsDown = countsDown;
}
