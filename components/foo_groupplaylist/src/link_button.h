#pragma once

#include <helpers/DarkMode.h>
#include <libPPUI/DarkMode-CHyperLink.h>

// "Syntax help" on the Columns and Groups pages: libPPUI's link control, which draws in a link colour that suits
// dark mode and shows the hand. A click comes to the page as WM_COMMAND with the control's ID.
// Call after the page's texts are translated: the control keeps the text it has when attached.
inline void AttachSyntaxLink(DarkMode::CHyperLink& link, HWND control, fb2k::CDarkModeHooks& dark) {
	link.SetHyperLinkExtendedStyle(HLINK_COMMANDBUTTON | HLINK_NOTOOLTIP);
	link.SubclassWindow(control);
	dark.AddCtrlMsg(link);
}
