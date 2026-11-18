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
