#include "stdafx.h"

#include "column_store.h"

namespace {

constexpr GUID guid_custom_columns = { 0x3ac807d1, 0xb847, 0x47db, { 0x9b, 0x81, 0x5c, 0x2b, 0x71, 0x78, 0x18, 0xb0 } };

cfg_string cfg_customColumns(guid_custom_columns, SerializeColumnDefs(DefaultCustomColumnDefs()).c_str());

constexpr GUID guid_hidden_built_in_columns = { 0x2e318e16, 0x4072, 0x44b5, { 0x92, 0x20, 0x02, 0x25, 0xae, 0x74, 0x1f, 0xaf } };

cfg_string cfg_hiddenBuiltInColumns(guid_hidden_built_in_columns, "");

constexpr GUID guid_built_in_column_edits = { 0x801dd8ba, 0x51ef, 0x41b8, { 0x8e, 0x4f, 0xa4, 0xb8, 0x49, 0x4f, 0x1b, 0xc3 } };

cfg_string cfg_builtInColumnEdits(guid_built_in_column_edits, "");

} // namespace

std::vector<ColumnDef> LoadCustomColumnDefs() {
	std::vector<std::string> builtInNames;
	for (const ColumnDef& def : BuiltInColumnDefs()) builtInNames.push_back(def.name);
	return RenameClashingColumns(ParseColumnDefs(cfg_customColumns.get().c_str()), builtInNames);
}

void SaveCustomColumnDefs(const std::vector<ColumnDef>& defs) {
	cfg_customColumns = SerializeColumnDefs(defs).c_str();
}

std::vector<ColumnDef> LoadBuiltInColumnEdits() {
	return ParseBuiltInEdits(cfg_builtInColumnEdits.get().c_str());
}

void SaveBuiltInColumnEdits(const std::vector<ColumnDef>& edits) {
	cfg_builtInColumnEdits = SerializeBuiltInEdits(edits).c_str();
}

std::vector<ColumnDef> SavedBuiltInColumnDefs() {
	return ApplyBuiltInEdits(BuiltInColumnDefsForMenu(LoadHiddenBuiltInColumns()), LoadBuiltInColumnEdits());
}

std::vector<std::string> LoadHiddenBuiltInColumns() {
	return ParseColumnNames(cfg_hiddenBuiltInColumns.get().c_str());
}

void SaveHiddenBuiltInColumns(const std::vector<std::string>& names) {
	cfg_hiddenBuiltInColumns = SerializeColumnNames(names).c_str();
}
