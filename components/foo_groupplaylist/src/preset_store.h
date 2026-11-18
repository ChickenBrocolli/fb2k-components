#pragma once

#include "group_preset.h"

// Group presets are shared by every panel and kept in the foobar2000 configuration.
// The loaded list always has at least one preset.
std::vector<GroupPreset> LoadGroupPresets();
void SaveGroupPresets(const std::vector<GroupPreset>& presets);
