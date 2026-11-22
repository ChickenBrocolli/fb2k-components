#pragma once

class PlaylistView;

// A small window of check boxes, opened by a right click on the info lines beside a cover: each box shows or hides a
// line of album information for the panel's preset at once, beside what the line gives for that cover. At least one
// line stays ticked: to show no info, the info column is the thing to hide. The rows drag to change the order of the
// lines. A click elsewhere or Esc closes it. One is open at a time.
void ShowInfoLinesPopup(PlaylistView& view, size_t block, CPoint screenPoint);
// Closes the popup when it belongs to this panel, which is going away.
void CloseInfoLinesPopupOf(const PlaylistView* view);
