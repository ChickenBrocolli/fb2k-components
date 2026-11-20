#pragma once

#include "column_def.h"

// Custom columns are shared by every panel and kept in the foobar2000 configuration.
std::vector<ColumnDef> LoadCustomColumnDefs();
void SaveCustomColumnDefs(const std::vector<ColumnDef>& defs);

// What the user changed in built-in columns (column_def.h, SerializeBuiltInEdits). Only changed columns are kept.
std::vector<ColumnDef> LoadBuiltInColumnEdits();
void SaveBuiltInColumnEdits(const std::vector<ColumnDef>& edits);

// The built-in columns with the user's changes, and the named ones left out of the Columns menu.
std::vector<ColumnDef> SavedBuiltInColumnDefs();

// The names of all columns, built-in and custom, in the order of the Columns page and the Columns menu.
// Empty until the user moves a column: then the built-in columns come first, as they are listed in column_def.h.
std::vector<std::string> LoadColumnOrder();
void SaveColumnOrder(const std::vector<std::string>& names);

// Every column, built-in with the user's changes and custom, in the user's order (OrderColumnDefs).
std::vector<ColumnDef> SavedColumnDefs();

// Each built-in column can be left out of the Columns menu. These are their names.
std::vector<std::string> LoadHiddenBuiltInColumns();
void SaveHiddenBuiltInColumns(const std::vector<std::string>& names);
