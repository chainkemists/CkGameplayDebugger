#pragma once

#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"
#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"

class FCk_DebugScene_Target;

// Presentation geometry belongs to the preview world. No gameplay handles or world survive capture.
class CKPROCEDURALANIMATIONDEBUGGER_API FCkProceduralAnimationDebugger_Preview : public ICkDebug3dPreviewAdapter
{
public:
    auto Initialize(UWorld* InPreviewWorld) -> void;
    auto Show(const FCk_ProceduralAnimation_DebugSnapshot& InSample, int32 InSelectedLeg = INDEX_NONE) -> void;
    auto Reset() -> void;
    auto Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const -> FBox override;
    auto Get_SelectionCenter() const -> TOptional<FVector> override;
    auto Get_Capabilities() const -> ECkDebug3dViewportCapability override;
    auto On_Pick(const FCkDebug3dCursorRay& InRay) -> void override;
    auto TryHit(const FCkDebug3dCursorRay& InRay) -> TOptional<FCkDebug3dInteractionHit> override;
    auto Select(uint64 InIdentity, bool InAdditive) -> void override;
    auto On_ViewportTeardown() -> void override;
    auto Get_ShowLabels() const -> bool override { return _ShowLabels; }
    auto Set_ShowLabels(bool InShow) -> void override;
    auto Get_OnLegSelected() -> TMulticastDelegate<void(int32)>& { return _OnLegSelected; }

private:
    TSharedPtr<FCk_DebugScene_Target> _Target;
    TOptional<FCk_ProceduralAnimation_DebugSnapshot> _Sample;
    TMulticastDelegate<void(int32)> _OnLegSelected;
    TOptional<FVector> _SelectedCenter;
    bool _ShowLabels = true;
    int32 _SelectedLeg = INDEX_NONE;
};
