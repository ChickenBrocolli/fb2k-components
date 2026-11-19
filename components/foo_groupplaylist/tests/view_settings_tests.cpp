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

void InertiaNumbersArePulledIntoTheirRanges() {
	smooth_scroll::InertiaParams wild;
	wild.tauMs = 5.0;
	wild.distanceScale = 99.0;
	wild.quickGapMs = std::nan("");
	wild.accelerationStep = -1.0;
	wild.maxAcceleration = 0.5;
	const smooth_scroll::InertiaParams clamped = ClampInertiaParams(wild);
	CHECK(clamped.tauMs == 30.0);
	CHECK(clamped.distanceScale == 4.0);
	CHECK(clamped.quickGapMs == 40.0);
	CHECK(clamped.accelerationStep == 0.0);
	CHECK(clamped.maxAcceleration == 1.0);
	// The defaults are inside the ranges and stay as they are.
	CHECK(ClampInertiaParams({}) == smooth_scroll::InertiaParams{});
}

// A number is kept with the decimals the page shows it with, so a typed 1.234 is the 1.23 the page shows and saves.
void InertiaNumbersKeepTheDecimalsThePageShows() {
	const InertiaSetting& glide = kInertiaSettings[0];  // no decimals
	const InertiaSetting& distance = kInertiaSettings[1];  // two decimals
	CHECK(ClampInertiaValue(distance, 1.234) == 1.23);
	CHECK(ClampInertiaValue(distance, 1.236) == 1.24);
	CHECK(ClampInertiaValue(kInertiaSettings[3], 0.15) == 0.15);  // the speed-up step, two decimals
	CHECK(ClampInertiaValue(glide, 90.5) == 91.0);
	CHECK(ClampInertiaValue(glide, 90.4) == 90.0);
}

void TheTrackSummaryShowsOnlyWhereItWasPicked() {
	CHECK(ShowsHeaderSummary(true, TrackSummaryPlace::HeadersAndLabels) && ShowsLabelSummary(true, TrackSummaryPlace::HeadersAndLabels));
	CHECK(ShowsHeaderSummary(true, TrackSummaryPlace::Headers) && !ShowsLabelSummary(true, TrackSummaryPlace::Headers));
	CHECK(!ShowsHeaderSummary(true, TrackSummaryPlace::Labels) && ShowsLabelSummary(true, TrackSummaryPlace::Labels));
}

void ATrackSummaryTurnedOffShowsNowhere() {
	for (const TrackSummaryPlace place : { TrackSummaryPlace::HeadersAndLabels, TrackSummaryPlace::Headers, TrackSummaryPlace::Labels }) {
		CHECK(!ShowsHeaderSummary(false, place) && !ShowsLabelSummary(false, place));
	}
}

void AnOrangeNobodyChangedFollowsFoobar() {
	CHECK(!UsesOwnTextColor(-1, kOrangeTextColor));
	CHECK(UsesOwnTextColor(-1, 0x00FF0000));
}

void AStoredChoiceIsKept() {
	CHECK(UsesOwnTextColor(1, kOrangeTextColor));
	CHECK(!UsesOwnTextColor(0, 0x00FF0000));
}

} // namespace

void RunViewSettingsTests() {
	AnOrangeNobodyChangedFollowsFoobar();
	AStoredChoiceIsKept();
	TheTrackSummaryShowsOnlyWhereItWasPicked();
	ATrackSummaryTurnedOffShowsNowhere();
	ACoverSizeInsideTheRangeIsKept();
	ACoverSizeOutsideTheRangeIsPulledIn();
	ARowPaddingOutsideTheRangeIsPulledIn();
	TheDefaultsLieInsideTheirRanges();
	InertiaNumbersArePulledIntoTheirRanges();
	InertiaNumbersKeepTheDecimalsThePageShows();
}
