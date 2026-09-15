#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebug_ViewportPicker;
class FCkIntentDebugger_ViewModel;
class SCkIntentDebugger_DevicesPanel;
class SCkIntentDebugger_KeyStatePanel;
class SCkIntentDebugger_LayerStackPanel;
class SCkIntentDebugger_NearMissPanel;
class SCkIntentDebugger_ResolutionPanel;
class SCkIntentDebugger_TimelineDock;
class SHorizontalBox;
class SBox;
class SComboButton;
class SCkDebug_WindowChrome;
class FCkUiView;
class FCkUiCollection;
struct FCkIntentDebuggerAuthoredTestAccess;

// --------------------------------------------------------------------------------------------------------------------
// CK Intent Debugger window.
//
//   toolbar   world selector · local-player source selector · refresh controls
//   left      layer stack (the selection surface)
//   right     timeline dock over two splitter rows, everything visible at once (no tabs):
//             { Key/State | Devices } over { Resolution table | Near misses }
//
// The window owns the ViewModel and fans its OnChanged out to every panel; panels never poll the runtime.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkIntentDebuggerWindow) {}
#if WITH_DEV_AUTOMATION_TESTS
        /** Test-only alternate directory, exercised through the ordinary file-admission path. */
        SLATE_ARGUMENT(FString, TestResourceDirectory)
#endif
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkIntentDebuggerWindow() override;

    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override
    { return FText::FromString(TEXT("Intent")); }

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

    // The ONE definition of "an entity this debugger lists" — shared by the
    // module's FCkDebug_EntityTargetRoute and the window's viewport picker.
    static auto Is_IntentDebuggerEntity(const FCk_Handle& InCandidate) -> bool;

    auto Set_PendingTarget(int32 InLocalPlayerIndex, int32 InLayerPriority) -> void;

    /** Idempotently release all feature-owned Slate/runtime state before tab/module teardown. */
    auto Release_Presentation() -> void;
    auto Get_AuthoredBody() const -> TSharedPtr<FCkUiView> { return _AuthoredBody; }
    auto IsUsingNativeBodyFallback() const -> bool { return _UsingNativeBodyFallback; }

private:
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_Body() -> TSharedRef<SWidget>;
    auto Build_AuthoredBody() -> void;
    auto Poll_AuthoredBody(double InCurrentTime) -> void;

    auto HandleViewModelChanged() -> void;
    auto Refresh_SourceSelector() -> void;
    auto Refresh_SourceRecords() -> void;
    auto DoApply_PendingTarget() -> void;
    auto Get_StatusText() const -> FText;

private:
    friend struct FCkIntentDebuggerAuthoredTestAccess;

    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    // Shared viewport picker (CkDebuggerCommon), specialized to input sources/layers.
    TSharedPtr<FCkDebug_ViewportPicker> _ViewportPicker;

    TSharedPtr<SCkIntentDebugger_LayerStackPanel> _LayerStackPanel;
    TSharedPtr<SCkIntentDebugger_TimelineDock>    _TimelineDock;
    TSharedPtr<SCkIntentDebugger_KeyStatePanel>   _KeyStatePanel;
    TSharedPtr<SCkIntentDebugger_ResolutionPanel> _ResolutionPanel;
    TSharedPtr<SCkIntentDebugger_NearMissPanel>   _NearMissPanel;
    TSharedPtr<SCkIntentDebugger_DevicesPanel>    _DevicesPanel;

    TSharedPtr<SHorizontalBox>  _SourceSelectorBox;
    TSharedPtr<SCkDebug_WindowChrome> _Chrome;
    TSharedPtr<SComboButton> _InputHudMenu;
    TSharedPtr<SBox> _SourceSelectorHost;
    TSharedPtr<FCkUiCollection> _SourceRecords;
    TSharedPtr<SBox> _BodyHost;
    TSharedPtr<SBox> _LayerStackMount;
    TSharedPtr<SBox> _TimelineMount;
    TSharedPtr<SBox> _KeyStateMount;
    TSharedPtr<SBox> _ResolutionMount;
    TSharedPtr<SBox> _NearMissMount;
    TSharedPtr<SBox> _DevicesMount;
    TSharedPtr<FCkUiView> _AuthoredBody;
    FString _AuthoredBodyMarkupPath;
    FString _AuthoredBodyStylesheetPath;
    double _NextAuthoredBodyPollSeconds = 0.0;
    bool _UsingNativeBodyFallback = true;
    bool _PresentationReleased = false;
#if WITH_DEV_AUTOMATION_TESTS
    FString _TestResourceDirectory;
#endif

    int32 _LastSourceCount = INDEX_NONE;

    // Plain values, never a handle — see OpenForEntity.
    int32 _PendingLocalPlayerIndex = INDEX_NONE;
    int32 _PendingLayerPriority = MIN_int32;
    bool  _HasPendingTarget = false;

    FDelegateHandle _ViewModelChangedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
