#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <helpers/CListControlFb2kColors.h>
#include <libPPUI/win32_op.h>

#include "playlist_view.h"
#include "preferences_pages.h"
#include "track_menu.h"
#include "ui_language.h"

namespace {

constexpr GUID guid_group_view_element = { 0x64e512d6, 0xdb5a, 0x4d3e, { 0x89, 0x34, 0x5f, 0xc7, 0x6f, 0x2e, 0x12, 0xa4 } };

typedef CListControlFb2kColorsUIElem<PlaylistView> ThemedPlaylistView;

class GroupViewWindow : public ui_element_instance, public ThemedPlaylistView {
public:
	GroupViewWindow(ui_element_config::ptr config, ui_element_instance_callback_ptr callback)
		: ThemedPlaylistView(callback) {
		set_configuration(config);
	}

	void initialize_window(HWND parent) {
		WIN32_OP(Create(parent) != NULL);
		ApplyHostFont();
		AttachToPlaylist();
	}

	BEGIN_MSG_MAP_EX(GroupViewWindow)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		CHAIN_MSG_MAP(ThemedPlaylistView)
	END_MSG_MAP()

	// The saved settings of one panel. Version 1 holds the name of the active group preset.
	// Version 2 adds the column layout. Version 3 adds whether the panel shows no groups ("None").
	// Version 4 adds whether the columns fill the panel ("Auto-size columns").
	// Version 5 adds where the cover and info columns sit, which the user can drag them to.
	// A panel never configured has no data and keeps the defaults, so a new panel fits its columns to its width.
	// A panel saved before version 4 keeps the widths its user set, so it starts with that off.
	void set_configuration(ui_element_config::ptr config) override {
		try {
			::ui_element_config_parser parser(config);
			t_uint32 version = 0;
			parser >> version;
			if (version < 1 || version > kConfigVersion) return;
			if (version < 4) SetAutoSizeColumns(false);
			pfc::string8 presetName;
			parser >> presetName;
			SelectPresetByName(presetName);
			if (version < 2) return;

			ColumnLayout layout;
			t_uint32 columnCount = 0;
			parser >> layout.coverWidth >> layout.infoWidth >> columnCount;
			for (t_uint32 index = 0; index < columnCount; ++index) {
				pfc::string8 name;
				float width = 0;
				parser >> name >> width;
				layout.columns.push_back({ name.c_str(), width });
			}
			SetColumnLayout(layout);
			if (version < 3) return;

			t_uint8 ungrouped = 0;
			parser >> ungrouped;
			if (ungrouped != 0) SetUngrouped();
			if (version < 4) return;

			t_uint8 autoSize = 0;
			parser >> autoSize;
			SetAutoSizeColumns(autoSize != 0);
			if (version < 5) return;

			t_uint32 coverPlace = 0, infoPlace = 1;
			parser >> coverPlace >> infoPlace;
			ColumnLayout placed = GetColumnLayout();
			placed.places = { coverPlace, infoPlace };
			SetColumnLayout(placed);
		} catch (exception_io_data const&) {
		}
	}

	// write_string is spelled out because "builder << const char*" picks the bool overload
	// and stores one byte in place of the text.
	ui_element_config::ptr get_configuration() override {
		const ColumnLayout layout = GetColumnLayout();
		ui_element_config_builder builder;
		builder << kConfigVersion;
		builder.write_string(Presets()[ActivePreset()].name.c_str());
		builder << layout.coverWidth << layout.infoWidth << static_cast<t_uint32>(layout.columns.size());
		for (const ColumnLayout::Column& column : layout.columns) {
			builder.write_string(column.name.c_str());
			builder << column.width;
		}
		builder << static_cast<t_uint8>(IsUngrouped() ? 1 : 0);
		builder << static_cast<t_uint8>(IsAutoSizeColumns() ? 1 : 0);
		builder << static_cast<t_uint32>(layout.places.cover) << static_cast<t_uint32>(layout.places.info);
		return builder.finish(g_get_guid());
	}

