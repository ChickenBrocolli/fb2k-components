#pragma once

#include <helpers/DarkMode.h>

// "Syntax help" on the Columns and Groups pages: an owner-drawn button that looks like a link.
// A SysLink control draws its link in a fixed dark blue that cannot be read on the dark preferences background,
// and the dark mode hooks leave it alone. Here the page draws the text itself, in a blue that suits the mode.

// Call from the page's WM_DRAWITEM for the link button. dialog is the page.
inline void DrawLinkButton(const DRAWITEMSTRUCT& item, HWND dialog) {
	CDCHandle dc(item.hDC);
	CWindow button(item.hwndItem);
	// The page's background, as the dark mode hooks or Windows give it to static text.
	const HBRUSH background = reinterpret_cast<HBRUSH>(::SendMessage(dialog, WM_CTLCOLORSTATIC, reinterpret_cast<WPARAM>(item.hDC), reinterpret_cast<LPARAM>(item.hwndItem)));
	dc.FillRect(&item.rcItem, background ? background : GetSysColorBrush(COLOR_BTNFACE));

	LOGFONTW underlined = {};
	::GetObjectW(button.GetFont(), sizeof(underlined), &underlined);
	underlined.lfUnderline = TRUE;
	CFont font;
	font.CreateFontIndirect(&underlined);
	const HFONT oldFont = dc.SelectFont(font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(fb2k::isDarkMode() ? RGB(120, 180, 255) : GetSysColor(COLOR_HOTLIGHT));

	wchar_t text[64] = {};
	button.GetWindowText(text, static_cast<int>(std::size(text)));
	CRect textRect = item.rcItem;
	dc.DrawText(text, -1, textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
	if ((item.itemState & ODS_FOCUS) != 0 && (item.itemState & ODS_NOFOCUSRECT) == 0) {
		CRect measured = textRect;
		dc.DrawText(text, -1, measured, DT_SINGLELINE | DT_LEFT | DT_NOPREFIX | DT_CALCRECT);
		measured.OffsetRect(0, (textRect.Height() - measured.Height()) / 2);
		dc.DrawFocusRect(measured);
	}
	dc.SelectFont(oldFont);
}

// Call from the page's WM_SETCURSOR. Shows the hand over the link, as over a link in a browser.
inline bool SetLinkCursor(HWND cursorWindow, HWND linkButton) {
	if (cursorWindow != linkButton) return false;
	::SetCursor(::LoadCursor(nullptr, IDC_HAND));
	return true;
}
