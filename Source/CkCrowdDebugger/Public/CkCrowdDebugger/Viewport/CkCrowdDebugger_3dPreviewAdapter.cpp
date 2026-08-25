#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dPreviewAdapter.h"

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_Target(TSharedPtr<FCk_DebugScene_Target> InTarget)
    -> void
{
    _Target = MoveTemp(InTarget);
    if (_Target.IsValid())
    {
        Set_RenderMode(_RenderMode);
        PublishGrid();
    }
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Reconcile(const FCkCrowdDebugger_3dSceneSnapshot& InSnapshot)
    -> bool
{
    if (NOT _Target.IsValid())
    {
        return false;
    }
    if (NOT _Scene.Reconcile(InSnapshot, *_Target))
    {
        return false;
    }
    _LastSnapshot = InSnapshot;
    _SelectedIdentity = InSnapshot._SelectedIdentity;
    PublishGrid();
    return true;
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Reset_World()
    -> void
{
    if (_Target.IsValid())
    {
        _Scene.Reset_ForWorldChange(*_Target);
    }
    _LastSnapshot.Reset();
    _SelectedIdentity.Reset();
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_OnSelected(FOnSelected InOnSelected)
    -> void
{
    _OnSelected = MoveTemp(InOnSelected);
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_OnCommandAtPoint(FOnCommandAtPoint InOnCommand)
    -> void
{
    _OnCommandAtPoint = MoveTemp(InOnCommand);
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Command_AtRay(const FCkDebug3dCursorRay& InRay)
    -> void
{
    if (NOT _Target.IsValid() || NOT _OnCommandAtPoint)
    {
        return;
    }

    // Deliberately the RAW pick, not TryHit: TryHit resolves to an agent identity and drops
    // everything else, but a move-to destination is precisely the geometry that is NOT an agent
    // (navmesh, ribbons). The surface point is the destination; projecting it onto the navmesh is
    // the command's job, not the viewport's.
    const auto Pick = _Target->TryPick(InRay._Origin, InRay._Direction);
    if (NOT Pick.IsSet())
    {
        return;
    }
    _OnCommandAtPoint(Pick->Get_HitPoint());
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Get_FrameBounds(ECkDebug3dFrameTarget InTarget) const
    -> FBox
{
    if (NOT _Target.IsValid())
    {
        return FBox{ForceInit};
    }
    if (InTarget == ECkDebug3dFrameTarget::Selection)
    {
        return _Scene.Get_SelectionBounds(*_Target).Get(FBox{ForceInit});
    }
    return _Target->Get_ContentBounds();
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Get_SelectionCenter() const
    -> TOptional<FVector>
{
    const auto Bounds = Get_FrameBounds(ECkDebug3dFrameTarget::Selection);
    return Bounds.IsValid != 0 ? TOptional<FVector>{Bounds.GetCenter()} : TOptional<FVector>{};
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Get_Capabilities() const
    -> ECkDebug3dViewportCapability
{
    return ECkDebug3dViewportCapability::FrameSelection | ECkDebug3dViewportCapability::FollowSelection |
           ECkDebug3dViewportCapability::IsolateSelection;
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
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
    FCkCrowdDebugger_3dPreviewAdapter::
    TryHit(const FCkDebug3dCursorRay& InRay)
    -> TOptional<FCkDebug3dInteractionHit>
{
    if (NOT _Target.IsValid())
    {
        return {};
    }
    const auto Pick = _Target->TryPick(InRay._Origin, InRay._Direction);
    if (NOT Pick.IsSet())
    {
        return {};
    }
    const auto Resolution = _Scene.Resolve_Pick(*Pick);
    return Resolution.IsSet() ? TOptional<FCkDebug3dInteractionHit>{FCkDebug3dInteractionHit{
                                    Resolution->_Identity, Pick->Get_HitPoint(), Pick->Get_Distance()}}
                              : TOptional<FCkDebug3dInteractionHit>{};
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Select(uint64 InIdentity, bool InAdditive)
    -> void
{
    if (NOT _Target.IsValid() || NOT _Scene.TrySelect_Identity(InIdentity, *_Target))
    {
        return;
    }
    _SelectedIdentity = InIdentity;
    if (_OnSelected)
    {
        _OnSelected(_Scene.Get_CurrentAgentIndex(InIdentity));
    }
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    ClearSelection(bool InAdditive)
    -> void
{
    _SelectedIdentity.Reset();
    if (_OnSelected)
    {
        _OnSelected({});
    }
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Get_RenderMode() const
    -> ECkDebug3dRenderMode
{
    return _RenderMode;
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_RenderMode(ECkDebug3dRenderMode InMode)
    -> void
{
    _RenderMode = InMode;
    if (NOT _Target.IsValid())
    {
        return;
    }
    _Target->Set_WireframeMode(InMode == ECkDebug3dRenderMode::All
                                   ? ECk_DebugScene_WireframeMode::All
                                   : (InMode == ECkDebug3dRenderMode::TransparentOnly
                                          ? ECk_DebugScene_WireframeMode::TransparentOnly
                                          : ECk_DebugScene_WireframeMode::None));
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Get_ShowGrid() const
    -> bool
{
    return _ShowGrid;
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_ShowGrid(bool InIsOn)
    -> void
{
    if (_ShowGrid == InIsOn)
    {
        return;
    }
    _ShowGrid = InIsOn;
    PublishGrid();
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    Set_IsolatedKeys(const TArray<uint64>& InKeys)
    -> void
{
    if (NOT _LastSnapshot.IsSet() || NOT _Target.IsValid())
    {
        return;
    }
    auto Filtered = *_LastSnapshot;
    if (NOT InKeys.IsEmpty())
    {
        Filtered._Agents.RemoveAll([&InKeys](const FCkCrowdDebugger_3dAgentSnapshot& InAgent)
        {
            return NOT InKeys.Contains(InAgent._Identity);
        });
        if (Filtered._SelectedIdentity.IsSet() && NOT InKeys.Contains(*Filtered._SelectedIdentity))
        {
            Filtered._SelectedIdentity.Reset();
        }
    }
    _Scene.Reconcile(Filtered, *_Target);
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    On_ViewportTeardown()
    -> void
{
    if (_Target.IsValid())
    {
        _Target->HideAll();
    }
    _Target.Reset();
    _LastSnapshot.Reset();
    _SelectedIdentity.Reset();
    _OnSelected = {};
}

auto
    FCkCrowdDebugger_3dPreviewAdapter::
    PublishGrid()
    -> void
{
    static const auto GridChannel = FName{TEXT("CkCrowd.Grid")};
    if (NOT _Target.IsValid())
    {
        return;
    }
    if (NOT _ShowGrid)
    {
        _Target->Clear_LineChannel(GridChannel);
        return;
    }

    auto Bounds = _Target->Get_ContentBounds();
    if (Bounds.IsValid == 0)
    {
        Bounds = FBox{FVector{-1000.0, -1000.0, 0.0}, FVector{1000.0, 1000.0, 0.0}};
    }
    const auto Extent = FMath::Max(Bounds.GetExtent().GetMax(), 1000.0);
    const auto Step = FMath::Max(FMath::GridSnap(Extent / 10.0, 100.0), 100.0);
    const auto Limit = FMath::CeilToDouble(Extent / Step) * Step;
    auto Lines = TArray<FCk_DebugScene_Line>{};
    const auto Color = FLinearColor{0.18f, 0.22f, 0.28f, 0.55f};
    for (auto Coordinate = -Limit; Coordinate <= Limit; Coordinate += Step)
    {
        Lines.Add({FVector{-Limit, Coordinate, 0.0}, FVector{Limit, Coordinate, 0.0}, Color, 0.0f});
        Lines.Add({FVector{Coordinate, -Limit, 0.0}, FVector{Coordinate, Limit, 0.0}, Color, 0.0f});
    }
    _Target->Set_LineChannel(GridChannel, MoveTemp(Lines));
}
