#include "CkOptimizationDebuggerEditor/Heatmap/CkPerfLab_HeatmapEdMode.h"

#include "CkPerfLab/Heatmap/CkPerfLab_HeatmapSlot.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Macros/CkMacros.h"

#include <Editor.h>
#include <Engine/World.h>
#include <PrimitiveDrawInterface.h>
#include <SceneManagement.h>
#include <Textures/SlateIcon.h>

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "Ck_PerfLab_HeatmapEdMode"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_HIT_PROXY(HCkPerfLabHeatmap_HitProxy, HHitProxy);

namespace ck_perf_lab_heatmap_edmode
{
    constexpr auto k_BaseRadius      = 40.0f;
    constexpr auto k_MaxRadiusScale  = 2.5f;
    constexpr auto k_Thickness       = 2.0f;
    constexpr auto k_SelectedScale   = 1.5f;
    constexpr auto k_StemHeight      = 90.0f;

    // The shape axis, by severity. Fewer sides reads as sharper and more urgent, and the three are far enough apart
    // to tell at a glance from across a level.
    constexpr auto k_TriangleSides = 3;
    constexpr auto k_DiamondSides  = 4;
    constexpr auto k_CircleSides   = 16;

    /**
     * Size reads the over-budget multiple, clamped so one catastrophic position cannot produce a marker that fills
     * the viewport and hides the rest of the level.
     */
    auto
        Get_Radius(
            float InOverBudgetRatio)
        -> float
    {
        return k_BaseRadius * FMath::Clamp(InOverBudgetRatio, 1.0f, k_MaxRadiusScale);
    }

    /**
     * How many sides the ring has, by severity — the redundant encoding that keeps the heatmap readable when the
     * colour axis is not. Colour alone would exclude a deuteranopic reader entirely.
     */
    auto
        Get_SideCount(
            ECk_PerfLab_Severity InSeverity)
        -> int32
    {
        switch (InSeverity)
        {
            case ECk_PerfLab_Severity::Critical: return k_TriangleSides;
            case ECk_PerfLab_Severity::Major:    return k_DiamondSides;
            case ECk_PerfLab_Severity::Minor:    return k_CircleSides;
        }

        return k_CircleSides;
    }

    auto
        Draw_Ring(
            FPrimitiveDrawInterface& InPdi,
            const FVector& InCentre,
            float InRadius,
            int32 InSides,
            const FLinearColor& InColor,
            float InThickness)
        -> void
    {
        const auto Step = 2.0f * PI / static_cast<float>(FMath::Max(3, InSides));

        auto Previous = InCentre + FVector{InRadius, 0.0, 0.0};

        for (auto Index = 1; Index <= InSides; ++Index)
        {
            const auto Angle = Step * static_cast<float>(Index);

            const auto Current = InCentre + FVector
            {
                static_cast<double>(InRadius * FMath::Cos(Angle)),
                static_cast<double>(InRadius * FMath::Sin(Angle)),
                0.0
            };

            InPdi.DrawLine(Previous, Current, InColor, SDPG_Foreground, InThickness);

            Previous = Current;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

UCk_PerfLab_HeatmapEdMode::UCk_PerfLab_HeatmapEdMode()
{
    Info = FEditorModeInfo(
        ck::perf_lab::heatmap::k_EdModeId,
        LOCTEXT("CkPerfLabHeatmapMode", "PerfLab Heatmap"),
        FSlateIcon(),
        /*bVisibleInUI*/ false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_PerfLab_HeatmapEdMode::
    Render(
        const FSceneView* InView,
        FViewport* InViewport,
        FPrimitiveDrawInterface* InPdi)
    -> void
{
    Super::Render(InView, InViewport, InPdi);

    using namespace ck_perf_lab_heatmap_edmode;

    if (ck::Is_NOT_Valid(InPdi, ck::IsValid_Policy_NullptrOnly{}))
    {
        return;
    }

    // Debug drawing is globally suppressible, and a performance overlay is exactly the kind of thing a streamer
    // does not want on screen.
    if (ck::debug_draw::Is_SuppressedForStreamerMode())
    {
        return;
    }

    const auto Snapshot = ck::perf_lab::heatmap::Get_Published();

    if (NOT Snapshot._IsEnabled || Snapshot._Markers.IsEmpty())
    {
        return;
    }

    // Refuse to draw a session captured against a different level. The coordinates would place markers somewhere
    // arbitrary in this one, and they would look exactly as trustworthy as real measurements.
    const auto* World = ck::IsValid(GEditor) ? GEditor->GetEditorWorldContext().World() : nullptr;

    if (ck::Is_NOT_Valid(World) || World->GetOutermost()->GetPathName() != Snapshot._MapPath)
    {
        return;
    }

    const auto Selected = ck::perf_lab::heatmap::Get_SelectedPositionId();

    for (const auto& Marker : Snapshot._Markers)
    {
        const auto IsSelected = NOT Selected.IsEmpty() && Marker._PositionId == Selected;

        // One ramp for the whole debugger suite, so this heatmap cannot drift from the colours every other tool
        // uses for the same idea.
        const auto Color  = ck::debug_axes::Get_HeatColor(Marker._Normalised);
        const auto Radius = Get_Radius(Marker._OverBudgetRatio) * (IsSelected ? k_SelectedScale : 1.0f);
        const auto Sides  = Get_SideCount(Marker._Severity);

        // Only on the hit-testing pass. Allocating a proxy in the ordinary draw pass copies an FString and takes a
        // GLOBAL critical section twice per marker, per viewport, per frame — for an object immediately discarded.
        const auto IsHitTesting = InPdi->IsHitTesting();

        if (IsHitTesting)
        {
            InPdi->SetHitProxy(new HCkPerfLabHeatmap_HitProxy{Marker._PositionId});
        }

        Draw_Ring(*InPdi, Marker._Location, Radius, Sides, Color,
            IsSelected ? k_Thickness * 2.0f : k_Thickness);

        // A stem, so a marker lying flat on the floor is still findable from a low camera angle.
        InPdi->DrawLine(
            Marker._Location,
            Marker._Location + FVector{0.0, 0.0, static_cast<double>(k_StemHeight)},
            Color, SDPG_Foreground, k_Thickness);

        if (IsHitTesting)
        {
            InPdi->SetHitProxy(nullptr);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

bool
    UCk_PerfLab_HeatmapEdMode::
    HandleClick(
        FEditorViewportClient* InViewportClient,
        HHitProxy* InHitProxy,
        const FViewportClick& InClick)
{
    if (ck::Is_NOT_Valid(InHitProxy, ck::IsValid_Policy_NullptrOnly{}) ||
        NOT InHitProxy->IsA(HCkPerfLabHeatmap_HitProxy::StaticGetType()))
    {
        return Super::HandleClick(InViewportClient, InHitProxy, InClick);
    }

    // Pushed back through the same slot the markers came from, so the page learns about the click without this mode
    // knowing the page exists.
    const auto* Proxy = static_cast<HCkPerfLabHeatmap_HitProxy*>(InHitProxy);
    ck::perf_lab::heatmap::Set_SelectedPositionId(Proxy->PositionId);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
