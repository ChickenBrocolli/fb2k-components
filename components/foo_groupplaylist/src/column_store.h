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

// Each built-in column can be left out of the Columns menu. These are their names.
std::vector<std::string> LoadHiddenBuiltInColumns();
void SaveHiddenBuiltInColumns(const std::vector<std::string>& names);
