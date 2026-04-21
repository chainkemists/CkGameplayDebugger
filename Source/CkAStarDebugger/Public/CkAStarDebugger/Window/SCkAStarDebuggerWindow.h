#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;
class SCkAStarDebugger_GridView;
class SCkAStarDebugger_StatsPanel;
class SCkAStarDebugger_SearchHistory;

// --------------------------------------------------------------------------------------------------------------------
// Top-level debugger window — placed inside the NomadTab.
// Composes grid view, stats panel, search history, and toolbar.
// --------------------------------------------------------------------------------------------------------------------

class SCkAStarDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkAStarDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK A* Debugger")); }

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto RefreshEntitySelector() -> void;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkAStarDebugger_GridView> _GridView;
    TSharedPtr<SCkAStarDebugger_StatsPanel> _StatsPanel;
    TSharedPtr<SCkAStarDebugger_SearchHistory> _SearchHistory;

    TArray<TSharedPtr<FString>> _EntitySelectorItems;
    TArray<FCk_Handle> _EntitySelectorHandles;
    TSharedPtr<STextBlock> _EntitySelectorLabel;
    TSharedPtr<STextBlock> _StatusBadgeText;

    UWorld* _CachedWorld = nullptr;
};

// --------------------------------------------------------------------------------------------------------------------
