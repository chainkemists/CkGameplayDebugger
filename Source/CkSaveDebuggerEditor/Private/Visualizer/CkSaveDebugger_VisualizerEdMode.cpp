#include "CkSaveDebuggerEditor/Visualizer/CkSaveDebugger_VisualizerEdMode.h"

#include "CkSaveDebugger/Visualizer/CkSaveDebugger_Visualizer.h"

#include "CkCore/Macros/CkMacros.h"

#include <Editor.h>
#include <PrimitiveDrawInterface.h>
#include <SceneManagement.h>
#include <Textures/SlateIcon.h>

// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "Ck_SaveDebugger_VisualizerEdMode"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_HIT_PROXY(HCkSaveDebuggerViz_HitProxy, HHitProxy);

namespace ck_save_debugger_visualizer_edmode
{
    const FEditorModeID VisualizerModeId = TEXT("Ck.SaveDebugger.Visualizer");

    constexpr auto DiamondHalfHeight = 32.0f;
    constexpr auto SelectedScale = 1.6f;
    constexpr auto DiamondThickness = 1.5f;
    constexpr auto SelectedThickness = 3.0f;
    constexpr auto ChainDashSize = 12.0f;

    // 6 vertices of an octahedron in local space (apex up/down, square ring), 12 edges — a true 3D diamond, drawn
    // by hand so the marker needs no engine primitive and no billboarding.
    auto
    DrawDiamond(
        FPrimitiveDrawInterface& InPdi,
        const FTransform& InTransform,
        float InHalfHeight,
        const FLinearColor& InColor,
        float InThickness) -> void
    {
        const auto Center = InTransform.GetLocation();
        const auto Rotation = InTransform.GetRotation();
        const auto RingRadius = InHalfHeight * 0.65f;

        const FVector Vertices[6] =
        {
            Center + Rotation.RotateVector(FVector{0, 0,  InHalfHeight}),
            Center + Rotation.RotateVector(FVector{0, 0, -InHalfHeight}),
            Center + Rotation.RotateVector(FVector{ RingRadius, 0, 0}),
            Center + Rotation.RotateVector(FVector{0,  RingRadius, 0}),
            Center + Rotation.RotateVector(FVector{-RingRadius, 0, 0}),
            Center + Rotation.RotateVector(FVector{0, -RingRadius, 0}),
        };

        constexpr int32 Edges[12][2] =
        {
            {0, 2}, {0, 3}, {0, 4}, {0, 5},
            {1, 2}, {1, 3}, {1, 4}, {1, 5},
            {2, 3}, {3, 4}, {4, 5}, {5, 2},
        };

        for (const auto& Edge : Edges)
        { InPdi.DrawLine(Vertices[Edge[0]], Vertices[Edge[1]], InColor, SDPG_World, InThickness); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

UCk_SaveDebugger_VisualizerEdMode::UCk_SaveDebugger_VisualizerEdMode()
{
    Info = FEditorModeInfo(
        ck_save_debugger_visualizer_edmode::VisualizerModeId,
        LOCTEXT("CkSaveDebuggerVisualizerMode", "Save Debugger Visualizer"),
        FSlateIcon(),
        /*bVisibleInUI*/ false);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_SaveDebugger_VisualizerEdMode::
    Render(
        const FSceneView* InView,
        FViewport* InViewport,
        FPrimitiveDrawInterface* InPdi)
    -> void
{
    Super::Render(InView, InViewport, InPdi);

    if (InPdi == nullptr)
    { return; }

    if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
    { return; }

    const auto Rows = ck::save_debugger_viz::Get_Rows();
    if (NOT Rows.IsValid())
    { return; }

    const auto SelectedSavedId = ck::save_debugger_viz::Get_SelectedSavedId();

    // Chain lines below the diamonds and outside any hit proxy — a click on a line has no single row to mean.
    for (const auto& Row : *Rows)
    {
        if (NOT Rows->IsValidIndex(Row.OwnerRowIndex))
        { continue; }

        DrawDashedLine(
            InPdi,
            Row.WorldTransform.GetLocation(),
            (*Rows)[Row.OwnerRowIndex].WorldTransform.GetLocation(),
            CkStyle::TextMute(),
            ck_save_debugger_visualizer_edmode::ChainDashSize,
            SDPG_World);
    }

    for (const auto& Row : *Rows)
    {
        const auto IsSelected = Row.SavedId == SelectedSavedId;

        const auto Color = Row.HasProblems
            ? CkStyle::Err()
            : ck_save_debugger_model::Get_ProvenanceVisualizationColor(Row.Provenance);

        const auto HalfHeight = ck_save_debugger_visualizer_edmode::DiamondHalfHeight
            * (IsSelected ? ck_save_debugger_visualizer_edmode::SelectedScale : 1.0f);

        InPdi->SetHitProxy(new HCkSaveDebuggerViz_HitProxy{Row.SavedId});

        ck_save_debugger_visualizer_edmode::DrawDiamond(
            *InPdi, Row.WorldTransform, HalfHeight, Color,
            IsSelected
                ? ck_save_debugger_visualizer_edmode::SelectedThickness
                : ck_save_debugger_visualizer_edmode::DiamondThickness);

        InPdi->SetHitProxy(nullptr);

        if (IsSelected)
        {
            ck_save_debugger_visualizer_edmode::DrawDiamond(
                *InPdi, Row.WorldTransform,
                HalfHeight * 1.15f,
                CkStyle::Selection(),
                ck_save_debugger_visualizer_edmode::DiamondThickness);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

bool
    UCk_SaveDebugger_VisualizerEdMode::
    HandleClick(
        FEditorViewportClient* InViewportClient,
        HHitProxy* InHitProxy,
        const FViewportClick& InClick)
{
    if (InHitProxy != nullptr && InHitProxy->IsA(HCkSaveDebuggerViz_HitProxy::StaticGetType()))
    {
        ck::save_debugger_viz::Notify_RowClicked(static_cast<HCkSaveDebuggerViz_HitProxy*>(InHitProxy)->SavedId);
        return true;
    }

    return Super::HandleClick(InViewportClient, InHitProxy, InClick);
}

// --------------------------------------------------------------------------------------------------------------------

#undef LOCTEXT_NAMESPACE
