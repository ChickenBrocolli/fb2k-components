#pragma once

#include <string>

// The component's pages under Preferences > Tools. The main page holds the look and behaviour settings.
// Columns and Groups sit below it and edit the custom columns and the group presets.
inline constexpr GUID guid_preferences_page = { 0xe49aca5c, 0xa4ad, 0x4597, { 0x9d, 0x60, 0x59, 0x34, 0xa8, 0x85, 0x0f, 0x37 } };
inline constexpr GUID guid_columns_page = { 0xa22e7759, 0xf596, 0x4ac3, { 0xa4, 0x25, 0x33, 0x98, 0xaf, 0x30, 0xc7, 0x6a } };
inline constexpr GUID guid_groups_page = { 0xdef43071, 0x4e72, 0x48e6, { 0xa3, 0x4c, 0x3b, 0x39, 0x12, 0xe9, 0x37, 0x8b } };

// The panel header menu switched smooth scrolling or header pinning. An open main page shows the new state.
void MenuTogglesChanged();

// Opens Preferences at the Columns page.
void ShowColumnsPage();

// Opens Preferences at the Groups page with this preset selected, usually the one the panel shows.
void ShowGroupsPage(const std::string& selectPreset);
