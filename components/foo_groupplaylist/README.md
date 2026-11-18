# GroupPlaylist

**English** · [한국어](README.ko.md) · Part of [fb2k-components](../../README.md)

A playlist view for foobar2000 64-bit. It groups the tracks of a playlist, for example by album, with the album art and an information column beside each group. Disc labels (`Disc 1`, `Disc 2`) can split a group further.

GroupPlaylist is a 64-bit alternative to SimPlaylist. It is a new program written from scratch and is not affiliated with the author of SimPlaylist.

- Groups and labels made from title formatting patterns, with ready-made presets (Album, Album + Disc, Artist / Cover / Album and others), or no grouping at all
- 22 built-in columns plus your own columns
- Star ratings you click, kept in the file tags or in Playback Statistics
- Follows the Default UI colours, fonts and dark mode
- English and Korean

## Requirements

- foobar2000 v2.0 or newer, **64-bit**. Tested with foobar2000 v2.26.3 on Windows 11, at 100% and 150% display scaling.
- Default User Interface. Columns UI is not supported.
- Optional: the official [Playback Statistics](https://www.foobar2000.org/components/view/foo_playcount) component (`foo_playcount`), to keep ratings in its database instead of the file tags.

## Install

1. Download `foo_groupplaylist-<version>.fb2k-component` from the [Releases](https://github.com/ChickenBrocolli/fb2k-components/releases) page.
2. In foobar2000, open **File > Preferences > Components**.
3. Click **Install...** and choose the downloaded file.
4. foobar2000 shows the license. Click **Accept**.
5. Click **Apply**, then **OK** to restart foobar2000.

**GroupPlaylist** then appears in the list of installed components.

To remove it, right-click it in the same list, choose **Remove**, and click **Apply**.

## Add the panel to your layout

1. Open **View > Layout > Enable layout editing mode**.
2. Right-click the panel that should become the playlist (for example the current playlist view) and choose **Replace UI Element...**. To add it next to other panels instead, right-click a splitter and choose **Add New UI Element...**.
3. Under **Playlist Views**, choose **GroupPlaylist**.
4. Open **View > Layout > Enable layout editing mode** again to leave editing mode.

## Using the panel

Right-click a **column title** to open the panel menu:

| Item | What it does |
|---|---|
| **Columns** | Tick the columns to show. **Edit...** opens the Columns preferences page. |
| **Groups** | Choose how this playlist is grouped. **None** shows a plain track list. **Sort playlist by this preset** sorts the playlist in the preset's order. **Edit...** opens the Groups preferences page. |
| **Smooth scrolling** | Scrolls smoothly instead of line by line. |
| **Pin group headers** | Keeps the header of the group at the top of the panel while you scroll through it. |

Other things you can do in the panel:

- Click an album cover to select the tracks under it. Double-click it to play them.
- Right-click an album cover to reload its art or open its folder.
- Double-click a group header to play the group.
- Drag a group header or a disc label to move the whole group or disc.
- Click a column title to sort the playlist by that column, and click again to reverse. Click the empty title above the covers to return to the preset's order. **Edit > Undo** (Ctrl+Z) undoes a sort.
- Hold **Ctrl** and turn the mouse wheel to make the album art larger or smaller.
- On the playing track, click the **Duration** cell to switch between the time played and the time left.

## Settings

Open **File > Preferences > Tools > GroupPlaylist**.

- **Fonts**: your own fonts for the tracks and for the group headers. Without them the panel uses the Default UI playlist font.
- **Colours and sizes**: colour of the group header text and of the labels, extra row height, album art size.
- **Rating**: where a click on the stars saves the rating: **File tags**, **Playback Statistics**, or **Both**. The last two need the separate Playback Statistics component (`foo_playcount`). Without it they are greyed out and marked "(needs Playback Statistics)".
- **Language**: **Automatic** follows the Windows display language (Korean on Korean Windows, English otherwise). **English** and **한국어** choose one. After **Apply** the panel and the page change at once. Reopen Preferences to see the page names in the new language.
- **Scrolling**: smooth scrolling and pinned group headers, the same switches as in the panel menu.
- **Behaviour**: remember the group preset and the scroll position of each playlist, dim text in brackets such as `(Remastered)`, sort by clicking a column title, and show the playing track in bold.

Two pages sit below it:

- **Columns**: the built-in columns (locked, but you can choose which appear in the menu or duplicate one to change the copy) and your own columns. A column's text is a [title formatting](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Title_Formatting_Reference) pattern. **Insert field** adds common fields, and the preview shows the result for the focused track.
- **Groups**: the presets. Each preset has a pattern for the group header, for the cover (tracks with the same value share one cover), for the labels inside a group (one per line), and for its sort order.

## Known limitations

- 64-bit foobar2000 only.
- foobar2000 draws at the display scaling of the main monitor. On a second monitor with a different scaling, Windows enlarges or shrinks the window, so it can look slightly blurred.

## License

GroupPlaylist is released under the [MIT License](../../LICENSE).

It is built with the foobar2000 SDK (Peter Pawlowski) and the Windows Template Library (Microsoft, WTL Team). Their licenses are in [THIRD-PARTY-NOTICES.txt](../../THIRD-PARTY-NOTICES.txt), which also ships inside the component file.

## Building from source

You need Visual Studio 2022 (or its Build Tools) with the C++ desktop workload.

1. Put the [foobar2000 SDK](https://www.foobar2000.org/SDK) in `foobar2000_SDK/` at the root of the repository.
2. Put the contents of the [WTL NuGet package](https://www.nuget.org/packages/wtl) (version 10) in `third_party/wtl/`.
3. From the root of the repository, run `tools\package.ps1` in PowerShell. It checks that every interface text has a Korean text, runs the unit tests, builds the Release DLL, and writes `dist\foo_groupplaylist-<version>.fb2k-component`.
