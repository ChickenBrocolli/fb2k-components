# GroupPlaylist

**English** · [한국어](README.ko.md) · Part of [fb2k-components](../../README.md)

A playlist view for foobar2000 64-bit. It groups the tracks of a playlist, for example by album, with the album art and an information column beside each group. Disc labels (`Disc 1`, `Disc 2`) can split a group further.

![GroupPlaylist with the Album / Disc preset](screenshots/overview.png)

*Four albums in the Album / Disc preset: one folder with one artist; a main folder with Disc 1 and Disc 2 subfolders; an album whose bonus tracks have a higher sample rate, so they get a cover block of their own; and an album with three artists, one cover block each.*

GroupPlaylist is a 64-bit alternative to SimPlaylist. It is a new program written from scratch and is not affiliated with the author of SimPlaylist.

- Groups and labels made from title formatting patterns, with ready-made presets (Album, Album / Disc, Artist / Cover / Album and others), or no grouping at all
- 23 built-in columns plus your own columns
- Star ratings you click and a play count column, kept by the Playback Statistics component (ratings can go in the file tags too)
- Likes: a heart you click in the Like column
- Smooth scrolling that glides like a flicked list, with presets you can fine-tune
- Follows the Default UI colours, fonts and dark mode
- Colour emoji in titles and tags; album art in WebP and any other format Windows can open, with transparency kept
- Settings export and import, for a backup or another computer
- English and Korean

## Screenshots

| Dark mode | Artist / Cover / Album preset |
|---|---|
| ![Dark mode](screenshots/overview-dark.png) | ![Artist / Cover / Album preset: artist headers, album and disc labels](screenshots/artist-preset.png) |

| Panel menu (right-click a column title) | Preferences > Tools > GroupPlaylist |
|---|---|
| ![Panel menu with the Groups submenu](screenshots/header-menu.png) | ![Settings page](screenshots/settings.png) |

| Preferences > Tools > GroupPlaylist > Scrolling |
|---|
| ![Scrolling page: presets and the five numbers of the glide](screenshots/scrolling.png) |

The albums in the screenshots are made up: `tools\make-screenshot-tracks.ps1` writes them.

## Requirements

- foobar2000 v2.0 or newer, **64-bit**. Tested with foobar2000 v2.26.3 on Windows 11, at 100% and 150% display scaling.
- Default User Interface. Columns UI is not supported.

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
| **Columns** | Tick the columns to show. They are listed in the order set on the Columns preferences page. **Edit...** opens that page. |
| **Groups** | Choose how this playlist is grouped. **None** shows a plain track list. **Sort playlist by this preset** sorts the playlist in the preset's order. **Edit...** opens the Groups preferences page. |
| **Smooth scrolling** | The mouse wheel glides to a stop instead of jumping line by line. On by default. A precision touchpad scrolls as usual. Pick how it feels, **Default**, **Light**, **Heavy** or one of your own presets, and it takes effect at the next turn of the wheel; **Off** turns it off. Numbers you changed on the Scrolling page without saving them as a preset show as **Custom (unsaved)**; picking a preset leaves them behind. **Edit...** opens the Scrolling preferences page. |
| **Columns > Auto-size columns** | The columns fill the panel, so there is no horizontal scroll bar. They keep their proportions when the panel is resized, beside the cover and info columns, which keep their width. Dragging a column's edge moves only the border with the next column. Each panel has its own setting. It is on in a new panel; a panel set up with 0.9.0 keeps it off. |
| **Columns > Drag to reorder columns** | Drag column titles, the empty ones above the covers and the album text too, to put the columns in another order. Turn it off once you like the order, so a click or a resize cannot move a column by accident; widths can still be dragged. On by default. Also on the preferences page. |
| **Columns > Show column headers** | Untick it to hide the row of column titles for a plainer look. The columns keep their widths and order. This menu opens from the column titles, so to bring them back, tick **Show column headers** on the GroupPlaylist preferences page. |
| **Smooth scrolling > Pin group headers** | Keeps the header of the group at the top of the panel while you scroll through it. Also on the Scrolling page. |

Other things you can do in the panel:

