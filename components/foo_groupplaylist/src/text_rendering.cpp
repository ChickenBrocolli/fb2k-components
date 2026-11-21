#include "stdafx.h"

#include "text_rendering.h"

namespace {

cfg_int cfg_textRendering(GUID{ 0x5b2e8f41, 0x7c3a, 0x4d19, { 0x9e, 0x6b, 0x21, 0xa4, 0xd8, 0x0f, 0x63, 0xc7 } }, static_cast<int>(TextRendering::Automatic));

} // namespace

TextRendering GetTextRendering() {
	const int64_t value = cfg_textRendering;
	if (value < 0 || value > static_cast<int>(TextRendering::Gdi)) return TextRendering::Automatic;
	return static_cast<TextRendering>(value);
}

void SetTextRendering(TextRendering rendering) {
	cfg_textRendering = static_cast<int>(rendering);
}

// Wine's ntdll has a function of its own that Windows' does not: the usual way to tell Wine apart.
bool RunningOnWine() {
	static const bool wine = [] {
		const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
	}();
	return wine;
}

bool UseDirectWrite() {
	switch (GetTextRendering()) {
	case TextRendering::DirectWrite: return true;
	case TextRendering::Gdi: return false;
	default: return !RunningOnWine();
	}
}
