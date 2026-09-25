#pragma once

#include "CkDebuggerCommon/Widgets/SCkDebug_EventTimeline.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkProceduralAnimationDebugger/Model/CkProceduralAnimationDebugger_Model.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkDebuggerModel_WorldSelector;
class FCkDebug_ViewportPicker;
class FCkProceduralAnimationDebugger_Preview;
class SCkDebug_EntityHealthList;
class SCkDebug_EvidenceList;
class SCkDebug_3dPreviewViewport;
class SBox;

// --------------------------------------------------------------------------------------------------------------------

// Timeline and sparkline presentation, appended from the history ring and evicted with it by solve sequence.
struct CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_TimelineCache
{
    TArray<FCkDebug_TimelineEvent> Events;
    TArray<FCkDebug_TimelineSpan> Spans;
    TArray<uint64> SpanSequences;
    TArray<uint64> SampleSequences;
    TSharedRef<TArray<float>> SpeedSamples = MakeShared<TArray<float>>();
    TSharedRef<TArray<float>> SupportSamples = MakeShared<TArray<float>>();
    FString EntityId;
    uint64 Revision = MAX_uint64;

    auto Reset() -> void;
    auto EvictBefore(uint64 InFirstSequence, int32 InFirstSelectionId) -> void;
};

// --------------------------------------------------------------------------------------------------------------------

class CKPROCEDURALANIMATIONDEBUGGER_API SCkProceduralAnimationDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAnimationDebuggerWindow) {}
    SLATE_END_ARGS()

public:
    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkProceduralAnimationDebuggerWindow() override;
    auto Tick(const FGeometry& InGeometry, double InTime, float InDeltaTime) -> void override;
    auto Get_WindowId() const -> FName override { return TEXT("ProceduralAnimationDebugger"); }
    auto Get_WindowDisplayName() const -> FText override;
    auto Get_Model() const -> TSharedPtr<FCkProceduralAnimationDebugger_Model> { return _Model; }
    auto Request_Refresh() -> void;
    auto ReleaseSession() -> void;
    static auto OpenForEntity(const FCk_Handle& InEntity) -> void;
    static auto Is_ProceduralEntity(const FCk_Handle& InEntity) -> bool;

private:
    auto DoBuild_LegActions() -> TSharedRef<SWidget>;
    auto DoRefresh_Presentation() -> void;
    auto DoRefresh_Roster() -> void;
    auto DoRefresh_Legs(const FCk_ProceduralAnimation_DebugSnapshot& InSample) -> void;
    auto DoRefresh_Timeline() -> void;
    auto DoRecreate_Timeline(const TArray<FString>& InLaneLabels) -> void;
    auto DoAppend_TimelineSample(int32 InChronologicalIndex) -> void;
    auto DoSelect_Leg(const FString& InLegEntityId) -> void;
    auto DoSelect_LegAt(int32 InLegIndex) -> void;
    auto DoScrub_Time(double InSeconds) -> void;
    auto DoGet_StatusText() const -> FText;
    auto DoGet_DetailText() const -> FText;

private:
    TSharedPtr<FCkProceduralAnimationDebugger_Model> _Model;
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<FCkDebug_ViewportPicker> _Picker;
    TSharedPtr<SCkDebug_EntityHealthList> _Roster;
    TSharedPtr<SCkDebug_EvidenceList> _Legs;
    TSharedPtr<SCkDebug_EventTimeline> _Timeline;
    TSharedPtr<SBox> _TimelineHost;
    TSharedPtr<FCkProceduralAnimationDebugger_Preview> _Preview;
    TSharedPtr<SCkDebug_3dPreviewViewport> _Viewport;
    FCkProceduralAnimationDebugger_TimelineCache _TimelineCache;
    TArray<FString> _LaneLabels;
    FString _Filter;
    FString _FramedEntity;
    bool _Released = false;
    FDelegateHandle _ModelChangedHandle;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _PreviewSelectedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