	void notify(const GUID& what, t_size, const void*, t_size) override {
		if (what == ui_element_notify_colors_changed) {
			ui_colors_changed();
		} else if (what == ui_element_notify_font_changed) {
			ApplyHostFont();
		}
	}

	static GUID g_get_guid() { return guid_group_view_element; }
	static GUID g_get_subclass() { return ui_element_subclass_playlist_renderers; }
	static void g_get_name(pfc::string_base& out) { out = "GroupPlaylist"; }
	static const char* g_get_description() { return Tr("Playlist view with grouping and album art."); }
	static ui_element_config::ptr g_get_default_configuration() {
		return ui_element_config::g_create_empty(g_get_guid());
	}

private:
	void ApplyHostFont() { SetHostFont(m_callback->query_font_ex(ui_font_playlists)); }

	// In layout editing mode the host owns the right-click menu, so the message goes up unhandled.
	void OnContextMenu(CWindow clicked, CPoint screenPoint) {
		if (m_callback->is_edit_mode_enabled()) {
			SetMsgHandled(FALSE);
		} else if (clicked == GetHeaderCtrl()) {
			RunHeaderMenu(screenPoint);
		} else if (clicked == m_hWnd && GetContextMenuPoint2(screenPoint)) {
			CPoint clientPoint = screenPoint;
			ScreenToClient(&clientPoint);
			if (const std::optional<size_t> block = CoverBlockAt(clientPoint)) RunCoverMenu(screenPoint, *block);
			else RunTrackMenuAt(screenPoint);
		} else {
			SetMsgHandled(FALSE);
		}
	}

	// A right click on a group header focuses the group, not a track, so Play starts the group as a double click does.
	t_size ItemToPlay() const {
		const size_t group = GetGroupFocus2();
		return group != SIZE_MAX ? group : GetFocusItem();
	}

	void RunTrackMenuAt(CPoint screenPoint) {
		const t_size play = ItemToPlay();
		switch (RunTrackMenu(m_hWnd, screenPoint, play < GetItemCount())) {
		case TrackMenuCommand::Play: if (play < GetItemCount()) ExecuteDefaultAction(play); break;
		case TrackMenuCommand::Cut: CopySelection(); RequestRemoveSelection(); break;
		case TrackMenuCommand::Copy: CopySelection(); break;
		case TrackMenuCommand::Paste: PasteClipboard(); break;
		case TrackMenuCommand::None: break;
		}
	}

