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
    auto RefreshPresentation() -> void;
    auto RefreshRoster() -> void;
    auto RefreshLegs(const FCk_ProceduralAnimation_DebugSnapshot& InSample) -> void;
    auto RefreshTimeline() -> void;
    auto RecreateTimeline(const TArray<FString>& InLaneLabels) -> void;
    auto ResetTimelineData() -> void;
    auto AppendTimelineSample(int32 InChronologicalIndex) -> void;
    auto SelectLeg(const FString& InLegEntityId) -> void;
    auto SelectLegAt(int32 InLegIndex) -> void;
    auto ScrubTime(double InSeconds) -> void;
    auto Get_StatusText() const -> FText;
    auto Get_DetailText() const -> FText;

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
    TSharedPtr<TArray<float>> _SpeedSamples;
    TSharedPtr<TArray<float>> _SupportSamples;
    TArray<FCkDebug_TimelineEvent> _TimelineEvents;
    TArray<FCkDebug_TimelineSpan> _TimelineSpans;
    TArray<uint64> _TimelineSpanSequences;
    TArray<uint64> _TimelineSampleSequences;
    TArray<FString> _LaneLabels;
    FString _TimelineEntityId;
    uint64 _TimelineRevision = MAX_uint64;
    FString _Filter;
    FString _FramedEntity;
    bool _Released = false;
    FDelegateHandle _ModelChangedHandle;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _PreviewSelectedHandle;
};

// --------------------------------------------------------------------------------------------------------------------
