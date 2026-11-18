#include "stdafx.h"

#include "column_def.h"
#include "ui_language.h"
#include "ui_text.h"

namespace {

cfg_int cfg_uiLanguage(GUID{ 0xd638e72b, 0xa598, 0x476a, { 0x97, 0xb8, 0x7e, 0x49, 0xb0, 0x73, 0x3c, 0x34 } }, static_cast<int>(UiLanguage::Automatic));

bool WindowsIsKorean() {
	return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_KOREAN;
}

} // namespace

UiLanguage GetUiLanguage() {
	const int64_t value = cfg_uiLanguage;
	if (value < 0 || value > static_cast<int>(UiLanguage::Korean)) return UiLanguage::Automatic;
	return static_cast<UiLanguage>(value);
}

void SetUiLanguage(UiLanguage language) {
	cfg_uiLanguage = static_cast<int>(language);
}

bool IsKoreanUi() {
	const UiLanguage language = GetUiLanguage();
	return language == UiLanguage::Korean || (language == UiLanguage::Automatic && WindowsIsKorean());
}

const char* Tr(const char* english) {
	if (!IsKoreanUi()) return english;
	const char* korean = KoreanTextFor(english);
	return korean ? korean : english;
}

std::wstring TrW(const char* english) {
	return std::wstring(pfc::stringcvt::string_wide_from_utf8(Tr(english)).get_ptr());
}

std::string ColumnTitle(const std::string& name) {
	for (const ColumnDef& def : BuiltInColumnDefs()) {
		if (def.name == name) return Tr(name.c_str());
	}
	return name;
}

std::string TrackCount(size_t tracks) {
	return TrackCountText(tracks, IsKoreanUi());
}

void DialogTexts::Translate(HWND dialog) {
	m_english.clear();
	for (HWND child = ::GetWindow(dialog, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
		wchar_t className[32] = {};
		::GetClassNameW(child, className, static_cast<int>(std::size(className)));
		// An edit box or a combo box holds the user's text, not a label.
		if (_wcsicmp(className, L"Edit") == 0 || _wcsicmp(className, L"ComboBox") == 0) continue;
		const int length = ::GetWindowTextLengthW(child);
		if (length <= 0) continue;
		std::wstring text(static_cast<size_t>(length) + 1, L'\0');
		text.resize(static_cast<size_t>(::GetWindowTextW(child, text.data(), length + 1)));
		m_english.push_back({ child, text });
	}
	Retranslate();
}

void DialogTexts::Retranslate() {
	for (const auto& [window, english] : m_english) {
		const pfc::string8 key = pfc::stringcvt::string_utf8_from_wide(english.c_str()).get_ptr();
		::SetWindowTextW(window, TrW(key.c_str()).c_str());
	}
}
