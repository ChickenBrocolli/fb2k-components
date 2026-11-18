#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

// The Korean texts of the user interface, looked up by their English text.
// The English text in the code and in foo_groupplaylist.rc is the key, so a text without an entry here shows in English.
// tools/check-ui-text.ps1 lists the English texts that have no entry. The unit tests check the table itself.
// Standard library only, so the unit tests build without the foobar2000 SDK. The file is UTF-8.

struct UiText {
	const char* english;
	const char* korean;
};

inline constexpr UiText kKoreanTexts[] = {
	// Main preferences page.
	{ "Fonts", "글꼴" },
	{ "Own font for tracks", "곡 줄 글꼴 직접 지정" },
	{ "Own font for group headers", "그룹 머리글 글꼴 직접 지정" },
	{ "Choose...", "선택..." },
	{ "Without an own font the panel uses the Default UI playlist font.", "직접 지정하지 않으면 Default UI의 재생목록 글꼴을 씁니다." },
	{ "Colours and sizes", "색과 크기" },
	{ "Group header text", "그룹 머리글 글자" },
	{ "Label text (Disc 1)", "라벨 글자 (Disc 1)" },
	{ "Extra row height", "줄 높이 추가" },
	{ "Album art size", "앨범아트 크기" },
	{ "Rating", "별점" },
	{ "Save ratings in", "별점 저장 위치" },
	{ "File tags", "파일 태그" },
	{ "Playback Statistics", "재생 통계" },
	{ "Both", "둘 다" },
	{ " (needs Playback Statistics)", " (재생 통계 컴포넌트 필요)" },
	{ "Language", "언어" },
	{ "Automatic", "자동" },
	{ "Scrolling", "스크롤" },
	{ "Smooth scrolling", "부드러운 스크롤" },
	{ "Pin group headers", "그룹 머리글 고정" },
	{ "Both also switch at once from the menu of a right-click on the column titles.", "두 가지 모두 열 제목을 오른쪽 클릭한 메뉴에서도 바로 켜고 끌 수 있습니다." },
	{ "Behaviour", "동작" },
	{ "Remember the group preset of each playlist", "재생목록마다 그룹 방식 기억" },
	{ "Dim text in brackets, such as (Remastered)", "괄호 안 글자를 흐리게 표시 (예: (Remastered))" },
	{ "Sort the playlist by clicking a column header", "열 제목을 클릭해 재생목록 정렬" },
	{ "Remember the scroll position of each playlist", "재생목록마다 스크롤 위치 기억" },
	{ "Bold the playing track", "재생 중인 곡을 굵게 표시" },
	{ ", bold", ", 굵게" },
	{ " pt", " pt" },

	// Columns page.
	{ "Columns", "열" },
	{ "Ticked columns are listed in the Columns menu of the panel header. Built-in columns are locked.", "체크한 열은 패널 열 제목 메뉴의 '열' 항목에 나옵니다. 기본 열은 고칠 수 없습니다." },
	{ "Add", "추가" },
	{ "Duplicate", "복제" },
	{ "Remove", "삭제" },
	{ "Move up", "위로" },
	{ "Move down", "아래로" },
	{ "Selected column", "선택한 열" },
	{ "Name", "이름" },
	{ "Alignment", "정렬" },
	{ "Pattern", "패턴" },
	{ "Insert field", "필드 넣기" },
	{ "Syntax help", "문법 도움말" },
	{ "Kind", "종류" },
	{ "Built-in", "기본" },
	{ "Custom", "사용자" },
	{ "Left", "왼쪽" },
	{ "Center", "가운데" },
	{ "Right", "오른쪽" },
	{ "(stars you click to rate)", "(클릭해서 매기는 별)" },
	{ "The main page sets where ratings are kept.", "별점 저장 위치는 첫 설정 페이지에서 정합니다." },
	{ "Built-in, locked. Duplicate it to change a copy.\n", "기본 열이라 고칠 수 없습니다. 복제해서 사본을 고치세요.\n" },
	{ "New column", "새 열" },
	{ " copy", " 사본" },
	{ "Every column needs a name. The columns were not saved.", "모든 열에 이름이 있어야 합니다. 열을 저장하지 않았습니다." },
	{ "This name belongs to a built-in column. Choose another name. The columns were not saved.", "기본 열의 이름입니다. 다른 이름을 고르세요. 열을 저장하지 않았습니다." },
	{ "Two columns have this name. Choose another name. The columns were not saved.", "이 이름을 쓰는 열이 둘 있습니다. 다른 이름을 고르세요. 열을 저장하지 않았습니다." },

	// Groups page.
	{ "Groups", "그룹" },
	{ "Ways to group a playlist. Ticked presets are listed in the Groups menu of the panel header.", "재생목록을 묶는 방식입니다. 체크한 프리셋은 패널 열 제목 메뉴의 '그룹' 항목에 나옵니다." },
	{ "Add copy", "복사해 추가" },
	{ "Selected preset", "선택한 프리셋" },
	{ "Group header", "그룹 머리글" },
	{ "Cover", "표지" },
	{ "Show the cover text next to the cover. A | starts a new line.", "표지 옆에 표지 글자 표시. | 는 줄바꿈입니다." },
	{ "Labels", "라벨" },
	{ "one per line", "한 줄에 하나" },
	{ "Hide a label that is the same for the whole group, such as Disc 1", "그룹 전체에서 같은 라벨은 숨기기 (예: Disc 1)" },
	{ "Sort by", "정렬 기준" },
	{ "Header: tracks with the same value share one header.", "머리글: 값이 같은 곡은 머리글 하나를 함께 씁니다." },
	{ "Cover: tracks with the same value share one cover.", "표지: 값이 같은 곡은 표지 하나를 함께 씁니다." },
	{ "Labels: a label row starts where the value changes.", "라벨: 값이 바뀌는 곳에서 라벨 줄이 시작됩니다." },
	{ "Sort: the order this preset sorts the playlist in.", "정렬: 이 프리셋으로 재생목록을 정렬할 때의 순서입니다." },
	{ "New preset", "새 프리셋" },
	{ "Every preset needs a name. The presets were not saved.", "모든 프리셋에 이름이 있어야 합니다. 프리셋을 저장하지 않았습니다." },
	{ "Two presets have this name. Choose another name. The presets were not saved.", "이 이름을 쓰는 프리셋이 둘 있습니다. 다른 이름을 고르세요. 프리셋을 저장하지 않았습니다." },

	// Insert field menu.
	{ "Tags", "태그" },
	{ "Audio", "오디오" },
	{ "File", "파일" },
	{ "Playback statistics", "재생 통계" },
	{ "Playlist", "재생목록" },
	{ "Syntax", "문법" },
	{ "ReplayGain", "ReplayGain" },
	{ "Track number", "트랙 번호" },
	{ "Disc number", "디스크 번호" },
	{ "Length", "길이" },
	{ "Bitrate (kbps)", "비트레이트 (kbps)" },
	{ "Sample rate (Hz)", "샘플레이트 (Hz)" },
	{ "Channels", "채널" },
	{ "Bits per sample", "샘플당 비트" },
	{ "File name with extension", "확장자를 포함한 파일 이름" },
	{ "Folder name", "폴더 이름" },
	{ "Full path", "전체 경로" },
	{ "File size", "파일 크기" },
	{ "Play count", "재생 횟수" },
	{ "Rating (number)", "별점 (숫자)" },
	{ "Rating (stars)", "별점 (별)" },
	{ "First played", "처음 재생한 날" },
	{ "Last played", "마지막으로 재생한 날" },
	{ "Added to library", "라이브러리에 추가한 날" },
	{ "Track peak", "트랙 피크" },
	{ "Album peak", "앨범 피크" },
	{ "Playing mark", "재생 표시" },
	{ "Position in playlist", "재생목록 안 위치" },
	{ "Hide when empty: [ ... ]", "비어 있으면 숨기기: [ ... ]" },
	{ "Line break in a cover text: |", "표지 글자 줄바꿈: |" },
	{ "Preview: ", "미리보기: " },
	{ "Preview: (the playlist is empty)", "미리보기: (재생목록이 비어 있음)" },
	{ "Preview: (the pattern has a syntax error)", "미리보기: (패턴에 문법 오류가 있음)" },
	{ "Preview: (empty for the focused track)", "미리보기: (선택한 곡에서는 비어 있음)" },

	// Built-in columns, also field names in the Insert field menu.
	{ "Playing", "재생" },
	{ "Track", "트랙" },
	{ "Title", "제목" },
	{ "Title / track artist", "제목 / 곡 아티스트" },
	{ "Artist", "아티스트" },
	{ "Album artist", "앨범 아티스트" },
	{ "Album", "앨범" },
	{ "Date", "날짜" },
	{ "Genre", "장르" },
	{ "Disc", "디스크" },
	{ "Composer", "작곡가" },
	{ "Comment", "코멘트" },
	{ "Duration", "길이" },
	{ "Codec", "코덱" },
	{ "Bitrate", "비트레이트" },
	{ "Sample rate", "샘플레이트" },
	{ "Track gain", "트랙 게인" },
	{ "Album gain", "앨범 게인" },
	{ "Played", "재생 횟수" },
	{ "File name", "파일 이름" },
	{ "File path", "파일 경로" },

	// Panel.
	{ "Playlist view with grouping and album art.", "곡을 앨범아트와 함께 묶어 보여 주는 재생목록 보기입니다." },
	{ "Reload album art", "앨범아트 다시 읽기" },
	{ "Open containing folder", "들어 있는 폴더 열기" },
	{ "Edit...", "편집..." },
	{ "None", "묶지 않음" },
	{ "Sort playlist by this preset", "이 프리셋으로 재생목록 정렬" },
	{ "The rating could not be saved in Playback Statistics, because that component (foo_playcount) is not installed.\nInstall it, or choose File tags under Rating on the GroupPlaylist preferences page.",
	  "재생 통계 컴포넌트(foo_playcount)가 설치되어 있지 않아 별점을 재생 통계에 저장하지 못했습니다.\n그 컴포넌트를 설치하거나, GroupPlaylist 설정 페이지에서 별점 저장 위치를 '파일 태그'로 바꾸세요." },

	// About text in Preferences > Components.
	{ "Playlist view with grouping and album art for foobar2000 64-bit.", "foobar2000 64비트용 재생목록 보기. 곡을 앨범아트와 함께 묶어 보여 줍니다." },
};

// The Korean text for this English text, or nullptr when the table has none.
inline const char* KoreanTextFor(std::string_view english) {
	static const std::unordered_map<std::string_view, const char*> table = [] {
		std::unordered_map<std::string_view, const char*> map;
		for (const UiText& text : kKoreanTexts) map.emplace(text.english, text.korean);
		return map;
	}();
	const auto found = table.find(english);
	return found == table.end() ? nullptr : found->second;
}

// "12 tracks" or "12곡".
inline std::string TrackCountText(size_t tracks, bool korean) {
	if (korean) return std::to_string(tracks) + "\xEA\xB3\xA1";
	return std::to_string(tracks) + (tracks == 1 ? " track" : " tracks");
}
