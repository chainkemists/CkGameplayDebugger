#pragma once

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "Containers/UnrealString.h"

// --------------------------------------------------------------------------------------------------------------------

/** What "Go To" did. A failure is ordinary — an actor whose sub-level has since been unloaded cannot be focused, and
 *  saying so beats a button that appears to do nothing. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_NavigationResult
{
    bool Succeeded = false;

    FString Message;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_navigation
{
    /** What "Go To" WILL do for this target, in the words the button's tooltip uses. Pure — this is the part of
     *  navigation a spec can pin without an editor, and the part that must never disagree with what the action does:
     *  an asset finding syncs the Content Browser to the ASSET, never to the actor that happens to place it. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_NavigationDescription(
        const FCkOptimizationDebugger_Target& InTarget) -> FString;

    /** Whether the target names something navigation can reach at all: a path for an asset or an actor, a section
     *  name for a project setting. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Can_Navigate(
        const FCkOptimizationDebugger_Target& InTarget) -> bool;

    /** Takes the reader to the object that owns the problem: Content Browser sync for an asset, selection plus
     *  viewport framing for an actor, the matching Project Settings page for a settings finding.
     *
     *  Outside the editor every route fails with a message saying so. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Navigate_ToFinding(
        const FCkOptimizationDebugger_FindingRow& InFinding) -> FCkOptimizationDebugger_NavigationResult;

    /** The same three routes, over a target that no finding produced — a memory-analyzer row names an asset the
     *  reader wants to see, and it is the SAME action with the same failure modes. Kept as one implementation so a
     *  second caller cannot invent a second way of syncing the Content Browser. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Navigate_ToTarget(
        const FCkOptimizationDebugger_Target& InTarget) -> FCkOptimizationDebugger_NavigationResult;

    /** What "Open Asset" WILL do, in the words the button's tooltip uses. Pure — the spec-pinnable half, and it
     *  must never disagree with what the action does. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Get_OpenAssetDescription(
        const FCkOptimizationDebugger_Target& InTarget) -> FString;

    /** Only an Asset-kind target with a path can be opened. An actor is placed BY an asset rather than being one,
     *  and a settings section is a page, so neither has an asset editor to open. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Can_OpenAsset(
        const FCkOptimizationDebugger_Target& InTarget) -> bool;

    /** Opens the asset's own editor. This one LOADS the asset — that is what opening an editor means, and unlike a
     *  scan it is a thing the reader pressed a button to ask for. Outside the editor it fails saying so. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Open_TargetAsset(
        const FCkOptimizationDebugger_Target& InTarget) -> FCkOptimizationDebugger_NavigationResult;
}

// --------------------------------------------------------------------------------------------------------------------
