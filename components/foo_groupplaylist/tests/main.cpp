#include "check.h"

void RunGroupModelTests();
void RunColumnDefTests();
void RunGroupPresetTests();
void RunRatingCellTests();
void RunViewSettingsTests();
void RunTextPartsTests();
void RunUiTextTests();
void RunSmoothScrollTests();

int main() {
	RunGroupModelTests();
	RunColumnDefTests();
	RunGroupPresetTests();
	RunRatingCellTests();
	RunViewSettingsTests();
	RunTextPartsTests();
	RunUiTextTests();
	RunSmoothScrollTests();

	if (FailureCount() == 0) {
		std::printf("unit tests: all checks passed\n");
		return 0;
	}
	std::printf("unit tests: %d check(s) failed\n", FailureCount());
	return 1;
}
