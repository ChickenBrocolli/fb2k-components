#pragma once

#include "scroll_presets.h"
#include "view_settings_rules.h"

// The look of every GroupPlaylist panel, set on the Preferences page and kept in the foobar2000 configuration.
// smoothScroll and pinHeaders are also on the panel header menu, so their effect can be tried at once.
// The panel background and track text colours are not here: they come from the Default UI colour settings.
struct ViewSettings {
	// On by default: a mouse wheel glides (smooth_scroll_math.h). A precision touchpad scrolls as it always does.
	bool smoothScroll = true;
	// Keeps the header of the group at the top in view while its tracks scroll under it.
	bool pinHeaders = false;
	// When false the panel uses the Default UI playlist font.
	// Both fonts are kept at their size at 100% display scaling (96 DPI). FontAtDpi gives the size to draw with.
	bool customTrackFont = false;
	LOGFONTW trackFont = {};
	// When false the group header uses the track font, larger and bold, as the list control draws it.
	bool customHeaderFont = false;
	LOGFONTW headerFont = {};
	// When false the group header and label text take foobar2000's highlight colour (Default UI colours),
	// so they follow the colour scheme. The colours below are the user's own, kept while it is off.
	bool customHeaderColor = false;
	bool customLabelColor = false;
	COLORREF headerColor = kOrangeTextColor;
	COLORREF labelColor = kOrangeTextColor;
	// When false the filled rating stars take foobar2000's highlight colour, as the header and label text do.
	bool customStarColor = false;
	COLORREF starColor = kOrangeTextColor;
	int rowPadding = kDefaultRowPadding;
	int coverSize = kDefaultCoverSize;
	// Behaviour.
	// Off by default: the panel keeps one preset for every playlist until the user asks for one per playlist.
	bool presetPerPlaylist = false;
	bool dimBrackets = true;
	// On by default: group headers and label rows show their track count and length at the right end.
	// The place picks one of them, as a grouping by album artist then album may want the count on one only.
	// Turned off and on again, the count comes back where it was.
	bool showTrackSummary = true;
	TrackSummaryPlace trackSummaryPlace = TrackSummaryPlace::HeadersAndLabels;
	bool sortOnHeaderClick = true;
	// On by default: column titles drag to reorder the columns. Off locks the order once the user likes it,
	// as foobar2000 locks its layout outside layout editing mode. Widths can still be dragged.
	bool dragColumns = true;
	bool scrollPerPlaylist = true;
	// Off by default: the default layout has a Playing column, which already marks the playing track.
	bool boldPlaying = false;
};

// A font kept at 96 DPI, sized for drawing at dpi, and back.
inline LOGFONTW FontAtDpi(LOGFONTW font, int dpi) {
	font.lfHeight = MulDiv(font.lfHeight, dpi, 96);
	return font;
}

inline LOGFONTW FontAt96Dpi(LOGFONTW font, int dpi) {
	font.lfHeight = MulDiv(font.lfHeight, 96, dpi);
	return font;
}

ViewSettings DefaultViewSettings();
ViewSettings LoadViewSettings();
void SaveViewSettings(const ViewSettings& settings);

// The numbers of the Inertia + acceleration scroll style, set on the Scrolling page and kept apart from ViewSettings.
// Every panel scrolls with the live numbers. The page changes them while it is open so a change can be felt at once;
// Apply saves them, and a page closed without Apply puts the saved ones back.
smooth_scroll::InertiaParams LoadInertiaParams();
void SaveInertiaParams(const smooth_scroll::InertiaParams& params);
const smooth_scroll::InertiaParams& LiveInertiaParams();
void SetLiveInertiaParams(const smooth_scroll::InertiaParams& params);

// The scroll preset last picked on the Scrolling page (its English key for a built-in one), and the user's presets.
std::string LoadScrollPresetName();
void SaveScrollPresetName(const std::string& name);
std::vector<ScrollPreset> LoadUserScrollPresets();
void SaveUserScrollPresets(const std::vector<ScrollPreset>& presets);

// Whether the Duration column of the playing track counts down (-remaining / total) or up (elapsed / total).
// A click on that cell switches it. Kept apart from the Preferences page values.
bool GetDurationCountsDown();
void SetDurationCountsDown(bool countsDown);
