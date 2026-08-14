#pragma once

#include "CkAStarDebugger/Data/CkAStarDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkAStarDebugger_ViewModel;
class FCkDebug_ViewportPicker;
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
    ~SCkAStarDebuggerWindow() override;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto TargetEntity(const FCk_Handle& InEntity) -> void;

    // The ONE definition of "an entity this debugger lists" — shared by the
    // module's FCkDebug_EntityTargetRoute and the window's viewport picker.
    static auto Is_AStarDebuggerEntity(const FCk_Handle& InCandidate) -> bool;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("A*")); }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto RefreshEntitySelector() -> void;
    auto HandleWorldChanged(UWorld* InWorld) -> void;
    auto HandleSessionInvalidated() -> void;

    TSharedPtr<FCkAStarDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkAStarDebugger_GridView> _GridView;
    TSharedPtr<SCkAStarDebugger_StatsPanel> _StatsPanel;
    TSharedPtr<SCkAStarDebugger_SearchHistory> _SearchHistory;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;

    // Shared viewport picker (CkDebuggerCommon), specialized to A* search entities.
    TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;

    TArray<TSharedPtr<FString>> _EntitySelectorItems;
    TArray<FCk_Handle> _EntitySelectorHandles;
    TSharedPtr<STextBlock> _EntitySelectorLabel;
    TSharedPtr<STextBlock> _StatusBadgeText;

    // One-shot external target consumed immediately after the first refreshed
    // search-entity list; never retained by the module.
    TOptional<FCk_Entity> _PendingTarget;

    UWorld* _CachedWorld = nullptr;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _SessionInvalidatedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
