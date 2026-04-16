#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class UCkSmDebugGraph;
class SGraphEditor;
class UClass;
class UWorld;

// --------------------------------------------------------------------------------------------------------------------
// Right-side preview pane for statically walking a state machine asset.
// Shown alongside the live debugger via a 50/50 horizontal splitter. Owns its own
// debug graph so the main live view is untouched while previewing.
// --------------------------------------------------------------------------------------------------------------------

class CKSMDEBUGGER_API SCkSmDebugger_PreviewPane : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSmDebugger_PreviewPane) {}
    SLATE_END_ARGS()

    ~SCkSmDebugger_PreviewPane();

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto
    Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) -> void override;

    // Exposes the picker row so the host window can embed it inline with its main
    // toolbar. Keeps the preview pane's graph top aligned with the live graph top.
    auto BuildPickerRow() -> TSharedRef<SWidget>;

    // Called by the host each tick with the current PIE world (or null). The walker
    // needs a live ECS world to run in; when the world disappears we reset pending state.
    auto Set_WorldContext(const TWeakObjectPtr<UWorld>& InWorld) -> void;

private:
    auto BuildHeader() -> TSharedRef<SWidget>;
    auto GetSelectedClassPath() const -> FString;
    auto RebuildPreviewGraph() -> void;
    auto GetStatusText() const -> FText;

    // Walker lifecycle helpers
    auto StartWalk() -> void;
    auto PollWalkAndFinalize() -> void;
    auto DestroyPreviewEntity() -> void;
    auto PublishStructuralSmInfo() -> void;

    // Owned graph + editor — independent of the live debugger graph
    UCkSmDebugGraph*           _PreviewGraph = nullptr;
    TSharedPtr<SGraphEditor>   _PreviewGraphEditor;

    TSoftClassPtr<UObject>     _SelectedInitialStateClass;
    FString                    _StatusMessage;

    TWeakObjectPtr<UWorld>     _World;

    // Handle to the throwaway SM entity used to run the walker. Destroyed on reset.
    FCk_Handle_StateMachine    _PreviewSmHandle;
    bool                       _WalkInProgress = false;
};

// --------------------------------------------------------------------------------------------------------------------
