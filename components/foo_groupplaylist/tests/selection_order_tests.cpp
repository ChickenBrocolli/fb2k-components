#include "check.h"
#include "selection_order.h"

namespace {

using Order = std::vector<size_t>;

// The order is the one playlist_manager::reorder_items takes: position i receives the track that was at order[i].
void SelectedTracksApartSwapEnds() {
	CHECK((ReversedSelectionOrder({ true, false, true, false, true }) == Order{ 4, 1, 2, 3, 0 }));
	CHECK((ReversedSelectionOrder({ false, true, true, false, true, true }) == Order{ 0, 5, 4, 3, 2, 1 }));
}

void EveryTrackSelectedReversesThePlaylist() {
	CHECK((ReversedSelectionOrder({ true, true, true }) == Order{ 2, 1, 0 }));
}

void OneOrNoSelectedTrackKeepsTheOrder() {
	CHECK((ReversedSelectionOrder({ false, true, false }) == Order{ 0, 1, 2 }));
	CHECK((ReversedSelectionOrder({ false, false }) == Order{ 0, 1 }));
	CHECK(ReversedSelectionOrder({}).empty());
}

} // namespace

void RunSelectionOrderTests() {
	SelectedTracksApartSwapEnds();
	EveryTrackSelectedReversesThePlaylist();
	OneOrNoSelectedTrackKeepsTheOrder();
}
