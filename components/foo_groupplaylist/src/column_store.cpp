#include "stdafx.h"

#include "column_store.h"

namespace {

constexpr GUID guid_custom_columns = { 0x3ac807d1, 0xb847, 0x47db, { 0x9b, 0x81, 0x5c, 0x2b, 0x71, 0x78, 0x18, 0xb0 } };

cfg_string cfg_customColumns(guid_custom_columns, SerializeColumnDefs(DefaultCustomColumnDefs()).c_str());

constexpr GUID guid_hidden_built_in_columns = { 0x2e318e16, 0x4072, 0x44b5, { 0x92, 0x20, 0x02, 0x25, 0xae, 0x74, 0x1f, 0xaf } };

cfg_string cfg_hiddenBuiltInColumns(guid_hidden_built_in_columns, "");

constexpr GUID guid_built_in_column_edits = { 0x801dd8ba, 0x51ef, 0x41b8, { 0x8e, 0x4f, 0xa4, 0xb8, 0x49, 0x4f, 0x1b, 0xc3 } };

cfg_string cfg_builtInColumnEdits(guid_built_in_column_edits, "");

constexpr GUID guid_column_order = { 0x5d0c7a3e, 0x92b4, 0x4e17, { 0xa6, 0x38, 0x1f, 0x4b, 0xc9, 0x07, 0x2d, 0x65 } };

cfg_string cfg_columnOrder(guid_column_order, "");

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

std::vector<std::string> LoadColumnOrder() {
	return ParseColumnNames(cfg_columnOrder.get().c_str());
}

void SaveColumnOrder(const std::vector<std::string>& names) {
	cfg_columnOrder = SerializeColumnNames(names).c_str();
}

std::vector<ColumnDef> SavedColumnDefs() {
	std::vector<ColumnDef> defs = SavedBuiltInColumnDefs();
	for (ColumnDef& def : LoadCustomColumnDefs()) defs.push_back(std::move(def));
	return OrderColumnDefs(defs, LoadColumnOrder());
}
