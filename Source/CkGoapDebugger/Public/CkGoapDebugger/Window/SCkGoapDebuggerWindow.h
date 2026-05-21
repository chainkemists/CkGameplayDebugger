#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// ====================================================================================================================

class FCkGoapDebugger_ViewModel;
class SCkGoapDebugger_Sidebar;
class SCkGoapDebugger_Breadcrumb;
class SCkGoapDebugger_PrimaryPane;
class SCkGoapDebugger_WorldStateRail;
class SCkGoapDebugger_GraphPane;
class STextBlock;
class SBox;
template <typename> class SComboBox;

// ====================================================================================================================
// SCkGoapDebuggerWindow — top-level Slate widget hosted by the GoapDebugger
// NomadTab. Owns:
//
//   - The ViewModel (selection state, mode, snapshot batch).
//   - The Sidebar (ActionSet tree + history list).
//   - Center column (breadcrumb + primary + graph stub) and the WS rail.
//   - Toolbar (entity picker, Live/Scrub toggle, Force replan).
//   - Mode bar (Standalone vs ECS-Inspector — only Standalone implemented in D2).
//   - Legend (action-color descriptive row).
//
// Lifetime:
//   - Tick polls for the PIE world, then ticks the ViewModel.
//   - PIE Begin/End delegate paths clear handle-bearing state through the
//     ViewModel + Sidebar so we don't leak FCk_Handle copies past the
//     registry death.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkGoapDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkGoapDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Goap Debugger")); }

private:
    // -----------------------------------------------------------------------------------------------------------------
    // Build helpers — called once from Construct; result subtrees are cached.
    // -----------------------------------------------------------------------------------------------------------------

    auto BuildModeBar()  -> TSharedRef<SWidget>;
    auto BuildToolbar()  -> TSharedRef<SWidget>;
    auto BuildLegend()   -> TSharedRef<SWidget>;
    auto BuildCenterColumn()  -> TSharedRef<SWidget>;

    // Refresh the entity picker's items array off the current snapshot batch.
    // Cheap; called every Tick. Set rebuilds use stable string identity by
    // matching against existing entries.
    auto RefreshEntityPickerItems() -> void;

    // PIE lifecycle — drop handle-bearing state.
    auto HandleWorldTornDown() -> void;

private:
    TSharedPtr<FCkGoapDebugger_ViewModel>   _ViewModel;
    TSharedPtr<SCkGoapDebugger_Sidebar>     _Sidebar;
    TSharedPtr<SCkGoapDebugger_Breadcrumb>  _Breadcrumb;
    TSharedPtr<SCkGoapDebugger_PrimaryPane> _PrimaryPane;
    TSharedPtr<SCkGoapDebugger_WorldStateRail> _WorldStateRail;
    TSharedPtr<SCkGoapDebugger_GraphPane>   _GraphPane;

    TWeakObjectPtr<UWorld> _CachedWorld;

    // Entity picker state
    TArray<TSharedPtr<FString>> _EntityPickerItems;
    TArray<FCk_Handle>          _EntityPickerHandles;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> _EntityPicker;
    TSharedPtr<STextBlock>      _EntityPickerLabel;

    // Editor delegate handles
    FDelegateHandle _OnBeginPieHandle;
    FDelegateHandle _OnEndPieHandle;
};

// ====================================================================================================================
