#include "stdafx.h"

#include "column_def.h"
#include "label_fit.h"
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

std::string ColumnTitle(const ColumnDef& def) {
	if (!def.title.empty()) return def.title;
	for (const ColumnDef& builtIn : BuiltInColumnDefs()) {
		if (builtIn.name == def.name) return Tr(def.name.c_str());
	}
	return def.name;
}

std::string TrackCount(size_t tracks) {
	return TrackCountText(tracks, IsKoreanUi());
}

namespace {

// A static text or a check box or radio button is a label. One whose text is one line, starting at the left, can be
// narrowed to its text. Group boxes, push buttons, centred, right-aligned and wrapping texts keep their size.
void ClassifyLabel(HWND window, const RECT& place, bool& label, bool& fits, bool& checkBox) {
	label = false;
	fits = false;
	checkBox = false;
	wchar_t className[32] = {};
	::GetClassNameW(window, className, static_cast<int>(std::size(className)));
	const LONG style = ::GetWindowLongW(window, GWL_STYLE);
	if (_wcsicmp(className, L"Static") == 0) {
		label = true;
		const LONG type = style & SS_TYPEMASK;
		if (type != SS_LEFT && type != SS_LEFTNOWORDWRAP && type != SS_SIMPLE) return;
		// A static text two lines high or more wraps; its width is part of its layout.
		TEXTMETRICW metrics = {};
		HDC dc = ::GetDC(window);
		HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HGDIOBJ>(::SendMessageW(window, WM_GETFONT, 0, 0)));
		::GetTextMetricsW(dc, &metrics);
		::SelectObject(dc, old);
		::ReleaseDC(window, dc);
		fits = place.bottom - place.top < 2 * metrics.tmHeight;
		return;
	}
	if (_wcsicmp(className, L"Button") == 0) {
		const LONG type = style & BS_TYPEMASK;
		const bool box = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE ||
			type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
		label = box;
		const LONG alignment = style & (BS_LEFT | BS_RIGHT | BS_CENTER);
		if (!box || (style & (BS_PUSHLIKE | BS_MULTILINE | BS_LEFTTEXT)) != 0 || (alignment != 0 && alignment != BS_LEFT)) return;
		fits = true;
		checkBox = true;
	}
}

bool IsGroupBox(HWND window) {
	wchar_t className[32] = {};
	::GetClassNameW(window, className, static_cast<int>(std::size(className)));
	return _wcsicmp(className, L"Button") == 0 && (::GetWindowLongW(window, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX;
}

// The width the control's current text needs, with the check box square and the space after it.
int NeededWidth(HWND window, bool checkBox) {
	const int length = ::GetWindowTextLengthW(window);
	std::wstring text(static_cast<size_t>(length) + 1, L'\0');
	text.resize(static_cast<size_t>(::GetWindowTextW(window, text.data(), length + 1)));
	HDC dc = ::GetDC(window);
	HGDIOBJ old = ::SelectObject(dc, reinterpret_cast<HGDIOBJ>(::SendMessageW(window, WM_GETFONT, 0, 0)));
	RECT measured = {};
	UINT flags = DT_CALCRECT | DT_SINGLELINE;
	if (!checkBox && (::GetWindowLongW(window, GWL_STYLE) & SS_NOPREFIX) != 0) flags |= DT_NOPREFIX;
	::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &measured, flags);
	const int dpi = ::GetDeviceCaps(dc, LOGPIXELSX);
	::SelectObject(dc, old);
	::ReleaseDC(window, dc);
	int width = measured.right - measured.left + MulDiv(2, dpi, 96);
	// foobar2000 runs at the system scaling, which the system metrics follow.
	if (checkBox) width += ::GetSystemMetrics(SM_CXMENUCHECK) + MulDiv(6, dpi, 96);
	return width;
}

} // namespace

void DialogTexts::Translate(HWND dialog) {
	m_dialog = dialog;
	m_controls.clear();
	for (HWND child = ::GetWindow(dialog, GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT)) {
		if (IsGroupBox(child)) {
			// A group box frames several rows; it keeps its place, but its title is translated.
			Control frame = { child, {}, {}, false, false, false };
			const int length = ::GetWindowTextLengthW(child);
			frame.english.resize(static_cast<size_t>(length) + 1);
			frame.english.resize(static_cast<size_t>(::GetWindowTextW(child, frame.english.data(), length + 1)));
			if (!frame.english.empty()) m_controls.push_back(frame);
			continue;
		}
		Control control = { child, {}, {}, false, false, false };
		::GetWindowRect(child, &control.englishPlace);
		::MapWindowPoints(HWND_DESKTOP, dialog, reinterpret_cast<POINT*>(&control.englishPlace), 2);
		wchar_t className[32] = {};
		::GetClassNameW(child, className, static_cast<int>(std::size(className)));
		// An edit box or a combo box holds the user's text, not a label.
		const bool holdsUserText = _wcsicmp(className, L"Edit") == 0 || _wcsicmp(className, L"ComboBox") == 0;
		if (!holdsUserText) {
			const int length = ::GetWindowTextLengthW(child);
			if (length > 0) {
				control.english.resize(static_cast<size_t>(length) + 1);
				control.english.resize(static_cast<size_t>(::GetWindowTextW(child, control.english.data(), length + 1)));
			}
		}
		if (!control.english.empty()) ClassifyLabel(child, control.englishPlace, control.label, control.fits, control.checkBox);
		m_controls.push_back(control);
	}
	Retranslate();
}

void DialogTexts::Retranslate() {
	for (const Control& control : m_controls) {
		if (control.english.empty()) continue;
		const pfc::string8 key = pfc::stringcvt::string_utf8_from_wide(control.english.c_str()).get_ptr();
		::SetWindowTextW(control.window, TrW(key.c_str()).c_str());
	}
	FitLabels();
}

// Group boxes are left out: they frame rows and do not take part in them.
void DialogTexts::FitLabels() {
	if (!m_dialog) return;
	std::vector<FitBox> boxes;
	std::vector<size_t> controlOf;
	std::vector<FitLabel> labels;
	for (size_t i = 0; i < m_controls.size(); ++i) {
		const Control& control = m_controls[i];
		if (IsGroupBox(control.window)) continue;
		const RECT& place = control.englishPlace;
		// A label that cannot be narrowed keeps its width, and so holds its column of labels in place.
		if (control.label) {
			const int width = place.right - place.left;
			labels.push_back({ boxes.size(), control.fits ? NeededWidth(control.window, control.checkBox) : width });
		}
		boxes.push_back({ place.left, place.top, place.right, place.bottom });
		controlOf.push_back(i);
	}
	// "Closely after" is within about two characters of the dialog font.
	RECT gap = { 0, 0, 8, 0 };
	::MapDialogRect(m_dialog, &gap);
	const FitResult fit = ::FitLabels(boxes, labels, gap.right);
	for (size_t b = 0; b < boxes.size(); ++b) {
		const FitBox& box = boxes[b];
		::SetWindowPos(m_controls[controlOf[b]].window, nullptr, box.left - fit.leftShift[b], box.top, fit.width[b], box.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
	}
}
