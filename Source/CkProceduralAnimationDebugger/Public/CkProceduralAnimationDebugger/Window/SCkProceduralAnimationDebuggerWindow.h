#pragma once

#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"
#include "CkProceduralAnimationDebugger/Model/CkProceduralAnimationDebugger_Model.h"

class FCkDebuggerModel_WorldSelector;
class FCkDebug_ViewportPicker;
class FCkProceduralAnimationDebugger_Preview;
class SCkDebug_EntityHealthList;
class SCkDebug_EvidenceList;
class SCkDebug_EventTimeline;
class SCkDebug_3dPreviewViewport;
class SBox;

class CKPROCEDURALANIMATIONDEBUGGER_API SCkProceduralAnimationDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    SLATE_BEGIN_ARGS(SCkProceduralAnimationDebuggerWindow) {}
    SLATE_END_ARGS()
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
    auto RefreshTimeline() -> void;
    auto SelectLeg(int32 InLeg) -> void;
    auto ScrubTime(double InSeconds) -> void;
    auto Get_StatusText() const -> FText;
    auto Get_DetailText() const -> FText;
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
    TArray<FString> _LaneLabels;
    FString _Filter;
    FString _FramedEntity;
    int32 _SelectedLeg = INDEX_NONE;
    bool _Released = false;
    FDelegateHandle _ModelChangedHandle;
    FDelegateHandle _WorldChangedHandle;
    FDelegateHandle _PreviewSelectedHandle;
};
