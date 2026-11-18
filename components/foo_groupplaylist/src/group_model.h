#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

// Splits a list into albums, and each album into blocks.
//
// An album starts at every item whose album key differs from the key of the item before it,
// so a key that comes back later starts another album. Inside an album, a block starts at
// every item whose block key differs from the one before it. An album start is always a block start.
//
// On screen an album gets a header line, and a block gets its own cover image and info lines.
//
// Subgroups add label rows such as "Disc 2" above a track. Each subgroup level has one text
// per item. A block start shows the text of every level. Inside a block, the first level whose
// text changed and every level below it show their text. An empty text shows no row.
//
// Standard library only, so the unit tests build without the foobar2000 SDK.
class GroupModel {
public:
	struct Album {
		size_t firstItem;
		size_t firstBlock;
		std::string key;
	};

	struct Block {
		size_t firstItem;
		size_t album;
	};

	// Label rows drawn above one item. Levels firstLevel and deeper show their text, lineCount of them non-empty.
	struct Label {
		size_t item;
		size_t firstLevel;
		size_t lineCount;
	};

	typedef std::vector<std::string> Keys;

	// Every list holds one entry per item. subgroupTexts holds one list per subgroup level, outermost first.
	// With hideUniformLabels, a label level whose text is the same for every track of an album shows no
	// label in that album, such as "Disc 1" on an album that has one disc.
	void Rebuild(const Keys& albumKeys, const Keys& blockKeys, const std::vector<Keys>& subgroupTexts = {}, bool hideUniformLabels = false) {
		m_albums.clear();
		m_blocks.clear();
		m_labels.clear();
		m_labelLinesBefore.assign(1, 0);
		m_levelShown.clear();
		m_itemCount = albumKeys.size();
		const size_t levels = subgroupTexts.size();

		// Which levels each album shows, worked out before the labels so that hidden levels count as empty.
		std::vector<size_t> albumOfItem(m_itemCount);
		for (size_t item = 0; item < m_itemCount; ++item) {
			const bool newAlbum = item == 0 || albumKeys[item] != albumKeys[item - 1];
			if (newAlbum) m_levelShown.push_back(std::vector<bool>(levels, !hideUniformLabels));
			albumOfItem[item] = m_levelShown.size() - 1;
			if (!newAlbum) {
				for (size_t level = 0; level < levels; ++level) {
					if (subgroupTexts[level][item] != subgroupTexts[level][item - 1]) m_levelShown.back()[level] = true;
				}
			}
		}
		const auto text = [&](size_t level, size_t item) -> const std::string& {
			static const std::string empty;
			return m_levelShown[albumOfItem[item]][level] ? subgroupTexts[level][item] : empty;
		};

		for (size_t item = 0; item < m_itemCount; ++item) {
			const bool newAlbum = item == 0 || albumKeys[item] != albumKeys[item - 1];
			const bool newBlock = newAlbum || blockKeys[item] != blockKeys[item - 1];
			if (newAlbum) m_albums.push_back({ item, m_blocks.size(), albumKeys[item] });
			if (newBlock) m_blocks.push_back({ item, m_albums.size() - 1 });

			size_t firstLevel = 0;
			if (!newBlock) {
				while (firstLevel < levels && text(firstLevel, item) == text(firstLevel, item - 1)) ++firstLevel;
			}
			size_t lineCount = 0;
			for (size_t level = firstLevel; level < levels; ++level) {
				if (!text(level, item).empty()) ++lineCount;
			}
			if (lineCount > 0) {
				m_labels.push_back({ item, firstLevel, lineCount });
				m_labelLinesBefore.push_back(m_labelLinesBefore.back() + lineCount);
			}
		}
	}

	size_t ItemCount() const { return m_itemCount; }

	// False when a label level is hidden in the album that holds item. item must be below ItemCount().
	bool IsLevelShown(size_t item, size_t level) const {
		const std::vector<bool>& shown = m_levelShown[AlbumIndexOf(item)];
		return level < shown.size() && shown[level];
	}

	size_t AlbumCount() const { return m_albums.size(); }
	const Album& AlbumAt(size_t album) const { return m_albums[album]; }
	size_t AlbumIndexOf(size_t item) const { return IndexOf(m_albums, item); }
	size_t AlbumBlockEnd(size_t album) const {
		return album + 1 < m_albums.size() ? m_albums[album + 1].firstBlock : m_blocks.size();
	}

	size_t BlockCount() const { return m_blocks.size(); }
	const Block& BlockAt(size_t block) const { return m_blocks[block]; }
	size_t BlockIndexOf(size_t item) const { return IndexOf(m_blocks, item); }
	size_t BlockSize(size_t block) const {
		const size_t end = block + 1 < m_blocks.size() ? m_blocks[block + 1].firstItem : m_itemCount;
		return end - m_blocks[block].firstItem;
	}
	bool IsLastBlockOfAlbum(size_t block) const {
		return block + 1 == AlbumBlockEnd(m_blocks[block].album);
	}

	// Returns null when no label row sits above the item.
	const Label* LabelAt(size_t item) const {
		const size_t index = FirstLabelAtOrAfter(item);
		return index < m_labels.size() && m_labels[index].item == item ? &m_labels[index] : nullptr;
	}
	size_t LabelLinesInBlock(size_t block) const {
		const size_t first = m_blocks[block].firstItem;
		return m_labelLinesBefore[FirstLabelAtOrAfter(first + BlockSize(block))] - m_labelLinesBefore[FirstLabelAtOrAfter(first)];
	}

private:
	// item must be below ItemCount().
	template<typename Range> static size_t IndexOf(const std::vector<Range>& ranges, size_t item) {
		const auto next = std::upper_bound(ranges.begin(), ranges.end(), item,
			[](size_t value, const Range& range) { return value < range.firstItem; });
		return static_cast<size_t>(next - ranges.begin()) - 1;
	}

	size_t FirstLabelAtOrAfter(size_t item) const {
		const auto found = std::lower_bound(m_labels.begin(), m_labels.end(), item,
			[](const Label& label, size_t value) { return label.item < value; });
		return static_cast<size_t>(found - m_labels.begin());
	}

	std::vector<Album> m_albums;
	// Per album, per label level: false when the level is hidden in that album.
	std::vector<std::vector<bool>> m_levelShown;
	std::vector<Block> m_blocks;
	std::vector<Label> m_labels;
	// m_labelLinesBefore[n] is the total line count of the first n labels. One entry more than m_labels.
	std::vector<size_t> m_labelLinesBefore = { 0 };
	size_t m_itemCount = 0;
};
