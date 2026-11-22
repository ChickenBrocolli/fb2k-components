#pragma once

#include <unordered_map>

namespace emoji_table {

// What the Unicode emoji data says about a character that can be an emoji.
struct EmojiProperties {
	bool emoji{ true };
	bool emoji_presentation{};        // shown as a picture unless U+FE0E asks for text
	bool has_variation{};             // U+FE0E (text) or U+FE0F (emoji) may follow to choose how it looks
	bool component{};                 // part of a sequence: skin tones, keycaps, tags
};

extern const std::unordered_map<char32_t, EmojiProperties> emojis;

} // namespace emoji_table
