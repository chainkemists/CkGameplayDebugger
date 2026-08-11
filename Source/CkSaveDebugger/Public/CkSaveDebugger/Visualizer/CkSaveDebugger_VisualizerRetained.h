#pragma once

#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "CkSnapshot/Inspection/CkSnapshot_Inspection_Data.h"

#include "Templates/SharedPointer.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;

// --------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/** Retained editor-ECS visuals for the save visualizer, complementing the EdMode's immediate-mode diamonds:
 *
 *  - CONSTRUCTION PREVIEWS — a plain script row's real EntityScript is spawned in the editor registry with its
 *    decoded spawn params (handles nulled — raw saved ids would alias unrelated editor entities), exactly the
 *    ACk_EntitySpawner_UE preview mechanism. Whatever Construct composes renders; ConstructSpawned children come
 *    back for free because replayed construction recreates them. BeginPlay never runs in editor worlds, so
 *    BeginPlay-composed visuals stay absent by design.
 *  - MESH GHOSTS — a bridged actor row's class is read PASSIVELY (native CDO components + the Blueprint SCS chain
 *    with inherited-component overrides) and its static meshes render as shared-ISM instances at the saved
 *    transform. Never previewed live: a WithActor Construct ensures without an owning actor, and spawning real
 *    actors would dirty the level.
 *  - SELECTION GIZMO — one FCkDebug_PmgGizmoSet at the selected row's saved transform.
 *
 *  Everything hangs off one root entity under the editor world's transient (cascade-destroy, inherited
 *  FTag_EditorOnlyEntity), created and destroyed only while Get_IsEditorEcsMutationSafe. */
namespace ck::save_debugger_viz_retained
{
    struct FRebuildStats
    {
        int32 PreviewCount = 0;
        int32 GhostMeshCount = 0;
        // Rows whose class could not resolve, whose class exposed no static-mesh template, or whose params blob
        // failed to decode (the preview still spawns with empty params in that last case).
        int32 UnresolvedClassCount = 0;
        int32 GhostsWithoutMeshCount = 0;
        int32 ParamsDecodeFailureCount = 0;

        // First few unresolved class paths, so the status line can say WHICH content this editor cannot see —
        // the difference between "bug" and "this save belongs to another project" at a glance.
        TArray<FString> UnresolvedClassSamples;
    };

    // ----------------------------------------------------------------------------------------------------------------

    /** Destroys the previous retained set and builds one for the given rows. Unset when the editor ECS is not
     *  mutation-safe right now (PIE transition) — nothing was built and the caller should say so. */
    CKSAVEDEBUGGER_API auto
    Rebuild(
        UWorld* InEditorWorld,
        const FCk_SnapshotInspection_Document& InDocument,
        const TArray<FCkSaveDebugger_VisualizationRow>& InRows) -> TOptional<FRebuildStats>;

    CKSAVEDEBUGGER_API auto
    Clear() -> void;

    /** Places the selection gizmo, or removes it when InTransform is unset. */
    CKSAVEDEBUGGER_API auto
    Update_SelectionGizmo(
        UWorld* InEditorWorld,
        const TOptional<FTransform>& InTransform) -> void;
}

#endif // WITH_EDITOR

// --------------------------------------------------------------------------------------------------------------------
