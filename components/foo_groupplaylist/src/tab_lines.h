#pragma once

#include <string>
#include <vector>

// Text form of the lists kept in the foobar2000 configuration: one record per line, fields separated by tabs.
// Standard library only, so the unit tests build without the foobar2000 SDK.

// The fields separated by tabs. A tab or line break inside a field becomes a space.
inline std::string JoinFields(const std::vector<std::string>& fields) {
	std::string line;
	for (size_t index = 0; index < fields.size(); ++index) {
		if (index > 0) line += '\t';
		for (const char c : fields[index]) line += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
	}
	return line;
}

// One record: the fields and a line break.
inline std::string JoinFieldLine(const std::vector<std::string>& fields) {
	return JoinFields(fields) + '\n';
}

inline std::vector<std::vector<std::string>> SplitFieldLines(const std::string& text) {
	std::vector<std::vector<std::string>> lines;
	size_t lineStart = 0;
	while (lineStart < text.size()) {
		size_t lineEnd = text.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = text.size();
		const std::string line = text.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;

		std::vector<std::string> fields;
		size_t fieldStart = 0;
		for (;;) {
			const size_t tab = line.find('\t', fieldStart);
			fields.push_back(line.substr(fieldStart, tab == std::string::npos ? std::string::npos : tab - fieldStart));
			if (tab == std::string::npos) break;
			fieldStart = tab + 1;
		}
		lines.push_back(std::move(fields));
	}
	return lines;
}
