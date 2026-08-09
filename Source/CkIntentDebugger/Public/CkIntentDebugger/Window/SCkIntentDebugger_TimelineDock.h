#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkIntentDebugger_ViewModel;
class SBox;
class SCkDebug_EventTimeline;

// --------------------------------------------------------------------------------------------------------------------
// Timeline view — the shared lanes widget, driven by frame indices rather than seconds.
//
// Lanes:
//   · one per LAYER (stack order, top-down) — spans over the frames that layer ended a routing walk on
//   · one per INTENT of the selected layer's active set — spans per witnessed phase, coloured by phase
//   · BLOCKED — one selectable marker per pending episode, at the press frame it opened on, annotated with the
//     cause that is armed and the candidates it is holding open
//
// The axis is the sampler's own monotonic logic-frame counter, fed to the widget's double time axis unchanged. A
// wall-clock axis would disagree with the record on the first hitch, which is exactly when a reader needs them to
// agree.
//
// Clicking a marker scrubs the ViewModel to that frame; every other view follows.
// --------------------------------------------------------------------------------------------------------------------

class SCkIntentDebugger_TimelineDock : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_TimelineDock) {}
        SLATE_ARGUMENT(TSharedPtr<FCkIntentDebugger_ViewModel>, ViewModel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto RefreshFromViewModel() -> void;
    auto Reset_ForWorldChange() -> void;

private:
    auto DoRebuild_Timeline() -> void;

private:
    TSharedPtr<FCkIntentDebugger_ViewModel> _ViewModel;

    TSharedPtr<SCkDebug_EventTimeline> _Timeline;
    TSharedPtr<SBox> _TimelineHost;

    // Marker selection ids are frame indices, so a click reports the frame directly.
    TArray<FString> _LaneLabels;

    uint32 _StructureHash = 0;
};

// --------------------------------------------------------------------------------------------------------------------
