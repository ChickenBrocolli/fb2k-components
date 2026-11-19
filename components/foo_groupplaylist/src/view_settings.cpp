#include "stdafx.h"

#include "view_settings.h"

#include <optional>

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
// -1 until saved once: UsesOwnTextColor then keeps a colour picked before 0.9.2.
cfg_int cfg_customHeaderColor(SettingGuid(29), -1);
cfg_int cfg_customLabelColor(SettingGuid(30), -1);
cfg_bool cfg_customStarColor(SettingGuid(31), kDefaults.customStarColor);
cfg_uint cfg_starColor(SettingGuid(32), kDefaults.starColor);
cfg_int cfg_rowPadding(SettingGuid(8), kDefaults.rowPadding);
cfg_int cfg_coverSize(SettingGuid(9), kDefaults.coverSize);
cfg_bool cfg_presetPerPlaylist(SettingGuid(10), kDefaults.presetPerPlaylist);
cfg_bool cfg_dimBrackets(SettingGuid(11), kDefaults.dimBrackets);
cfg_bool cfg_sortOnHeaderClick(SettingGuid(12), kDefaults.sortOnHeaderClick);
cfg_bool cfg_scrollPerPlaylist(SettingGuid(13), kDefaults.scrollPerPlaylist);
cfg_bool cfg_boldPlaying(SettingGuid(14), kDefaults.boldPlaying);
cfg_bool cfg_durationCountsDown(SettingGuid(15), false);
cfg_bool cfg_pinHeaders(SettingGuid(16), kDefaults.pinHeaders);
// SettingGuid(17) was the scroll style of the test menu, which is gone. Do not reuse it.
const smooth_scroll::InertiaParams kDefaultInertia;
cfg_float cfg_inertiaTau(SettingGuid(18), kDefaultInertia.tauMs);
cfg_float cfg_inertiaDistance(SettingGuid(19), kDefaultInertia.distanceScale);
cfg_float cfg_inertiaQuickGap(SettingGuid(20), kDefaultInertia.quickGapMs);
cfg_float cfg_inertiaStep(SettingGuid(21), kDefaultInertia.accelerationStep);
cfg_float cfg_inertiaMax(SettingGuid(22), kDefaultInertia.maxAcceleration);
cfg_string cfg_scrollPresetName(SettingGuid(23), kDefaultScrollPresetName);
cfg_string cfg_userScrollPresets(SettingGuid(24), "");
cfg_bool cfg_dragColumns(SettingGuid(25), kDefaults.dragColumns);
cfg_bool cfg_showTrackSummary(SettingGuid(26), kDefaults.showTrackSummary);
// 27 held a switch of a 0.9.2 build that was never released. It stays unused, so its saved value is not read as another type.
cfg_int cfg_trackSummaryPlace(SettingGuid(28), static_cast<int>(kDefaults.trackSummaryPlace));

// The numbers the panels scroll with: the saved ones until the Scrolling page changes them.
std::optional<smooth_scroll::InertiaParams> g_liveInertia;

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
	settings.customHeaderColor = UsesOwnTextColor(static_cast<int>(cfg_customHeaderColor.get()), settings.headerColor);
	settings.customLabelColor = UsesOwnTextColor(static_cast<int>(cfg_customLabelColor.get()), settings.labelColor);
	settings.customStarColor = cfg_customStarColor;
	settings.starColor = static_cast<COLORREF>(cfg_starColor.get());
	settings.rowPadding = ClampRowPadding(static_cast<int>(cfg_rowPadding.get()));
	settings.coverSize = ClampCoverSize(static_cast<int>(cfg_coverSize.get()));
	settings.presetPerPlaylist = cfg_presetPerPlaylist;
	settings.dimBrackets = cfg_dimBrackets;
	settings.showTrackSummary = cfg_showTrackSummary;
	settings.trackSummaryPlace = TrackSummaryPlaceFrom(static_cast<int>(cfg_trackSummaryPlace.get()));
	settings.sortOnHeaderClick = cfg_sortOnHeaderClick;
	settings.dragColumns = cfg_dragColumns;
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
	cfg_customHeaderColor = settings.customHeaderColor ? 1 : 0;
	cfg_customLabelColor = settings.customLabelColor ? 1 : 0;
	cfg_customStarColor = settings.customStarColor;
	cfg_starColor = static_cast<t_uint32>(settings.starColor);
	cfg_rowPadding = ClampRowPadding(settings.rowPadding);
	cfg_coverSize = ClampCoverSize(settings.coverSize);
	cfg_presetPerPlaylist = settings.presetPerPlaylist;
	cfg_dimBrackets = settings.dimBrackets;
	cfg_showTrackSummary = settings.showTrackSummary;
	cfg_trackSummaryPlace = static_cast<int>(settings.trackSummaryPlace);
	cfg_sortOnHeaderClick = settings.sortOnHeaderClick;
	cfg_dragColumns = settings.dragColumns;
	cfg_scrollPerPlaylist = settings.scrollPerPlaylist;
	cfg_boldPlaying = settings.boldPlaying;
	cfg_pinHeaders = settings.pinHeaders;
}

smooth_scroll::InertiaParams LoadInertiaParams() {
	smooth_scroll::InertiaParams params;
	params.tauMs = cfg_inertiaTau;
	params.distanceScale = cfg_inertiaDistance;
	params.quickGapMs = cfg_inertiaQuickGap;
	params.accelerationStep = cfg_inertiaStep;
	params.maxAcceleration = cfg_inertiaMax;
	return ClampInertiaParams(params);
}

void SaveInertiaParams(const smooth_scroll::InertiaParams& params) {
	const smooth_scroll::InertiaParams clamped = ClampInertiaParams(params);
	cfg_inertiaTau = clamped.tauMs;
	cfg_inertiaDistance = clamped.distanceScale;
	cfg_inertiaQuickGap = clamped.quickGapMs;
	cfg_inertiaStep = clamped.accelerationStep;
	cfg_inertiaMax = clamped.maxAcceleration;
	g_liveInertia = clamped;
}

const smooth_scroll::InertiaParams& LiveInertiaParams() {
	if (!g_liveInertia) g_liveInertia = LoadInertiaParams();
	return *g_liveInertia;
}

void SetLiveInertiaParams(const smooth_scroll::InertiaParams& params) {
	g_liveInertia = ClampInertiaParams(params);
}

std::string LoadScrollPresetName() {
	return cfg_scrollPresetName.get().c_str();
}

void SaveScrollPresetName(const std::string& name) {
	cfg_scrollPresetName = name.c_str();
}

std::vector<ScrollPreset> LoadUserScrollPresets() {
	return ParseScrollPresets(cfg_userScrollPresets.get().c_str());
}

void SaveUserScrollPresets(const std::vector<ScrollPreset>& presets) {
	cfg_userScrollPresets = SerializeScrollPresets(presets).c_str();
}

bool GetDurationCountsDown() {
	return cfg_durationCountsDown;
}

void SetDurationCountsDown(bool countsDown) {
	cfg_durationCountsDown = countsDown;
}