- Click an album cover to select the tracks under it. Double-click it to play them.
- Right-click an album cover to reload its art or open its folder.
- Right-click the tracks for **Play**, **Remove**, **Crop**, **Sort**, **Cut**, **Copy** and **Paste**, followed by the usual foobar2000 commands. **Sort** changes the order of the selected tracks only: by a title formatting pattern you type, at random, reversed, or by album, artist, file path, title or track number. **Edit > Undo** (Ctrl+Z) undoes these changes.
- Double-click a group header to play the group.
- Drag a group header or a disc label to move the whole group or disc. A group goes only between groups, a disc only between discs.
- Click a column title to sort the playlist by that column, and click again to reverse. Click the empty title above the covers, or pick a preset under **Groups** right after such a sort, to return to the preset's order. **Edit > Undo** (Ctrl+Z) undoes a sort.
- Hold **Ctrl** and turn the mouse wheel to make the album art larger or smaller, or drag the right edge of the empty title above the covers.
- Drag the empty title above the covers, or the one above the album text beside them, to move that column anywhere among the other columns, for example the covers to the right end. Each panel keeps its own order.
- On the playing track, click the **Duration** cell to switch between the time played and the time left.

## Settings

Open **File > Preferences > Tools > GroupPlaylist**. A change on this page shows in the panels at once, so you can try it out. **Apply** or **OK** saves it, and **Cancel** puts the saved settings back. **Language** and **Text rendering** change only on **Apply**.

