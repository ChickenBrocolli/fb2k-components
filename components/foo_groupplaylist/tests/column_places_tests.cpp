#include "check.h"

#include "column_places.h"

namespace {

using Order = std::vector<int>;

void TheDefaultKeepsCoverAndInfoAtTheLeft() {
	CHECK(ScreenOrder(3, {}) == Order({ 0, 1, 2, 3, 4 }));
	CHECK(PlacesFromOrder(Order({ 0, 1, 2, 3, 4 })) == BlockColumnPlaces({ 0, 1 }));
}

void CoverAndInfoGoWhereverTheyWereDragged() {
	// [info][Track][Title][Duration][cover]
	CHECK(ScreenOrder(3, { 4, 0 }) == Order({ 1, 2, 3, 4, 0 }));
	// [Track][Title][info][cover][Duration]
	CHECK(ScreenOrder(3, { 3, 2 }) == Order({ 2, 3, 1, 0, 4 }));
	// [Track][cover][Title][Duration][info]
	CHECK(ScreenOrder(3, { 1, 4 }) == Order({ 2, 0, 3, 4, 1 }));
	for (const BlockColumnPlaces places : { BlockColumnPlaces{ 4, 0 }, BlockColumnPlaces{ 3, 2 }, BlockColumnPlaces{ 1, 4 } }) {
		CHECK(PlacesFromOrder(ScreenOrder(3, places)) == places);
	}
}

void PlacesFromAHandEditedFileArePulledIn() {
	CHECK(ScreenOrder(2, { 9, 9 }) == Order({ 2, 3, 0, 1 }));
	CHECK(ScreenOrder(2, { 1, 1 }) == Order({ 2, 0, 1, 3 }));
	CHECK(ScreenOrder(0, {}) == Order({ 0, 1 }));
	CHECK(ScreenOrder(0, { 5, 0 }) == Order({ 1, 0 }));
}

void ARemovedColumnPullsThePlacesToItsRightIn() {
	// [info][Track][Title][Duration][cover] without Title: [info][Track][Duration][cover]
	CHECK(PlacesAfterTrackChange({ 4, 0 }, { true, false, true }, 0) == BlockColumnPlaces({ 3, 0 }));
	// [Track][cover][Title][Duration][info] without Track: [cover][Title][Duration][info]
	CHECK(PlacesAfterTrackChange({ 1, 4 }, { false, true, true }, 0) == BlockColumnPlaces({ 0, 3 }));
}

// A column turned on goes after the last track column, so a cover at the right end stays there.
void ANewColumnGoesAfterTheLastTrackColumn() {
	CHECK(PlacesAfterTrackChange({ 4, 0 }, { true, true, true }, 1) == BlockColumnPlaces({ 5, 0 }));
	CHECK(PlacesAfterTrackChange({}, { true, true, true }, 1) == BlockColumnPlaces({ 0, 1 }));
	// [Track][cover][Title][info] gets [Track][cover][Title][New][info]
	CHECK(PlacesAfterTrackChange({ 1, 3 }, { true, true }, 1) == BlockColumnPlaces({ 1, 4 }));
	// No track columns at all: the new one goes after the cover and info.
	CHECK(PlacesAfterTrackChange({}, {}, 1) == BlockColumnPlaces({ 0, 1 }));
}

void LabelsGoOverTheWidestRunOfTrackColumns() {
	// [cover][info][Track][Title][Duration]
	const auto left = WidestTrackRun(Order({ 0, 1, 2, 3, 4 }), { 100, 130, 50, 300, 50 });
	CHECK(left && left->first == 2 && left->second == 4);
	// [Track][Title][Duration][info][cover]
	const auto right = WidestTrackRun(Order({ 2, 3, 4, 1, 0 }), { 50, 300, 50, 130, 100 });
	CHECK(right && right->first == 0 && right->second == 2);
	// [Track][cover][Title][Duration]: Title and Duration are wider together.
	const auto split = WidestTrackRun(Order({ 2, 0, 3, 4, 1 }), { 50, 100, 300, 50, 0 });
	CHECK(split && split->first == 2 && split->second == 3);
	// Two runs as wide: the left one.
	const auto tie = WidestTrackRun(Order({ 2, 0, 3 }), { 80, 100, 80 });
	CHECK(tie && tie->first == 0 && tie->second == 0);
	CHECK(!WidestTrackRun(Order({ 0, 1 }), { 100, 130 }));
}

} // namespace

void RunColumnPlacesTests() {
	TheDefaultKeepsCoverAndInfoAtTheLeft();
	CoverAndInfoGoWhereverTheyWereDragged();
	PlacesFromAHandEditedFileArePulledIn();
	ARemovedColumnPullsThePlacesToItsRightIn();
	ANewColumnGoesAfterTheLastTrackColumn();
	LabelsGoOverTheWidestRunOfTrackColumns();
}
