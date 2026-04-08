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
