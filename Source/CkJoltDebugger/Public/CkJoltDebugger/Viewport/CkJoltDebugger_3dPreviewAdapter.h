#pragma once

#include "CkDebuggerCommon/Viewport/SCkDebug_3dPreviewViewport.h"
#include "CkJolt/Subsystem/CkJolt_DebugDrawTarget.h"

class CKJOLTDEBUGGER_API FCkJoltDebugger_3dPreviewAdapter final : public ICkDebug3dPreviewAdapter
{
  public:
    using FOnPick = TFunction<void(TOptional<uint64>, bool)>;
    using FOnRenderModeChanged = TFunction<void(ECk_Jolt_DebugDraw_RenderMode)>;
    using FOnGridChanged = TFunction<void(bool)>;
    using FOnLabelsChanged = TFunction<void(bool)>;
    using FOnDirectionGlyphScaleChanged = TFunction<void(float)>;
    using FOnIsolatedKeysChanged = TFunction<void(const TArray<uint64>&)>;
    using FOnDragArm = TFunction<void(uint64, const FVector&)>;
    using FOnDragRay = TFunction<void(const FCkDebug3dCursorRay&)>;
    using FOnDragPlaneShift = TFunction<void(float)>;
    using FOnDragRelease = TFunction<void()>;
    using FOnHover = TFunction<void(TOptional<uint64>)>;
    using FOnCommand = TFunction<void(ECkDebug3dNeutralCommand)>;

    auto
    Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget) -> void;
    auto
    Set_SelectionBounds(TOptional<FBox> InBounds) -> void;
    auto
    Set_OnPick(FOnPick InOnPick) -> void;
    auto
    Set_OnRenderModeChanged(FOnRenderModeChanged InCallback) -> void;
    auto
    Set_OnGridChanged(FOnGridChanged InCallback) -> void;
    auto
    Set_OnLabelsChanged(FOnLabelsChanged InCallback) -> void;
    auto
    Set_OnDirectionGlyphScaleChanged(FOnDirectionGlyphScaleChanged InCallback) -> void;
    auto
    Set_OnIsolatedKeysChanged(FOnIsolatedKeysChanged InCallback) -> void;
    auto
    Set_DragEnabled(bool InIsEnabled) -> void;
    auto
    Set_OnDragArm(FOnDragArm InCallback) -> void;
    auto
    Set_OnDragRay(FOnDragRay InCallback) -> void;
    auto
    Set_OnDragPlaneShift(FOnDragPlaneShift InCallback) -> void;
    auto
    Set_OnDragRelease(FOnDragRelease InCallback) -> void;
    auto
    Set_OnHover(FOnHover InCallback) -> void;
    auto
    Set_OnCommand(FOnCommand InCallback) -> void;

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
    CanArmDrag(const FCkDebug3dInteractionHit& InHit) const -> bool override;
    virtual auto
    ArmDrag(const FCkDebug3dInteractionHit& InHit) -> void override;
    virtual auto
    UpdateDrag(const FCkDebug3dCursorRay& InRay) -> void override;
    virtual auto
    ReleaseDrag() -> void override;
    virtual auto
    ShiftDragPlane(float InDirection) -> void override;
    virtual auto
    SetHover(TOptional<uint64> InIdentity) -> void override;
    virtual auto
    Command(ECkDebug3dNeutralCommand InCommand) -> void override;
    virtual auto
    Get_RenderMode() const -> ECkDebug3dRenderMode override;
    virtual auto
    Set_RenderMode(ECkDebug3dRenderMode InMode) -> void override;
    virtual auto
    Get_ShowGrid() const -> bool override;
    virtual auto
    Set_ShowGrid(bool InIsOn) -> void override;
    virtual auto
    Get_ShowLabels() const -> bool override;
    virtual auto
    Set_ShowLabels(bool InIsOn) -> void override;
    virtual auto
    Get_DirectionGlyphScale() const -> float override;
    virtual auto
    Set_DirectionGlyphScale(float InScale) -> void override;
    virtual auto
    Set_IsolatedKeys(const TArray<uint64>& InKeys) -> void override;
    virtual auto
    On_ViewportTeardown() -> void override;

  private:
    TWeakPtr<FCk_Jolt_DebugDrawTarget> _Target;
    FOnPick _OnPick;
    FOnRenderModeChanged _OnRenderModeChanged;
    FOnGridChanged _OnGridChanged;
    FOnLabelsChanged _OnLabelsChanged;
    FOnDirectionGlyphScaleChanged _OnDirectionGlyphScaleChanged;
    FOnIsolatedKeysChanged _OnIsolatedKeysChanged;
    FOnDragArm _OnDragArm;
    FOnDragRay _OnDragRay;
    FOnDragPlaneShift _OnDragPlaneShift;
    FOnDragRelease _OnDragRelease;
    FOnHover _OnHover;
    FOnCommand _OnCommand;
    bool _ShowGrid = true;
    bool _DragEnabled = false;
    TOptional<FBox> _SelectionBounds;
};