- **Fonts**: your own fonts for the tracks and for the group headers. Without them the panel uses the Default UI playlist font.
- **Colours and sizes**: the group header and label text follow foobar2000's highlight colour (Default UI colours) unless you tick **Own header colour** or **Own label colour** and pick one; extra row height, album art size.
- **Language**: **Automatic** follows the Windows display language (Korean on Korean Windows, English otherwise). **English** and **한국어** choose one. After **Apply** the panel and the page change at once. Reopen Preferences to see the page names in the new language.
- **Text rendering**: **Automatic (recommended)** draws text with DirectWrite, which shows colour emoji, and switches to GDI when foobar2000 runs under Wine (Linux, or macOS through CrossOver and the like), where DirectWrite can crash and leave the column titles blank. **DirectWrite** and **GDI** choose one. GDI shows emoji in black and white. After **Apply** the panels change at once.
- **Behaviour**: remember the group preset, the scroll position and the column layout of each playlist (the column layout is off by default; when on, a column change stays with the playlist it was made in, and playlists never changed keep the panel's columns), sort by clicking a column title, drag column titles to reorder the columns, show or hide the column titles, dim text in brackets such as `(Remastered)`, show the playing track in bold, and show the track count and length on group headers, on label rows or on both (it can also be turned off).
- **Settings file**: **Export settings...** saves every GroupPlaylist setting to a text file, and **Import settings...** loads one, for a backup or another computer. Per-playlist presets and scroll positions, ratings, and each panel's column layout are not in the file.

Four pages sit below it:

- **Columns**: the built-in columns and your own columns. A built-in column cannot be removed, but you can change its name, pattern and alignment, choose whether it appears in the menu, and put it back with **Restore default**. **Move up** and **Move down** put any column, built-in or your own, where you want it in the panel's Columns menu. The order of the columns a panel shows is set by dragging their titles. A column's text is a [title formatting](https://wiki.hydrogenaud.io/index.php?title=Foobar2000:Title_Formatting_Reference) pattern. **Insert field** adds common fields, and the preview shows the result for the focused track.
- **Ratings and likes**: where ratings are saved: **File tags** (the RATING tag), **Playback Statistics** (the default; the files are left alone; without that component ratings go to the file tags) or **Both**. And where likes are saved, a choice of their own because Playback Statistics has no likes: **GroupPlaylist store** (the default; the files are left alone), **File tags** (the LOVED tag) or **Both**. See [Ratings, likes and play history](#ratings-likes-and-play-history); both change on **Apply**. Filled stars take foobar2000's highlight colour, or a colour of your own with **Own star colour**. The hearts of the Like column are drawn as **Colour emoji** ❤️ (the default) or **In the star colour** (or a colour of your own with **Own heart colour**).
- **Groups**: the presets. Each preset has a pattern for the group header, for the cover (tracks with the same value share one cover), for the labels inside a group (one per line), and for its sort order.
- **Scrolling**: pinned group headers and smooth scrolling (the same switches as in the panel menu), and how the wheel glides: **Default**, **Light** (stops at once, hardly speeds up), **Heavy** (glides long, speeds up a lot when you spin the wheel), or your own sets, made from five numbers. A change shows at once while you scroll; **Apply** keeps it. Once you change a number, the preset list shows **Custom (unsaved)** and offers an unused name such as "Custom preset 1"; **Save as new** keeps the numbers as a preset of their own, and **Overwrite** saves them into your preset they started from.

## Ratings, likes and play history

Ratings and play history are the Playback Statistics component's (`foo_playcount`), as in foobar2000's other playlist views. GroupPlaylist shows them and sets ratings there, so every other component sees the same values. Likes, which Playback Statistics does not have, are kept by GroupPlaylist itself.

- The **Rating** column shows and sets the Playback Statistics rating. Without that component, ratings are read from and written to the files' RATING tags. To use the tags with Playback Statistics installed, choose **File tags** or **Both** on the **Ratings and likes** page. If you used an earlier GroupPlaylist that kept ratings in file tags, it goes on keeping them there.
- The **Played** column and the other play history fields need Playback Statistics. Without it they are empty.
- A like is a heart: click it in the **Like** column (shown in a new panel, and in the Columns menu). A like is separate from the rating.
- Likes are kept in GroupPlaylist's own store inside the foobar2000 profile, so the music files are left alone. With **File tags** or **Both** as the place for likes, they go in the files' LOVED tags. In the store, a track is known by its artist, album and title, so its like stays when the file is moved or renamed. Tracks with the same artist, album and title share it. A track without a title tag is known by its path.
- Likes already in the files (the LOVED tag) show until you change them in GroupPlaylist. After that the store's choice counts, even when you take the like away.
- GroupPlaylist 0.10.0 kept ratings and play counts in its own store. It no longer does: ratings given there do not show any more, and the fields `%gp_rating%`, `%gp_play_count%`, `%gp_first_played%`, `%gp_last_played%` and `%gp_added%` are gone. Use the Playback Statistics fields in their place.

The values can be used anywhere in foobar2000 (other columns, searches, autoplaylists, other components):

| Field | Value |
|---|---|
| `%rating%` | rating, 1 to 5 |
| `%play_count%` | number of plays |
| `%first_played%`, `%last_played%` | first and last played, as `YYYY-MM-DD hh:mm:ss` |
| `%added%` | when the track came into the library |
| `%gp_liked%` | 1 when the track is liked |
| `%gp_liked_date%` | when the track was liked; none for tracks liked before GroupPlaylist kept the date |

The first four rows are Playback Statistics fields; the last two are GroupPlaylist's.

To collect tracks by these values, make an autoplaylist in **Library > Search**: type a query, then choose **Create Autoplaylist** from the **...** button. Some queries:

| Autoplaylist | Query |
|---|---|
| Played more than 10 times | `%play_count% GREATER 10` |
| Played in the last year | `%last_played% DURING LAST 52 WEEKS` |
| Never played | `%play_count% MISSING` |
| Liked tracks | `%gp_liked% IS 1` (`LOVED IS 1` when likes are saved in file tags only) |
| Rated 4 stars or more | `%rating% GREATER 3` |

An autoplaylist updates when tracks change. A query on dates, such as the last year, is worked out again then or when foobar2000 starts, so a track can stay in it a little past the date until something changes.

## Known limitations

- 64-bit foobar2000 only.
- foobar2000 draws at the display scaling of the main monitor. On a second monitor with a different scaling, Windows enlarges or shrinks the window, so it can look slightly blurred.

## License

GroupPlaylist is released under the [MIT License](../../LICENSE).

It is built with the foobar2000 SDK (Peter Pawlowski) and the Windows Template Library (Microsoft, WTL Team), and its smooth scrolling and its DirectWrite text drawing (with colour emoji) are adapted from [ui_helpers](https://github.com/reupen/ui_helpers) (Reupen Shah, BSD 3-Clause). Their licenses are in [THIRD-PARTY-NOTICES.txt](../../THIRD-PARTY-NOTICES.txt), which also ships inside the component file.

## Building from source

You need Visual Studio 2022 (or its Build Tools) with the C++ desktop workload.

1. Put the [foobar2000 SDK](https://www.foobar2000.org/SDK) in `foobar2000_SDK/` at the root of the repository.
2. Put the contents of the [WTL NuGet package](https://www.nuget.org/packages/wtl) (version 10) in `third_party/wtl/`.
3. From the root of the repository, run `tools\package.ps1` in PowerShell. It checks that every interface text has a Korean text, runs the unit tests, builds the Release DLL, and writes `dist\foo_groupplaylist-<version>.fb2k-component`.
