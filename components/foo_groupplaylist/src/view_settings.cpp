#include "stdafx.h"

#include "view_settings.h"

#include <functional>
#include <memory>
#include <optional>

#include "view_settings_fields.h"

namespace {

// Each setting has its own configuration entry, so a setting added later leaves the saved ones alone.
// The entries share one GUID that differs in its last byte.
GUID SettingGuid(uint8_t index) {
	GUID guid = { 0xae83014e, 0xb07b, 0x4023, { 0x95, 0x85, 0x7e, 0xa1, 0x12, 0x23, 0xe3, 0x00 } };
	guid.Data4[7] = index;
	return guid;
}

LOGFONTW FontNamed(const wchar_t* face, int pointsAt96Dpi, LONG weight) {
	LOGFONTW font = {};
	font.lfHeight = -MulDiv(pointsAt96Dpi, 96, 72);
	font.lfWeight = weight;
	font.lfCharSet = DEFAULT_CHARSET;
	font.lfQuality = CLEARTYPE_QUALITY;
	wcsncpy_s(font.lfFaceName, face, _TRUNCATE);
	return font;
}

const ViewSettings kDefaults = [] {
	ViewSettings settings;
	settings.trackFont = FontNamed(L"Segoe UI", 9, FW_NORMAL);
	settings.headerFont = FontNamed(L"Segoe UI", 11, FW_BOLD);
	return settings;
}();

// One configuration entry per setting of ViewSettings, made from the list in view_settings_fields.h when the
// component loads, before foobar2000 reads the configuration. Each keeps the type it has always been saved as.
struct StoredSetting {
	std::function<void(ViewSettings&)> load;
	std::function<void(const ViewSettings&)> save;
};

std::vector<StoredSetting> MakeStoredSettings() {
	using namespace view_settings_fields;
	std::vector<StoredSetting> stored;
	VisitViewSettings([&](const auto& field) {
		using F = std::decay_t<decltype(field)>;
		const GUID guid = SettingGuid(field.store);
		const auto member = field.member;
		if constexpr (std::is_same_v<F, BoolField>) {
			auto cfg = std::make_shared<cfg_bool>(guid, kDefaults.*member);
			stored.push_back({ [cfg, member](ViewSettings& v) { v.*member = cfg->get(); }, [cfg, member](const ViewSettings& v) { *cfg = v.*member; } });
		} else if constexpr (std::is_same_v<F, IntField>) {
			auto cfg = std::make_shared<cfg_int>(guid, kDefaults.*member);
			const int minimum = field.minimum, maximum = field.maximum;
			stored.push_back({ [=](ViewSettings& v) { v.*member = ClampSetting(static_cast<int>(cfg->get()), minimum, maximum); },
				[=](const ViewSettings& v) { *cfg = ClampSetting(v.*member, minimum, maximum); } });
		} else if constexpr (std::is_same_v<F, ColorField>) {
			auto cfg = std::make_shared<cfg_uint>(guid, kDefaults.*member);
			stored.push_back({ [cfg, member](ViewSettings& v) { v.*member = static_cast<COLORREF>(cfg->get()); },
				[cfg, member](const ViewSettings& v) { *cfg = static_cast<t_uint32>(v.*member); } });
		} else if constexpr (std::is_same_v<F, FontField>) {
			auto cfg = std::make_shared<cfg_struct_t<LOGFONTW>>(guid, kDefaults.*member);
			stored.push_back({ [cfg, member](ViewSettings& v) { v.*member = cfg->get(); }, [cfg, member](const ViewSettings& v) { *cfg = v.*member; } });
		} else if constexpr (std::is_same_v<F, OwnColorField>) {
			// -1 until saved once: UsesOwnTextColor then keeps a colour picked before 0.9.2.
			auto cfg = std::make_shared<cfg_int>(guid, -1);
			const auto color = field.color;
			stored.push_back({ [=](ViewSettings& v) { v.*member = UsesOwnTextColor(static_cast<int>(cfg->get()), v.*color); },
				[=](const ViewSettings& v) { *cfg = v.*member ? 1 : 0; } });
		} else {
			auto cfg = std::make_shared<cfg_int>(guid, static_cast<int>(kDefaults.*member));
			const auto from = field.from;
			stored.push_back({ [=](ViewSettings& v) { v.*member = from(static_cast<int>(cfg->get())); },
				[=](const ViewSettings& v) { *cfg = static_cast<int>(v.*member); } });
		}
	});
	return stored;
}

const std::vector<StoredSetting> g_storedSettings = MakeStoredSettings();

// Settings kept apart from ViewSettings. SettingGuid(17) was the scroll style of the test menu, which is gone,
// and 27 a switch of a 0.9.2 build that was never released: they stay unused, so their saved values are not
// read as another type.
cfg_bool cfg_durationCountsDown(SettingGuid(15), false);
const smooth_scroll::InertiaParams kDefaultInertia;
cfg_float cfg_inertiaTau(SettingGuid(18), kDefaultInertia.tauMs);
cfg_float cfg_inertiaDistance(SettingGuid(19), kDefaultInertia.distanceScale);
cfg_float cfg_inertiaQuickGap(SettingGuid(20), kDefaultInertia.quickGapMs);
cfg_float cfg_inertiaStep(SettingGuid(21), kDefaultInertia.accelerationStep);
cfg_float cfg_inertiaMax(SettingGuid(22), kDefaultInertia.maxAcceleration);
cfg_string cfg_scrollPresetName(SettingGuid(23), kDefaultScrollPresetName);
cfg_string cfg_userScrollPresets(SettingGuid(24), "");

// The numbers the panels scroll with: the saved ones until the Scrolling page changes them.
std::optional<smooth_scroll::InertiaParams> g_liveInertia;

} // namespace

