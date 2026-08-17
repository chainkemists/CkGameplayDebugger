#include "CkJoltDebugger/Viewport/CkJoltDebugger_3dPreviewAdapter.h"

namespace ck_jolt_debugger_3d_preview_adapter
{
auto
To_Common(ECk_Jolt_DebugDraw_RenderMode InMode) -> ECkDebug3dRenderMode
{
    return InMode == ECk_Jolt_DebugDraw_RenderMode::SensorWireframe ? ECkDebug3dRenderMode::TransparentOnly
           : InMode == ECk_Jolt_DebugDraw_RenderMode::Wireframe     ? ECkDebug3dRenderMode::All
                                                                    : ECkDebug3dRenderMode::None;
}
auto
To_Jolt(ECkDebug3dRenderMode InMode) -> ECk_Jolt_DebugDraw_RenderMode
{
    return InMode == ECkDebug3dRenderMode::TransparentOnly ? ECk_Jolt_DebugDraw_RenderMode::SensorWireframe
           : InMode == ECkDebug3dRenderMode::All           ? ECk_Jolt_DebugDraw_RenderMode::Wireframe
                                                           : ECk_Jolt_DebugDraw_RenderMode::Solid;
}
} // namespace ck_jolt_debugger_3d_preview_adapter

auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_Target(TSharedPtr<FCk_Jolt_DebugDrawTarget> InTarget)
    -> void
{
    _Target = MoveTemp(InTarget);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_SelectionBounds(TOptional<FBox> InBounds)
    -> void
{
    _SelectionBounds = MoveTemp(InBounds);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnPick(FOnPick InOnPick)
    -> void
{
    _OnPick = MoveTemp(InOnPick);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnRenderModeChanged(FOnRenderModeChanged InCallback)
    -> void
{
    _OnRenderModeChanged = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnGridChanged(FOnGridChanged InCallback)
    -> void
{
    _OnGridChanged = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnLabelsChanged(FOnLabelsChanged InCallback)
    -> void
{
    _OnLabelsChanged = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnDirectionGlyphScaleChanged(FOnDirectionGlyphScaleChanged InCallback)
    -> void
{
    _OnDirectionGlyphScaleChanged = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnIsolatedKeysChanged(FOnIsolatedKeysChanged InCallback)
    -> void
{
    _OnIsolatedKeysChanged = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_DragEnabled(bool InIsEnabled)
    -> void
{
    _DragEnabled = InIsEnabled;
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnDragArm(FOnDragArm InCallback)
    -> void
{
    _OnDragArm = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnDragRay(FOnDragRay InCallback)
    -> void
{
    _OnDragRay = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnDragPlaneShift(FOnDragPlaneShift InCallback)
    -> void
{
    _OnDragPlaneShift = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnDragRelease(FOnDragRelease InCallback)
    -> void
{
    _OnDragRelease = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnHover(FOnHover InCallback)
    -> void
{
    _OnHover = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_OnCommand(FOnCommand InCallback)
    -> void
{
    _OnCommand = MoveTemp(InCallback);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const
    -> FBox
{
    if (InTarget == ECkDebug3dFrameTarget::Selection && _SelectionBounds.IsSet())
    {
        return *_SelectionBounds;
    }
    const auto Target = _Target.Pin();
    return Target.IsValid() ? (InTarget == ECkDebug3dFrameTarget::Selection
                                   ? Target->Get_HighlightedBodyBounds().Get(FBox{ForceInit})
                                   : Target->Get_ContentBounds())
                            : FBox{ForceInit};
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_SelectionCenter() const
    -> TOptional<FVector>
{
    const auto Bounds = Get_FrameBounds(ECkDebug3dFrameTarget::Selection);
    return Bounds.IsValid != 0 ? TOptional<FVector>{Bounds.GetCenter()} : TOptional<FVector>{};
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_Capabilities() const
    -> ECkDebug3dViewportCapability
{
    return ECkDebug3dViewportCapability::Labels | ECkDebug3dViewportCapability::DirectionGlyphScale |
           ECkDebug3dViewportCapability::FollowSelection | ECkDebug3dViewportCapability::IsolateSelection |
           ECkDebug3dViewportCapability::FrameSelection;
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    On_Pick(const FCkDebug3dCursorRay& InRay)
    -> void
{
    const auto Hit = TryHit(InRay);
    if (Hit.IsSet())
    {
        Select(Hit->_Identity, InRay._IsAdditiveSelection);
    }
    else
    {
        ClearSelection(InRay._IsAdditiveSelection);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    TryHit(const FCkDebug3dCursorRay& InRay)
    -> TOptional<FCkDebug3dInteractionHit>
{
    auto Key = uint64{0};
    auto Point = FVector::ZeroVector;
    auto Distance = 0.0f;
    const auto Target = _Target.Pin();
    return Target.IsValid() && Target->TryPick_BodyHit(InRay._Origin, InRay._Direction, Key, Point, Distance)
               ? TOptional<FCkDebug3dInteractionHit>{FCkDebug3dInteractionHit{Key, Point, Distance}}
               : TOptional<FCkDebug3dInteractionHit>{};
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Select(uint64 InIdentity, bool InAdditive)
    -> void
{
    if (_OnPick)
    {
        _OnPick(InIdentity, InAdditive);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    ClearSelection(bool InAdditive)
    -> void
{
    if (_OnPick)
    {
        _OnPick({}, InAdditive);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    CanArmDrag(const FCkDebug3dInteractionHit& InHit) const
    -> bool
{
    return _DragEnabled && static_cast<bool>(_OnDragArm);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    ArmDrag(const FCkDebug3dInteractionHit& InHit)
    -> void
{
    if (_OnDragArm)
    {
        _OnDragArm(InHit._Identity, InHit._SurfacePoint);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    UpdateDrag(const FCkDebug3dCursorRay& InRay)
    -> void
{
    if (_OnDragRay)
    {
        _OnDragRay(InRay);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    ReleaseDrag()
    -> void
{
    if (_OnDragRelease)
    {
        _OnDragRelease();
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    ShiftDragPlane(float InDirection)
    -> void
{
    if (_OnDragPlaneShift)
    {
        _OnDragPlaneShift(InDirection);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    SetHover(TOptional<uint64> InIdentity)
    -> void
{
    if (_OnHover)
    {
        _OnHover(InIdentity);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Command(ECkDebug3dNeutralCommand InCommand)
    -> void
{
    if (_OnCommand)
    {
        _OnCommand(InCommand);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_RenderMode() const
    -> ECkDebug3dRenderMode
{
    const auto Target = _Target.Pin();
    return Target.IsValid() ? ck_jolt_debugger_3d_preview_adapter::To_Common(Target->Get_RenderMode())
                            : ECkDebug3dRenderMode::None;
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_RenderMode(ECkDebug3dRenderMode InMode)
    -> void
{
    const auto JoltMode = ck_jolt_debugger_3d_preview_adapter::To_Jolt(InMode);
    if (const auto Target = _Target.Pin(); Target.IsValid() && Target->Get_RenderMode() != JoltMode)
    {
        Target->Set_RenderMode(JoltMode);
        if (_OnRenderModeChanged)
        {
            _OnRenderModeChanged(JoltMode);
        }
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_ShowGrid() const
    -> bool
{
    return _ShowGrid;
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_ShowGrid(bool InIsOn)
    -> void
{
    if (_ShowGrid == InIsOn)
    {
        return;
    }
    _ShowGrid = InIsOn;
    if (_OnGridChanged)
    {
        _OnGridChanged(InIsOn);
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_ShowLabels() const
    -> bool
{
    const auto Target = _Target.Pin();
    return Target.IsValid() && Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Labels);
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_ShowLabels(bool InIsOn)
    -> void
{
    if (const auto Target = _Target.Pin();
        Target.IsValid() && Target->Get_IsDrawFlagSet(ECk_Jolt_DebugDrawFlags::Labels) != InIsOn)
    {
        auto Flags = Target->Get_DrawFlags();
        if (InIsOn)
        {
            EnumAddFlags(Flags, ECk_Jolt_DebugDrawFlags::Labels);
        }
        else
        {
            EnumRemoveFlags(Flags, ECk_Jolt_DebugDrawFlags::Labels);
        }
        Target->Set_DrawFlags(Flags);
        if (_OnLabelsChanged)
        {
            _OnLabelsChanged(InIsOn);
        }
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Get_DirectionGlyphScale() const
    -> float
{
    const auto Target = _Target.Pin();
    return Target.IsValid() ? Target->Get_DirectionGlyphScale() : 1.0f;
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_DirectionGlyphScale(float InScale)
    -> void
{
    if (const auto Target = _Target.Pin();
        Target.IsValid() && NOT FMath::IsNearlyEqual(Target->Get_DirectionGlyphScale(), InScale))
    {
        Target->Set_DirectionGlyphScale(InScale);
        if (_OnDirectionGlyphScaleChanged)
        {
            _OnDirectionGlyphScaleChanged(Target->Get_DirectionGlyphScale());
        }
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    Set_IsolatedKeys(const TArray<uint64>& InKeys)
    -> void
{
    if (const auto Target = _Target.Pin())
    {
        auto Keys = TSet<uint64>{};
        Keys.Reserve(InKeys.Num());
        for (const auto Key : InKeys)
        {
            Keys.Add(Key);
        }
        const auto& ExistingKeys = Target->Get_IsolatedBodies();
        const auto KeysChanged = ExistingKeys.Num() != Keys.Num() || NOT ExistingKeys.Includes(Keys);
        if (KeysChanged)
        {
            Target->Set_IsolatedBodies(MoveTemp(Keys));
            if (_OnIsolatedKeysChanged)
            {
                _OnIsolatedKeysChanged(InKeys);
            }
        }
    }
}
auto
    FCkJoltDebugger_3dPreviewAdapter::
    On_ViewportTeardown()
    -> void
{
    _Target.Reset();
    _OnPick = {};
    _OnRenderModeChanged = {};
    _OnGridChanged = {};
    _OnLabelsChanged = {};
    _OnDirectionGlyphScaleChanged = {};
    _OnIsolatedKeysChanged = {};
    _OnDragArm = {};
    _OnDragRay = {};
    _OnDragPlaneShift = {};
    _OnDragRelease = {};
    _OnHover = {};
    _OnCommand = {};
    _SelectionBounds.Reset();
}
