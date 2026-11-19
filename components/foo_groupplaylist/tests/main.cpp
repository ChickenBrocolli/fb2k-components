#include "check.h"

void RunGroupModelTests();
void RunColumnDefTests();
void RunGroupPresetTests();
void RunRatingCellTests();
void RunViewSettingsTests();
void RunTextPartsTests();
void RunUiTextTests();
void RunSmoothScrollTests();
void RunSettingsTests();
void RunHeaderSortTests();
void RunColumnFitTests();
void RunColumnPlacesTests();
void RunSelectionOrderTests();
void RunTrackRecordTests();
void RunLabelFitTests();
void RunViewSettingsFieldsTests();

int main() {
	RunGroupModelTests();
	RunColumnDefTests();
	RunGroupPresetTests();
	RunRatingCellTests();
	RunViewSettingsTests();
	RunTextPartsTests();
	RunUiTextTests();
	RunSmoothScrollTests();
	RunSettingsTests();
	RunHeaderSortTests();
	RunColumnFitTests();
	RunColumnPlacesTests();
	RunSelectionOrderTests();
	RunTrackRecordTests();
	RunLabelFitTests();
	RunViewSettingsFieldsTests();

	if (FailureCount() == 0) {
		std::printf("unit tests: all checks passed\n");
		return 0;
	}
	std::printf("unit tests: %d check(s) failed\n", FailureCount());
	return 1;
}