ViewSettings DefaultViewSettings() {
	return kDefaults;
}

// In the order of the list, so a text colour is read before whether it is the user's own.
ViewSettings LoadViewSettings() {
	ViewSettings settings = kDefaults;
	for (const StoredSetting& setting : g_storedSettings) setting.load(settings);
	return settings;
}

void SaveViewSettings(const ViewSettings& settings) {
	for (const StoredSetting& setting : g_storedSettings) setting.save(settings);
}

smooth_scroll::InertiaParams LoadInertiaParams() {
	smooth_scroll::InertiaParams params;
	params.tauMs = cfg_inertiaTau;
	params.distanceScale = cfg_inertiaDistance;
	params.quickGapMs = cfg_inertiaQuickGap;
	params.accelerationStep = cfg_inertiaStep;
	params.maxAcceleration = cfg_inertiaMax;
	return ClampInertiaParams(params);
}

void SaveInertiaParams(const smooth_scroll::InertiaParams& params) {
	const smooth_scroll::InertiaParams clamped = ClampInertiaParams(params);
	cfg_inertiaTau = clamped.tauMs;
	cfg_inertiaDistance = clamped.distanceScale;
	cfg_inertiaQuickGap = clamped.quickGapMs;
	cfg_inertiaStep = clamped.accelerationStep;
	cfg_inertiaMax = clamped.maxAcceleration;
	g_liveInertia = clamped;
}

const smooth_scroll::InertiaParams& LiveInertiaParams() {
	if (!g_liveInertia) g_liveInertia = LoadInertiaParams();
	return *g_liveInertia;
}

void SetLiveInertiaParams(const smooth_scroll::InertiaParams& params) {
	g_liveInertia = ClampInertiaParams(params);
}

std::string LoadScrollPresetName() {
	return cfg_scrollPresetName.get().c_str();
}

void SaveScrollPresetName(const std::string& name) {
	cfg_scrollPresetName = name.c_str();
}

std::vector<ScrollPreset> LoadUserScrollPresets() {
	return ParseScrollPresets(cfg_userScrollPresets.get().c_str());
}

void SaveUserScrollPresets(const std::vector<ScrollPreset>& presets) {
	cfg_userScrollPresets = SerializeScrollPresets(presets).c_str();
}

bool GetDurationCountsDown() {
	return cfg_durationCountsDown;
}

void SetDurationCountsDown(bool countsDown) {
	cfg_durationCountsDown = countsDown;
}
