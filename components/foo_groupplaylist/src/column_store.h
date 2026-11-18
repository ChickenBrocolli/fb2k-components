#pragma once

#include "column_def.h"

// Custom columns are shared by every panel and kept in the foobar2000 configuration.
std::vector<ColumnDef> LoadCustomColumnDefs();
void SaveCustomColumnDefs(const std::vector<ColumnDef>& defs);

// Built-in columns cannot be edited, but each can be left out of the Columns menu. These are their names.
std::vector<std::string> LoadHiddenBuiltInColumns();
void SaveHiddenBuiltInColumns(const std::vector<std::string>& names);
