#pragma once

#include <string>

// The component's pages under Preferences > Tools. The main page holds the look and behaviour settings.
// Columns and Groups sit below it and edit the custom columns and the group presets.
// Scrolling sits below it too: header pinning, smooth scrolling and its presets (scrolling_page.cpp).
inline constexpr GUID guid_preferences_page = { 0xe49aca5c, 0xa4ad, 0x4597, { 0x9d, 0x60, 0x59, 0x34, 0xa8, 0x85, 0x0f, 0x37 } };
inline constexpr GUID guid_columns_page = { 0xa22e7759, 0xf596, 0x4ac3, { 0xa4, 0x25, 0x33, 0x98, 0xaf, 0x30, 0xc7, 0x6a } };
inline constexpr GUID guid_groups_page = { 0xdef43071, 0x4e72, 0x48e6, { 0xa3, 0x4c, 0x3b, 0x39, 0x12, 0xe9, 0x37, 0x8b } };
inline constexpr GUID guid_scrolling_page = { 0x96d228d5, 0x35af, 0x4af8, { 0xaa, 0xdd, 0x4c, 0xf6, 0x95, 0x76, 0x64, 0xd7 } };

// The panel header menu switched smooth scrolling, header pinning or the scroll preset. An open Scrolling page shows
// the new state.
void MenuTogglesChanged();

// A panel saved a setting that the main page also shows (the cover size, column dragging). An open main page takes it.
void PanelSettingsChanged();

// Settings were imported (settings_transfer.h). A page that foobar2000 keeps from earlier in this Preferences
// window drops its edits and shows the imported settings, so Apply or OK there cannot save the old ones again.
void ReloadColumnsPage();
void ReloadGroupsPage();
void ReloadScrollingPage();

// Opens Preferences at the Columns page.
void ShowColumnsPage();

// Opens Preferences at the Scrolling page.
void ShowScrollingPage();

// Opens Preferences at the Groups page with this preset selected, usually the one the panel shows.
void ShowGroupsPage(const std::string& selectPreset);
