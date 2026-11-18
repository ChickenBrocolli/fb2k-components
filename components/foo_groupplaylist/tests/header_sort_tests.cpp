#include "check.h"

#include "header_sort.h"

namespace {

void ASecondClickOnTheSameColumnReverses() {
	HeaderSortMemory<int> sort;
	CHECK(!sort.Click("Title", 1));
	CHECK(sort.Click("Title", 1));
	CHECK(!sort.Click("Title", 1));
	CHECK(!sort.Click("Artist", 1));
}

// A preset picked after a header sort puts the playlist back in the preset's order.
void APresetIsReAppliedOnlyAfterAHeaderSort() {
	HeaderSortMemory<int> sort;
	CHECK(!sort.SortedByColumn(1));
	sort.Click("Title", 1);
	CHECK(sort.SortedByColumn(1));
	sort.Forget();  // the preset's sort, a drag, or Ctrl+Z
	CHECK(!sort.SortedByColumn(1));
}

// Another playlist was not sorted by the click: a preset picked there keeps its order,
// and the first click there sorts forward again.
void TheSortBelongsToThePlaylistThatWasSorted() {
	HeaderSortMemory<int> sort;
	sort.Click("Title", 1);
	CHECK(!sort.SortedByColumn(2));
	CHECK(!sort.Click("Title", 2));
	CHECK(sort.SortedByColumn(2));
	CHECK(!sort.SortedByColumn(1));
}

} // namespace

void RunHeaderSortTests() {
	ASecondClickOnTheSameColumnReverses();
	APresetIsReAppliedOnlyAfterAHeaderSort();
	TheSortBelongsToThePlaylistThatWasSorted();
}
