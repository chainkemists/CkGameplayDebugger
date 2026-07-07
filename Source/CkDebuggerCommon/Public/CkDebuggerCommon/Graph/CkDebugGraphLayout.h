#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Generic Sugiyama layered graph layout.
// Extracted from the SM debugger's compound-aware Sugiyama implementation.
// Works on any DAG represented as nodes + directed edges.
//
// Phases:
//   1. BFS layer assignment (directed or undirected)
//   2. Dummy vertex insertion for multi-rank edges
//   3. Barycenter crossing reduction
//   4. Coordinate assignment
// --------------------------------------------------------------------------------------------------------------------

struct FCkDebugGraphLayoutNode
{
	int32 Index = INDEX_NONE;

	// Optional — width hint in layout units for this node. When any node in
	// the graph provides a positive width, the layout switches from uniform
	// columns (R * SpacingX) to per-column sizing where each column is at
	// least as wide as its widest node and columns are separated by
	// SpacingX. Leaving all widths at 0 preserves the legacy behavior.
	int32 EstimatedWidth = 0;

	// Optional — height hint in layout units for this node. A node with a
	// positive height occupies max(SpacingY, Height + gap) of vertical space
	// in its column, pushing subsequent rows down instead of overlapping
	// them. Leaving all heights at 0 preserves the legacy uniform-row
	// behavior (origin-to-origin SpacingY).
	int32 EstimatedHeight = 0;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDebugGraphLayoutEdge
{
	int32 From = INDEX_NONE;
	int32 To = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebugGraphLayoutParams
{
	int32 SpacingX = 300;
	int32 SpacingY = 100;
	int32 CrossingReductionPasses = 4;
	bool IsDirectedBFS = true;
	int32 InitialNodeIndex = INDEX_NONE;
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkDebugGraphLayoutResult
{
	TMap<int32, FIntPoint> Positions;
};

// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API FCkDebugGraphLayout
{
public:
	static auto
	ComputeLayout(
		const TArray<FCkDebugGraphLayoutNode>& InNodes,
		const TArray<FCkDebugGraphLayoutEdge>& InEdges,
		const FCkDebugGraphLayoutParams& InParams)
		-> FCkDebugGraphLayoutResult;
};

// --------------------------------------------------------------------------------------------------------------------
