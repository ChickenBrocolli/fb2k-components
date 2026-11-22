#include "stdafx.h"

#include "preset_store.h"

namespace {

constexpr GUID guid_group_presets = { 0x03488209, 0x0abc, 0x47e2, { 0x91, 0xde, 0x04, 0x17, 0xb8, 0xc9, 0x3d, 0x55 } };

cfg_string cfg_groupPresets(guid_group_presets, SerializePresets(DefaultGroupPresets()).c_str());

} // namespace

std::vector<GroupPreset> LoadGroupPresets() {
	std::vector<GroupPreset> presets = ParsePresets(cfg_groupPresets.get().c_str());
	return presets.empty() ? DefaultGroupPresets() : presets;
}

void SaveGroupPresets(const std::vector<GroupPreset>& presets) {
	cfg_groupPresets = SerializePresets(presets).c_str();
}

namespace {

constexpr GUID guid_info_lines = { 0x9e569fa9, 0xd36d, 0x4157, { 0xa3, 0xb2, 0xcf, 0x80, 0x6d, 0xe9, 0x62, 0x5a } };

cfg_string cfg_infoLines(guid_info_lines, "");

} // namespace

InfoLineChoices LoadInfoLineChoices() {
	return ParseInfoLines(cfg_infoLines.get().c_str());
}

void SaveInfoLineChoices(const InfoLineChoices& choices) {
	cfg_infoLines = SerializeInfoLines(choices).c_str();
}
