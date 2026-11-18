#pragma once

#include <string>
#include <vector>

// Splits a cell's text so that bracketed parts, such as "(Remastered 2012)", can be drawn dimmer.
// Standard library only, so the unit tests build without the foobar2000 SDK.

struct TextPart {
	std::string text;
	bool dim;
};

// A part is dim from an opening bracket ( [ { up to and including its closing bracket. Brackets may nest.
// An opening bracket that is never closed dims the rest of the text. A closing bracket with no opening
// one is plain text.
inline std::vector<TextPart> SplitBracketParts(const std::string& text) {
	std::vector<TextPart> parts;
	int depth = 0;
	std::string current;
	bool currentDim = false;
	const auto flush = [&] {
		if (!current.empty()) parts.push_back({ current, currentDim });
		current.clear();
	};
	for (const char c : text) {
		const bool opens = c == '(' || c == '[' || c == '{';
		const bool closes = c == ')' || c == ']' || c == '}';
		if (opens && depth == 0) {
			flush();
			currentDim = true;
		}
		current += c;
		if (opens) ++depth;
		if (closes && depth > 0 && --depth == 0) {
			flush();
			currentDim = false;
		}
	}
	flush();
	return parts;
}

inline bool HasBracket(const std::string& text) {
	return text.find_first_of("([{") != std::string::npos;
}
