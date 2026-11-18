#include "stdafx.h"

#include <helpers/atl-misc.h>
#include <helpers/DarkMode.h>
#include <helpers/CListControlFb2kColors.h>
#include <libPPUI/win32_op.h>

#include "playlist_view.h"
#include "preferences_pages.h"
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
	// A panel never configured has no data and keeps the defaults.
	void set_configuration(ui_element_config::ptr config) override {
		try {
			::ui_element_config_parser parser(config);
			t_uint32 version = 0;
			parser >> version;
			if (version < 1 || version > kConfigVersion) return;
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
			else contextmenu_manager::win32_run_menu_context_playlist(m_hWnd, &screenPoint);
		} else {
			SetMsgHandled(FALSE);
		}
	}

	// The menu of a cover. A right click elsewhere in the list shows the usual track menu.
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
			kSmoothScrollCommand = 5002, kPinHeadersCommand = 5003, kSortByPresetCommand = 5005, kNoGroupingCommand = 5006 };
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
			columns.AppendMenu(flags, kFirstColumnCommand + static_cast<UINT>(index), wide(ColumnTitle(defs[index].name)));
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

		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_POPUP, columns.Detach(), TrW("Columns").c_str());
		menu.AppendMenu(MF_POPUP, groups.Detach(), TrW("Groups").c_str());

		// Settings whose effect is best judged by trying them, so they switch here in one click.
		// Everything else is on the Preferences page.
		const ViewSettings settings = LoadViewSettings();
		menu.AppendMenu(MF_SEPARATOR);
		menu.AppendMenu(MF_STRING | (settings.smoothScroll ? MF_CHECKED : 0), kSmoothScrollCommand, TrW("Smooth scrolling").c_str());
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
		} else if (command == kSmoothScrollCommand || command == kPinHeadersCommand) {
			ViewSettings changed = settings;
			if (command == kSmoothScrollCommand) changed.smoothScroll = !changed.smoothScroll;
			else changed.pinHeaders = !changed.pinHeaders;
			ApplyViewSettings(changed);
			MenuTogglesChanged();
		} else if (command >= kFirstColumnCommand) {
			ToggleColumn(defs[command - kFirstColumnCommand].name);
		} else if (command >= kFirstPresetCommand) {
			ChoosePreset(command - kFirstPresetCommand);
		}
	}

	static constexpr t_uint32 kConfigVersion = 3;
};

class GroupViewElement : public ui_element_impl<GroupViewWindow> {};

service_factory_single_t<GroupViewElement> g_group_view_element_factory;

} // namespace
