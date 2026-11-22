#pragma once

#include "group_preset.h"
#include "info_lines.h"

// Group presets are shared by every panel and kept in the foobar2000 configuration.
// The loaded list always has at least one preset.
std::vector<GroupPreset> LoadGroupPresets();
void SaveGroupPresets(const std::vector<GroupPreset>& presets);

// The info lines picked for each preset in the info popup (info_lines.h), kept apart from the presets.
InfoLineChoices LoadInfoLineChoices();
void SaveInfoLineChoices(const InfoLineChoices& choices);
