#include "check.h"
#include "view_settings_rules.h"

namespace {

void ACoverSizeInsideTheRangeIsKept() {
	CHECK(ClampCoverSize(kDefaultCoverSize) == kDefaultCoverSize);
	CHECK(ClampCoverSize(kMinCoverSize) == kMinCoverSize);
	CHECK(ClampCoverSize(kMaxCoverSize) == kMaxCoverSize);
}

void ACoverSizeOutsideTheRangeIsPulledIn() {
	CHECK(ClampCoverSize(0) == kMinCoverSize);
	CHECK(ClampCoverSize(-5) == kMinCoverSize);
	CHECK(ClampCoverSize(5000) == kMaxCoverSize);
}

void ARowPaddingOutsideTheRangeIsPulledIn() {
	CHECK(ClampRowPadding(kDefaultRowPadding) == kDefaultRowPadding);
	CHECK(ClampRowPadding(-1) == kMinRowPadding);
	CHECK(ClampRowPadding(99) == kMaxRowPadding);
	CHECK(ClampRowPadding(7) == 7);
}

void TheDefaultsLieInsideTheirRanges() {
	CHECK(kMinCoverSize <= kDefaultCoverSize && kDefaultCoverSize <= kMaxCoverSize);
	CHECK(kMinRowPadding <= kDefaultRowPadding && kDefaultRowPadding <= kMaxRowPadding);
}

} // namespace

void RunViewSettingsTests() {
	ACoverSizeInsideTheRangeIsKept();
	ACoverSizeOutsideTheRangeIsPulledIn();
	ARowPaddingOutsideTheRangeIsPulledIn();
	TheDefaultsLieInsideTheirRanges();
}
