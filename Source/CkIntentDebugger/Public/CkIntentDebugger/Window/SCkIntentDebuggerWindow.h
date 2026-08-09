#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;
class SCkIntentDebugger_KeyStatePanel;
class SCkIntentDebugger_LayerStackPanel;
class SCkIntentDebugger_NearMissPanel;
class SCkIntentDebugger_ResolutionPanel;
class SCkIntentDebugger_TimelineDock;
class SHorizontalBox;
class SWidgetSwitcher;

// --------------------------------------------------------------------------------------------------------------------
// CK Intent Debugger window.
//
//   toolbar   world selector · local-player source selector · refresh controls
//   left      layer stack (the selection surface)
//   right     timeline dock over { Key/State · Resolution table · Near misses }
//
// The window owns the ViewModel and fans its OnChanged out to every panel; panels never poll the runtime.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkIntentDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkIntentDebuggerWindow() override;

    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override
    { return FText::FromString(TEXT("CK Intent Debugger")); }

public:
    /**
     * Opens the tab and targets an entity that is (or whose lineage contains) an input source or an input layer.
     *
     * The target is reduced to PLAIN VALUES — the owning local player's index and, for a layer, its priority —
     * before anything is stored. A just-opened window has no snapshot yet, so the target has to survive until the
     * first collector pass; retaining an `FCk_Handle` across that gap is what makes a debugger crash at the next
     * PIE start, and neither of these two ints can.
     */
    static auto OpenForEntity(const FCk_Handle& InEntity) -> void;

    auto Set_PendingTarget(int32 InLocalPlayerIndex, int32 InLayerPriority) -> void;

private:
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_Body() -> TSharedRef<SWidget>;

    auto HandleViewModelChanged() -> void;
    auto Refresh_SourceSelector() -> void;
    auto DoApply_PendingTarget() -> void;
    auto Get_StatusText() const -> FText;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkIntentDebugger_LayerStackPanel> _LayerStackPanel;
    TSharedPtr<SCkIntentDebugger_TimelineDock>    _TimelineDock;
    TSharedPtr<SCkIntentDebugger_KeyStatePanel>   _KeyStatePanel;
    TSharedPtr<SCkIntentDebugger_ResolutionPanel> _ResolutionPanel;
    TSharedPtr<SCkIntentDebugger_NearMissPanel>   _NearMissPanel;

    TSharedPtr<SHorizontalBox>  _SourceSelectorBox;
    TSharedPtr<SWidgetSwitcher> _DetailSwitcher;

    FName _ActiveTabId;
    int32 _LastSourceCount = INDEX_NONE;

    // Plain values, never a handle — see OpenForEntity.
    int32 _PendingLocalPlayerIndex = INDEX_NONE;
    int32 _PendingLayerPriority = MIN_int32;
    bool  _HasPendingTarget = false;

    FDelegateHandle _ViewModelChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
