#pragma once

#include "CkObjectPoolingDebugger/Data/CkObjectPoolingDebugger_Snapshot.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// --------------------------------------------------------------------------------------------------------------------

class SVerticalBox;
class STextBlock;
class FCkDebuggerModel_WorldSelector;

// ====================================================================================================================
// CK Object Pooling Debugger window.
//
// Shows, for the selected world, one row per (class, archetype) pool the CkCore ObjectPooling
// subsystem is managing — free / in-use / live / high-water / hits / misses / prewarm — plus the
// count of pinned-unique (DestroyOnRelease) instances. The table is rebuilt each gated tick; pool
// counts are small, so no in-place-update machinery is warranted.
// ====================================================================================================================

class SCkObjectPoolingDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkObjectPoolingDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Object Pooling Debugger")); }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildHeaderRow() -> TSharedRef<SWidget>;
    auto BuildPoolRow(const FCkObjectPoolingDebugger_PoolRow& InRow) -> TSharedRef<SWidget>;
    auto RebuildTable(const FCkObjectPoolingDebugger_Snapshot& InSnapshot) -> void;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<STextBlock>                     _SummaryText;
    TSharedPtr<SVerticalBox>                   _TableBox;
};

// ====================================================================================================================
