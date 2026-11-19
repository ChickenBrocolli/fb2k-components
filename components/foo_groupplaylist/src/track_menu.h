#pragma once

// The commands of the track menu that the view runs itself: Play needs its focus, which may be a group header,
// and the clipboard items need its paste place. Every other item (Remove, Crop, Sort and the foobar2000 commands
// below them) runs inside RunTrackMenu.
enum class TrackMenuCommand { None, Play, Cut, Copy, Paste };

// The right-click menu of the tracks, laid out as in SimPlaylist: the playlist's own edit items on top,
// then the commands foobar2000 and its components offer for the selected tracks.
// canPlay: the view has a focused track or group header to play.
TrackMenuCommand RunTrackMenu(HWND owner, CPoint screenPoint, bool canPlay);
