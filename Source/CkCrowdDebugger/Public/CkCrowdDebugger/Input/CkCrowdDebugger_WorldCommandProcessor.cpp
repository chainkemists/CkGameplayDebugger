#include "CkCrowdDebugger_WorldCommandProcessor.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Utils_FlatShapes.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowddebugger_worldcommand
{
    constexpr auto TraceLengthCm    = 100'000.0f;   // 1 km command ray
    constexpr auto PingOuterCm      = 60.0f;
    constexpr auto PingInnerCm      = 45.0f;
    constexpr auto PingSegments     = 32;
    constexpr auto PingDurationSecs = 1.5f;
    constexpr auto PingZLiftCm      = 4.0f;         // keep the ring off z-fighting with the floor
}

// --------------------------------------------------------------------------------------------------------------------

FCkCrowdDebugger_WorldCommandProcessor::FCkCrowdDebugger_WorldCommandProcessor(
    TWeakPtr<FCkCrowdDebugger_ViewModel> InViewModel)
    : _ViewModel(MoveTemp(InViewModel))
{
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkCrowdDebugger_WorldCommandProcessor::
    HandleMouseButtonDownEvent(
        FSlateApplication&   InSlateApp,
        const FPointerEvent& InMouseEvent) -> bool
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _RmbDown    = true;
        _RmbDragged = false;
        _RmbDownPos = InMouseEvent.GetScreenSpacePosition();
    }

    return false;   // observe only — never starve the viewport's own RMB handling
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkCrowdDebugger_WorldCommandProcessor::
    HandleMouseMoveEvent(
        FSlateApplication&   InSlateApp,
        const FPointerEvent& InMouseEvent) -> bool
{
    if (_RmbDown && NOT _RmbDragged)
    {
        const auto Threshold = InSlateApp.GetDragTriggerDistance();
        const auto DistSq    = FVector2D::DistSquared(InMouseEvent.GetScreenSpacePosition(), _RmbDownPos);
        if (DistSq > Threshold * Threshold)
        { _RmbDragged = true; }   // camera-look drag, not a command click
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkCrowdDebugger_WorldCommandProcessor::
    HandleMouseButtonUpEvent(
        FSlateApplication&   InSlateApp,
        const FPointerEvent& InMouseEvent) -> bool
{
    if (InMouseEvent.GetEffectingButton() != EKeys::RightMouseButton)
    { return false; }

    const auto WasCleanClick = _RmbDown && NOT _RmbDragged;
    _RmbDown    = false;
    _RmbDragged = false;

    if (WasCleanClick)
    {
        DoTryCommand(InMouseEvent.GetScreenSpacePosition());
    }

    return false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkCrowdDebugger_WorldCommandProcessor::
    DoTryCommand(
        const FVector2D& InScreenPos) -> void
{
    using namespace ck_crowddebugger_worldcommand;

    // Ejected-only: while possessed the game owns the mouse (and usually hides it).
    if (NOT ck::DebugViewportView::Get_IsEjected())
    { return; }

    const auto ViewModel = _ViewModel.Pin();
    if (NOT ViewModel.IsValid())
    { return; }

    auto SelectedHandle = ViewModel->Get_SelectedHandle();
    auto Agent = UCk_Utils_CrowdAgent_UE::Cast(SelectedHandle);
    if (ck::Is_NOT_Valid(Agent))
    { return; }

    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(SelectedHandle);
    if (ck::Is_NOT_Valid(World))
    { return; }

    auto Origin    = FVector::ZeroVector;
    auto Direction = FVector::ForwardVector;
    if (NOT ck::DebugViewportView::Deproject(World, InScreenPos, Origin, Direction))
    { return; }

    auto Destination = FVector::ZeroVector;
    auto Hit = FHitResult{};
    if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * TraceLengthCm, ECC_Visibility))
    {
        Destination = Hit.ImpactPoint;
    }
    else
    {
        // No geometry under the cursor — intersect the agent's own ground plane.
        const auto* Sel = ViewModel->Get_SelectedSnapshot();
        if (Sel == nullptr || FMath::IsNearlyZero(Direction.Z))
        { return; }

        const auto T = (Sel->Position.Z - Origin.Z) / Direction.Z;
        if (T <= 0.0)
        { return; }
        Destination = Origin + Direction * T;
    }

    // RTS feel: commanding IS taking control — no separate arming click.
    if (NOT UCk_Utils_CrowdAgent_UE::Get_HasDebugOverride(Agent))
    { UCk_Utils_CrowdAgent_UE::Request_SetDebugOverride(Agent, true); }

    auto MoveReq = FCk_Request_CrowdAgent_MoveTo(Destination);
    UCk_Utils_CrowdAgent_UE::Request_MoveTo(Agent, MoveReq);

    // Destination acknowledgment ping — transient PMG ring, self-destroys.
    UCk_Utils_Pmg_FlatShapes::DrawFilledRing(
        World,
        Destination + FVector{0.0f, 0.0f, PingZLiftCm},
        PingOuterCm,
        PingInnerCm,
        PingSegments,
        FLinearColor{0.15f, 1.0f, 0.35f, 0.85f},
        /*InDrawLines=*/false,
        /*InLineThickness=*/2.0f,
        ECk_Plane_Axis::XY,
        PingDurationSecs);
}

// --------------------------------------------------------------------------------------------------------------------
