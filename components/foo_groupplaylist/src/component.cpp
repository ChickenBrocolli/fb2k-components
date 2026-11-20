#include "stdafx.h"

#include "ui_language.h"

namespace {

// The About text in Preferences > Components. The first line follows the component's language.
std::string AboutText() {
	std::string text = Tr("Playlist view with grouping and album art for foobar2000 64-bit.");
	text +=
		"\n\n"
		"Copyright (c) 2026 ChickenBrocolli. Released under the MIT License.\n"
		"\n"
		"Built with:\n"
		"- foobar2000 SDK, Copyright (c) Peter Pawlowski (BSD-style license; pfc and libPPUI under the zlib license)\n"
		"- Windows Template Library (WTL), Copyright (c) Microsoft Corporation, WTL Team (Microsoft Public License)\n"
		"- Smooth scrolling and DirectWrite text drawing adapted from ui_helpers, Copyright (c) Reupen Shah (BSD 3-Clause License)\n"
		"\n"
		"The full license texts are in LICENSE.txt and THIRD-PARTY-NOTICES.txt in the component package.";
	return text;
}

} // namespace

DECLARE_COMPONENT_VERSION("GroupPlaylist", "0.10.1", AboutText().c_str());

VALIDATE_COMPONENT_FILENAME("foo_groupplaylist.dll");

FOOBAR2000_IMPLEMENT_CFG_VAR_DOWNGRADE;