	// The menu of a cover. A right click elsewhere in the list shows the track menu.
	void RunCoverMenu(CPoint screenPoint, size_t block) {
		enum : UINT { kReloadCommand = 1, kOpenFolderCommand };
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, kReloadCommand, TrW("Reload album art").c_str());
		menu.AppendMenu(MF_STRING, kOpenFolderCommand, TrW("Open containing folder").c_str());
		const UINT command = static_cast<UINT>(menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, m_hWnd));
		if (command == kReloadCommand) ReloadCover(block);
		else if (command == kOpenFolderCommand) OpenCoverFolder(block);
	}

	void RunHeaderMenu(CPoint screenPoint) {
		enum : UINT { kFirstPresetCommand = 1, kFirstColumnCommand = 1000, kCustomColumnsCommand = 5000, kEditPresetsCommand = 5001,
			kSmoothScrollOffCommand = 5002, kPinHeadersCommand = 5003, kSortByPresetCommand = 5005, kNoGroupingCommand = 5006, kAutoSizeCommand = 5007,
			kEditScrollingCommand = 5008, kCustomScrollCommand = 5009, kDragColumnsCommand = 5010,
			kFirstScrollPresetCommand = 6000 };
		const auto wide = [](const std::string& text) { return pfc::stringcvt::string_os_from_utf8(text.c_str()); };

		// ColumnDefs() lists the built-in groups in order and then the custom columns.
		// A line goes above the first column of every group after the first, and above the custom columns.
		std::vector<size_t> lineAbove;
		size_t groupEnd = 0;
		for (const std::vector<ColumnDef>& group : BuiltInColumnGroups()) {
			groupEnd += group.size();
			lineAbove.push_back(groupEnd);
		}

		CMenu columns;
		columns.CreatePopupMenu();
		const std::vector<ColumnDef>& defs = ColumnDefs();
		// A line waits until the next listed column, so a group whose columns are all left out leaves no extra line.
		bool lineWaiting = false;
		bool anyListed = false;
		for (size_t index = 0; index < defs.size(); ++index) {
			if (std::find(lineAbove.begin(), lineAbove.end(), index) != lineAbove.end()) lineWaiting = true;
			// A column unticked on the Columns page stays listed while this panel shows it, so it can be hidden again.
			const bool shown = IsColumnShown(defs[index].name);
			if (!defs[index].showInMenu && !shown) continue;
			if (lineWaiting && anyListed) columns.AppendMenu(MF_SEPARATOR);
			lineWaiting = false;
			anyListed = true;
			const UINT flags = MF_STRING | (shown ? MF_CHECKED : 0);
			columns.AppendMenu(flags, kFirstColumnCommand + static_cast<UINT>(index), wide(ColumnTitle(defs[index])));
		}
		if (anyListed) columns.AppendMenu(MF_SEPARATOR);
		columns.AppendMenu(MF_STRING, kCustomColumnsCommand, TrW("Edit...").c_str());

		CMenu groups;
		groups.CreatePopupMenu();
		// "None" shows the playlist as a plain list, as the original's first Groups item does.
		groups.AppendMenu(MF_STRING | (IsUngrouped() ? MF_CHECKED : 0), kNoGroupingCommand, TrW("None").c_str());
		groups.AppendMenu(MF_SEPARATOR);
		for (size_t index = 0; index < Presets().size(); ++index) {
			// A preset unticked on the Groups page stays listed while this panel uses it.
			const bool active = !IsUngrouped() && index == ActivePreset();
			if (!Presets()[index].showInMenu && !active) continue;
			const UINT flags = MF_STRING | (active ? MF_CHECKED : 0);
			groups.AppendMenu(flags, kFirstPresetCommand + static_cast<UINT>(index), wide(Presets()[index].name));
		}
		groups.AppendMenu(MF_SEPARATOR);
		groups.AppendMenu(MF_STRING | (IsUngrouped() ? MF_GRAYED : 0), kSortByPresetCommand, TrW("Sort playlist by this preset").c_str());
		groups.AppendMenu(MF_STRING, kEditPresetsCommand, TrW("Edit...").c_str());

		// The feel of smooth scrolling is best judged by trying it, so its presets switch here, next to Off.
		// Numbers changed on the Scrolling page belong to no preset: they are listed last, as "Custom (unsaved)",
		// so it shows that picking a preset leaves them behind.
		const ViewSettings settings = LoadViewSettings();
		const std::vector<ScrollPreset> scrollPresets = AllScrollPresets(LoadUserScrollPresets());
		const bool customScroll = IsCustomScroll(scrollPresets, LoadScrollPresetName(), LoadInertiaParams());
		const std::optional<size_t> scrollPreset = settings.smoothScroll && !customScroll ? FindScrollPreset(scrollPresets, LoadScrollPresetName()) : std::nullopt;
		const size_t builtInCount = BuiltInScrollPresets().size();
		CMenu scrolling;
		scrolling.CreatePopupMenu();
		scrolling.AppendMenu(MF_STRING | (settings.smoothScroll ? 0 : MF_CHECKED), kSmoothScrollOffCommand, TrW("Off").c_str());
		scrolling.AppendMenu(MF_SEPARATOR);
		for (size_t index = 0; index < scrollPresets.size(); ++index) {
			if (index == builtInCount) scrolling.AppendMenu(MF_SEPARATOR);
			const ScrollPreset& preset = scrollPresets[index];
			const std::string name = index < builtInCount ? Tr(preset.name.c_str()) : preset.name;
			scrolling.AppendMenu(MF_STRING | (scrollPreset == index ? MF_CHECKED : 0), kFirstScrollPresetCommand + static_cast<UINT>(index), wide(name));
		}
		if (customScroll) {
			scrolling.AppendMenu(MF_SEPARATOR);
			scrolling.AppendMenu(MF_STRING | (settings.smoothScroll ? MF_CHECKED : 0), kCustomScrollCommand, TrW("Custom (unsaved)").c_str());
		}
		scrolling.AppendMenu(MF_SEPARATOR);
		scrolling.AppendMenu(MF_STRING, kEditScrollingCommand, TrW("Edit...").c_str());

		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_POPUP, columns.Detach(), TrW("Columns").c_str());
		menu.AppendMenu(MF_POPUP, groups.Detach(), TrW("Groups").c_str());
		menu.AppendMenu(MF_POPUP, scrolling.Detach(), TrW("Smooth scrolling").c_str());

		// Switches that take one click. Everything else is on the Preferences page.
		// Auto-size columns belongs to this panel, like its columns, as in the Default UI playlist's header menu.
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING | (IsAutoSizeColumns() ? MF_CHECKED : 0), kAutoSizeCommand, TrW("Auto-size columns").c_str());
		menu.AppendMenu(MF_STRING | (settings.dragColumns ? MF_CHECKED : 0), kDragColumnsCommand, TrW("Drag to reorder columns").c_str());
		menu.AppendMenu(MF_STRING | (settings.pinHeaders ? MF_CHECKED : 0), kPinHeadersCommand, TrW("Pin group headers").c_str());
		const UINT command = static_cast<UINT>(menu.TrackPopupMenu(TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenPoint.x, screenPoint.y, m_hWnd));
		if (command == kCustomColumnsCommand) {
			ShowColumnsPage();
		} else if (command == kSortByPresetCommand) {
			SortByActivePreset();
		} else if (command == kEditPresetsCommand) {
			ShowGroupsPage(Presets()[ActivePreset()].name);
		} else if (command == kNoGroupingCommand) {
			ChooseUngrouped();
		} else if (command == kAutoSizeCommand) {
			SetAutoSizeColumns(!IsAutoSizeColumns());
		} else if (command == kEditScrollingCommand) {
			ShowScrollingPage();
		} else if (command == kCustomScrollCommand) {
			// The custom numbers stay; only smooth scrolling comes back on, if it was off.
			if (!settings.smoothScroll) {
				ViewSettings changed = settings;
				changed.smoothScroll = true;
				ApplyViewSettings(changed);
			}
		} else if (command == kSmoothScrollOffCommand || command == kPinHeadersCommand || command == kDragColumnsCommand) {
			ViewSettings changed = settings;
			if (command == kSmoothScrollOffCommand) changed.smoothScroll = false;
			else if (command == kPinHeadersCommand) changed.pinHeaders = !changed.pinHeaders;
			else changed.dragColumns = !changed.dragColumns;
			ApplyViewSettings(changed);
		} else if (command >= kFirstScrollPresetCommand) {
			PickScrollPreset(scrollPresets[command - kFirstScrollPresetCommand], settings);
		} else if (command >= kFirstColumnCommand) {
			ToggleColumn(defs[command - kFirstColumnCommand].name);
		} else if (command >= kFirstPresetCommand) {
			ChoosePreset(command - kFirstPresetCommand);
		}
	}

	// A preset picked in the header menu is saved and used at once, as one picked on the Scrolling page and applied.
	// Picking one also turns smooth scrolling on.
	static void PickScrollPreset(const ScrollPreset& preset, const ViewSettings& settings) {
		SaveInertiaParams(preset.params);
		SaveScrollPresetName(preset.name);
		SetLiveInertiaParams(preset.params);
		if (!settings.smoothScroll) {
			ViewSettings changed = settings;
			changed.smoothScroll = true;
			ApplyViewSettings(changed);
		} else {
			PanelSettingsChanged();
		}
	}

	static constexpr t_uint32 kConfigVersion = 5;
};

class GroupViewElement : public ui_element_impl<GroupViewWindow> {};

service_factory_single_t<GroupViewElement> g_group_view_element_factory;

} // namespace
