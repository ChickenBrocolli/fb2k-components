#pragma once

#include "view_settings_rules.h"

// The look of every GroupPlaylist panel, set on the Preferences page and kept in the foobar2000 configuration.
// smoothScroll and pinHeaders are also on the panel header menu, so their effect can be tried at once.
// The panel background and track text colours are not here: they come from the Default UI colour settings.
struct ViewSettings {
	bool smoothScroll = false;
	// Keeps the header of the group at the top in view while its tracks scroll under it.
	bool pinHeaders = false;
	// When false the panel uses the Default UI playlist font.
	// Both fonts are kept at their size at 100% display scaling (96 DPI). FontAtDpi gives the size to draw with.
	bool customTrackFont = false;
	LOGFONTW trackFont = {};
	// When false the group header uses the track font, larger and bold, as the list control draws it.
	bool customHeaderFont = false;
	LOGFONTW headerFont = {};
	COLORREF headerColor = RGB(230, 126, 34);
	COLORREF labelColor = RGB(230, 126, 34);
	int rowPadding = kDefaultRowPadding;
	int coverSize = kDefaultCoverSize;
	// Behaviour.
	// Off by default: the panel keeps one preset for every playlist until the user asks for one per playlist.
	bool presetPerPlaylist = false;
	bool dimBrackets = true;
	bool sortOnHeaderClick = true;
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

// Whether the Duration column of the playing track counts down (-remaining / total) or up (elapsed / total).
// A click on that cell switches it. Kept apart from the Preferences page values.
bool GetDurationCountsDown();
void SetDurationCountsDown(bool countsDown);
