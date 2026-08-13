#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

// ====================================================================================================================
// Pure-data snapshot of the player's Enhanced Input User Settings PROFILE — the rebind layer the
// structural snapshot cannot see: per mapping, the authored DEFAULT key beside the player's
// CURRENT key, its display metadata, and its Ck scope tags. Rows arrive grouped by display
// category in first-seen order, ready to render.
// ====================================================================================================================

class APlayerController;

struct FCkInputDebugger_BindingRow
{
    FName   MappingName;
    FString DisplayName;
    FString Category;
    FString ScopeTags;     // joined Ck scope tags; empty when the mapping is on stock settings
    FKey    DefaultKey;
    FKey    CurrentKey;
    FString SlotLabel;     // "First" / "Second" / ...
    bool    IsGamepad = false;
    bool    IsRebound = false;
};

struct FCkInputDebugger_BindingsSnapshot
{
    TArray<FString> CategoryOrder;
    TMap<FString, TArray<FCkInputDebugger_BindingRow>> RowsByCategory;

    // Changes only when the profile's rows / keys / metadata change — the pane's rebuild gate.
    FString Signature;

    static auto Gather(
        APlayerController* InPlayerController) -> FCkInputDebugger_BindingsSnapshot;
};

// ====================================================================================================================
