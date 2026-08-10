#pragma once

#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"

// --------------------------------------------------------------------------------------------------------------------
// COMPATIBILITY SHIM — the single-mode search bar was promoted to CkDebuggerCommon
// (SCkDebug_SearchBar) in the 2026-08-09 common-widget consolidation. This header keeps the old
// spelling alive so the ECS call sites compile unchanged; a later unit of that campaign rewrites
// those call sites and deletes this file.
//
// Do NOT add behavior here. New code includes CkDebuggerCommon/Search/SCkDebug_SearchBar.h.
// --------------------------------------------------------------------------------------------------------------------

using SCkDebuggerWidget_SearchBar = SCkDebug_SearchBar;
using FCkDebugger_OnSearchTextChanged = FCkDebug_OnSearchTextChanged;

// --------------------------------------------------------------------------------------------------------------------
