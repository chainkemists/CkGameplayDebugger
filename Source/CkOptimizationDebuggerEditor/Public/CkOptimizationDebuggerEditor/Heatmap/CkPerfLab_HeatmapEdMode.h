#pragma once

#include <CoreMinimal.h>

#include <HitProxies.h>
#include <Tools/LegacyEdModeWidgetHelpers.h>

#include "CkPerfLab_HeatmapEdMode.generated.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
struct FViewportClick;

// --------------------------------------------------------------------------------------------------------------------

/** One clickable measurement marker. Carries the position id — a string, never a handle — so a viewport click can
 *  select the matching row in the open Performance page without anything live crossing between them. */
struct HCkPerfLabHeatmap_HitProxy : HHitProxy
{
    DECLARE_HIT_PROXY(CKOPTIMIZATIONDEBUGGEREDITOR_API);

    FString PositionId;

    explicit HCkPerfLabHeatmap_HitProxy(FString InPositionId)
        : HHitProxy(HPP_UI)
        , PositionId(MoveTemp(InPositionId))
    {}
};

// --------------------------------------------------------------------------------------------------------------------

/**
 * Hidden, auto-discovered EdMode that draws measured positions over the level-editor viewport.
 *
 * Stateless by design, following the save visualizer: every draw pulls the immutable snapshot from
 * ck::perf_lab::heatmap and clicks are pushed back through the same slot, so this mode holds nothing that could go
 * stale and nothing that could outlive the window.
 *
 * It spawns no actors, adds no components and opens no transaction — the level is never modified to show a
 * measurement of it. It also refuses to draw when the published session belongs to a different map, because markers
 * placed by coordinates from another level would look exactly as authoritative as real ones.
 */
UCLASS()
class CKOPTIMIZATIONDEBUGGEREDITOR_API UCk_PerfLab_HeatmapEdMode : public UBaseLegacyWidgetEdMode
{
    GENERATED_BODY()

public:
    UCk_PerfLab_HeatmapEdMode();

    auto
    Render(
        const FSceneView* InView,
        FViewport* InViewport,
        FPrimitiveDrawInterface* InPdi) -> void override;

    virtual bool HandleClick(
        FEditorViewportClient* InViewportClient,
        HHitProxy* InHitProxy,
        const FViewportClick& InClick) override;
};

// --------------------------------------------------------------------------------------------------------------------
