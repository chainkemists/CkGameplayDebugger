#pragma once

#include <CoreMinimal.h>

#if WITH_EDITOR
#include <HitProxies.h>
#include <Tools/LegacyEdModeWidgetHelpers.h>
#endif

#include "CkSaveDebugger_VisualizerEdMode.generated.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
struct FViewportClick;

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/** One clickable diamond of the save visualizer. Carries the raw saved id — the only entity reference this module
 *  ever holds — so a viewport click can select the matching row in the open Save Debugger window. */
struct HCkSaveDebuggerViz_HitProxy : HHitProxy
{
    DECLARE_HIT_PROXY(CKSAVEDEBUGGER_API);

    uint32 SavedId;

    explicit HCkSaveDebuggerViz_HitProxy(uint32 InSavedId)
        : HHitProxy(HPP_UI)
        , SavedId(InSavedId)
    {}
};

// --------------------------------------------------------------------------------------------------------------------

/** Hidden, auto-discovered EdMode that draws the published save-visualization rows into the level-editor viewport:
 *  a wire diamond per placed entity, dashed owner-chain lines, and a highlight on the window's selected row.
 *  Stateless by design (the CkVoxelNavPreview_EdMode shape): every draw pulls the immutable snapshot from
 *  ck::save_debugger_viz, and clicks are pushed back through the same slot. Subclasses UBaseLegacyWidgetEdMode,
 *  not UEdMode, for the overridable Render/HandleClick. */
UCLASS()
class CKSAVEDEBUGGER_API UCk_SaveDebugger_VisualizerEdMode : public UBaseLegacyWidgetEdMode
{
    GENERATED_BODY()

public:
    static const FEditorModeID EM_CkSaveDebuggerVisualizerModeId;

    UCk_SaveDebugger_VisualizerEdMode();

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

#endif // WITH_EDITOR

// --------------------------------------------------------------------------------------------------------------------
