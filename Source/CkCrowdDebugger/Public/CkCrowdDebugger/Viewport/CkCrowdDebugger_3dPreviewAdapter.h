#pragma once

#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"
#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"

class FCk_DebugScene_Target;

class CKCROWDDEBUGGER_API FCkCrowdDebugger_3dPreviewAdapter final : public ICkDebug3dPreviewAdapter
{
  public:
    using FOnSelected = TFunction<void(TOptional<int32>)>;
    auto
    Set_Target(TSharedPtr<FCk_DebugScene_Target> InTarget) -> void;
    auto
    Reconcile(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot) -> bool;
    auto
    Reset_World() -> void;
    auto
    Set_OnSelected(FOnSelected InOnSelected) -> void;
    virtual auto
    Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const -> FBox override;
    virtual auto
    Get_SelectionCenter() const -> TOptional<FVector> override;
    virtual auto
    Get_Capabilities() const -> ECkDebug3dViewportCapability override;
    virtual auto
    On_Pick(const FCkDebug3dCursorRay& InRay) -> void override;
    virtual auto
    TryHit(const FCkDebug3dCursorRay& InRay) -> TOptional<FCkDebug3dInteractionHit> override;
    virtual auto
    Select(uint64 InIdentity, bool InAdditive) -> void override;
    virtual auto
    ClearSelection(bool InAdditive) -> void override;
    virtual auto
    Get_RenderMode() const -> ECkDebug3dRenderMode override;
    virtual auto
    Set_RenderMode(ECkDebug3dRenderMode InMode) -> void override;
    virtual auto
    Get_ShowGrid() const -> bool override;
    virtual auto
    Set_ShowGrid(bool InIsOn) -> void override;
    virtual auto
    Set_IsolatedKeys(const TArray<uint64>& InKeys) -> void override;
    virtual auto
    On_ViewportTeardown() -> void override;

  private:
    auto
    PublishGrid() -> void;

    TSharedPtr<FCk_DebugScene_Target> _Target;
    FCkCrowdDebugger_3dSceneAdapter _Scene;
    FOnSelected _OnSelected;
    ECkDebug3dRenderMode _RenderMode = ECkDebug3dRenderMode::None;
    TOptional<uint64> _SelectedIdentity;
    TOptional<FCkCrowdDebugger_3dSceneSnapshot> _LastSnapshot;
    bool _ShowGrid = false;
};
