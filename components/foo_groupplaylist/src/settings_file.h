#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

// The text of an exported settings file, which Notepad can open: one "key = value" per line, UTF-8.
// Lines starting with # are comments. A list is a count line ("customColumns = 2") followed by one line per item
// ("column = ..."), so an empty list can be told from a list the file does not have.
// Standard library only, so the unit tests build without the foobar2000 SDK.

inline constexpr char kSettingsFormatKey[] = "format";
inline constexpr char kSettingsFormatName[] = "GroupPlaylist settings";
inline constexpr int kSettingsFileVersion = 1;

using SettingsEntries = std::vector<std::pair<std::string, std::string>>;

class SettingsWriter {
public:
	SettingsWriter() {
		Add(kSettingsFormatKey, kSettingsFormatName);
		Add("version", std::to_string(kSettingsFileVersion));
	}

	void Comment(const std::string& text) { m_text += "\n# " + text + "\n"; }

	// A line break in a value would end the line, so it becomes a space. Tabs are kept.
	void Add(const std::string& key, const std::string& value) {
		m_text += key + " = ";
		for (const char c : value) m_text += (c == '\n' || c == '\r') ? ' ' : c;
		m_text += '\n';
	}

	void AddBool(const std::string& key, bool value) { Add(key, value ? "1" : "0"); }
	void AddInt(const std::string& key, long long value) { Add(key, std::to_string(value)); }

	void AddList(const std::string& countKey, const std::string& itemKey, const std::vector<std::string>& items) {
		AddInt(countKey, static_cast<long long>(items.size()));
		for (const std::string& item : items) Add(itemKey, item);
	}

	const std::string& Text() const { return m_text; }

private:
	std::string m_text;
};

// The entries of a settings file in their order, or nothing when the text is not one (no "format" line naming it).
inline std::optional<SettingsEntries> ParseSettingsText(std::string text) {
	if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);  // a byte order mark from Notepad
	SettingsEntries entries;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos) end = text.size();
		std::string line = text.substr(start, end - start);
		start = end + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		const size_t first = line.find_first_not_of(" \t");
		if (first == std::string::npos || line[first] == '#') continue;
		const size_t equals = line.find('=');
		if (equals == std::string::npos) continue;
		std::string key = line.substr(first, equals - first);
		while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
		std::string value = line.substr(equals + 1);
		if (!value.empty() && value.front() == ' ') value.erase(0, 1);
		entries.emplace_back(std::move(key), std::move(value));
	}
	for (const auto& [key, value] : entries) {
		if (key == kSettingsFormatKey && value == kSettingsFormatName) return entries;
	}
	return std::nullopt;
}

// The value of the first line with this key.
inline std::optional<std::string> SettingValue(const SettingsEntries& entries, const std::string& key) {
	for (const auto& [entryKey, value] : entries) {
		if (entryKey == key) return value;
	}
	return std::nullopt;
}

// The items of a list, or nothing when the file has no count line for it. Only the items after the count line count.
inline std::optional<std::vector<std::string>> SettingList(const SettingsEntries& entries, const std::string& countKey, const std::string& itemKey) {
	std::optional<std::vector<std::string>> items;
	for (const auto& [key, value] : entries) {
		if (key == countKey) items.emplace();
		else if (key == itemKey && items) items->push_back(value);
	}
	return items;
}

inline std::optional<bool> ParseSettingBool(const std::string& text) {
	if (text == "1" || text == "true") return true;
	if (text == "0" || text == "false") return false;
	return std::nullopt;
}

inline std::optional<long long> ParseSettingInt(const std::string& text) {
	if (text.empty()) return std::nullopt;
	size_t index = text[0] == '-' ? 1 : 0;
	if (index == text.size()) return std::nullopt;
	long long value = 0;
	for (; index < text.size(); ++index) {
		if (text[index] < '0' || text[index] > '9' || value > 100000000000LL) return std::nullopt;
		value = value * 10 + (text[index] - '0');
	}
	return text[0] == '-' ? -value : value;
}

// Colours are written as #RRGGBB. The returned value is red in the low byte, as Windows COLORREF.
inline std::string FormatSettingColor(unsigned long color) {
	static const char digits[] = "0123456789ABCDEF";
	const unsigned parts[] = { color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF };
	std::string out = "#";
	for (const unsigned part : parts) {
		out += digits[part >> 4];
		out += digits[part & 0xF];
	}
	return out;
}

inline std::optional<unsigned long> ParseSettingColor(const std::string& text) {
	if (text.size() != 7 || text[0] != '#') return std::nullopt;
	unsigned long rgb = 0;
	for (size_t index = 1; index < 7; ++index) {
		const char c = text[index];
		const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
		if (digit < 0) return std::nullopt;
		rgb = rgb * 16 + static_cast<unsigned long>(digit);
	}
	return ((rgb >> 16) & 0xFF) | (rgb & 0xFF00) | ((rgb & 0xFF) << 16);
}
