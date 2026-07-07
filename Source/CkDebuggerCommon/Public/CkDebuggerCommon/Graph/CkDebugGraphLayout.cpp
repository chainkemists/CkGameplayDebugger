#include "CkDebuggerCommon/Graph/CkDebugGraphLayout.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkDebugGraphLayout::
	ComputeLayout(
		const TArray<FCkDebugGraphLayoutNode>& InNodes,
		const TArray<FCkDebugGraphLayoutEdge>& InEdges,
		const FCkDebugGraphLayoutParams& InParams)
	-> FCkDebugGraphLayoutResult
{
	auto Result = FCkDebugGraphLayoutResult{};

	auto NodeCount = InNodes.Num();
	if (NodeCount == 0)
	{ return Result; }

	// Build index mapping: external Index -> internal 0-based index
	auto ExternalToInternal = TMap<int32, int32>{};
	for (auto i = 0; i < NodeCount; ++i)
	{ ExternalToInternal.Add(InNodes[i].Index, i); }

	// ================================================================================================================
	// Phase 1: Layer assignment via BFS
	// ================================================================================================================

	auto Ranks = TArray<int32>{};
	Ranks.SetNum(NodeCount);
	for (auto& R : Ranks) { R = -1; }

	auto InitialInternal = 0;
	if (InParams.InitialNodeIndex != INDEX_NONE)
	{
		auto* Found = ExternalToInternal.Find(InParams.InitialNodeIndex);
		if (Found) { InitialInternal = *Found; }
	}

	Ranks[InitialInternal] = 0;
	auto Queue = TArray<int32>{ InitialInternal };

	for (auto Head = 0; Head < Queue.Num(); ++Head)
	{
		auto Current = Queue[Head];

		for (auto& Edge : InEdges)
		{
			auto* InternalSrc = ExternalToInternal.Find(Edge.From);
			auto* InternalDst = ExternalToInternal.Find(Edge.To);
			if (!InternalSrc || !InternalDst) { continue; }

			auto Neighbor = -1;
			if (InParams.IsDirectedBFS)
			{
				if (*InternalSrc == Current) { Neighbor = *InternalDst; }
			}
			else
			{
				if (*InternalSrc == Current) { Neighbor = *InternalDst; }
				else if (*InternalDst == Current) { Neighbor = *InternalSrc; }
			}

			if (Neighbor >= 0 && Neighbor < Ranks.Num() && Ranks[Neighbor] < 0)
			{
				Ranks[Neighbor] = Ranks[Current] + 1;
				Queue.Add(Neighbor);
			}
		}
	}

	for (auto& R : Ranks) { if (R < 0) { R = 0; } }

	auto MaxRank = 0;
	for (auto R : Ranks) { MaxRank = FMath::Max(MaxRank, R); }

	// ================================================================================================================
	// Phase 2: Dummy vertex insertion for multi-rank edges
	// ================================================================================================================

	struct FLayoutNode { int32 OriginalIndex; int32 Rank; };
	auto LayoutNodes = TArray<FLayoutNode>{};
	LayoutNodes.Reserve(NodeCount * 2);

	for (auto i = 0; i < NodeCount; ++i)
	{ LayoutNodes.Add({ i, Ranks[i] }); }

	struct FLayoutEdge { int32 From; int32 To; };
	auto LayoutEdges = TArray<FLayoutEdge>{};

	for (auto& Edge : InEdges)
	{
		auto* InternalSrc = ExternalToInternal.Find(Edge.From);
		auto* InternalDst = ExternalToInternal.Find(Edge.To);
		if (!InternalSrc || !InternalDst) { continue; }

		auto S = *InternalSrc;
		auto T = *InternalDst;
		if (S == T) { continue; }

		auto RankS = Ranks[S];
		auto RankT = Ranks[T];
		if (RankS == RankT) { continue; }

		auto From = (RankS < RankT) ? S : T;
		auto To   = (RankS < RankT) ? T : S;
		auto FromRank = FMath::Min(RankS, RankT);
		auto ToRank   = FMath::Max(RankS, RankT);

		if (ToRank - FromRank == 1)
		{
			LayoutEdges.Add({ From, To });
		}
		else
		{
			auto Prev = From;
			for (auto R = FromRank + 1; R < ToRank; ++R)
			{
				auto DummyIdx = LayoutNodes.Num();
				LayoutNodes.Add({ -1, R });
				LayoutEdges.Add({ Prev, DummyIdx });
				Prev = DummyIdx;
			}
			LayoutEdges.Add({ Prev, To });
		}
	}

	auto TotalLayoutNodes = LayoutNodes.Num();
	for (auto& N : LayoutNodes) { MaxRank = FMath::Max(MaxRank, N.Rank); }

	// ================================================================================================================
	// Phase 3: Barycenter crossing reduction
	// ================================================================================================================

	auto RankLayers = TArray<TArray<int32>>{};
	RankLayers.SetNum(MaxRank + 1);
	for (auto i = 0; i < TotalLayoutNodes; ++i)
	{ RankLayers[LayoutNodes[i].Rank].Add(i); }

	auto Adj = TArray<TArray<int32>>{};
	Adj.SetNum(TotalLayoutNodes);
	for (auto& E : LayoutEdges)
	{
		Adj[E.From].AddUnique(E.To);
		Adj[E.To].AddUnique(E.From);
	}

	// Same-rank adjacency for edges that land on the same rank
	for (auto& Edge : InEdges)
	{
		auto* InternalSrc = ExternalToInternal.Find(Edge.From);
		auto* InternalDst = ExternalToInternal.Find(Edge.To);
		if (!InternalSrc || !InternalDst) { continue; }

		if (Ranks[*InternalSrc] == Ranks[*InternalDst])
		{
			Adj[*InternalSrc].AddUnique(*InternalDst);
			Adj[*InternalDst].AddUnique(*InternalSrc);
		}
	}

	auto Slots = TArray<float>{};
	Slots.SetNum(TotalLayoutNodes);
	for (auto R = 0; R <= MaxRank; ++R)
	{
		for (auto S = 0; S < RankLayers[R].Num(); ++S)
		{ Slots[RankLayers[R][S]] = static_cast<float>(S); }
	}

	for (auto Iter = 0; Iter < InParams.CrossingReductionPasses; ++Iter)
	{
		auto bLTR  = (Iter % 2 == 0);
		auto Start = bLTR ? 1 : MaxRank - 1;
		auto End   = bLTR ? MaxRank + 1 : -1;
		auto Step  = bLTR ? 1 : -1;

		for (auto R = Start; R != End; R += Step)
		{
			auto& Layer = RankLayers[R];
			auto Weights = TArray<TPair<float, int32>>{};

			for (auto Idx : Layer)
			{
				auto Sum = 0.0f;
				auto Cnt = 0;
				for (auto Nbr : Adj[Idx])
				{
					if (LayoutNodes[Nbr].Rank != R)
					{
						Sum += Slots[Nbr];
						++Cnt;
					}
				}
				Weights.Add({ Cnt > 0 ? Sum / Cnt : Slots[Idx], Idx });
			}

			Weights.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });

			for (auto S = 0; S < Weights.Num(); ++S)
			{
				Layer[S] = Weights[S].Value;
				Slots[Weights[S].Value] = static_cast<float>(S);
			}
		}
	}

	// ================================================================================================================
	// Phase 4: Coordinate assignment
	// ================================================================================================================

	// Per-column width: if any caller node provided a width hint, compute the
	// widest node in each column and use (max_width + SpacingX) as the gap
	// between column centers-of-origin. This guarantees wide nodes never
	// overlap into the next column — classic pain point with Sugiyama layers.
	// When no width hints are present, fall back to uniform SpacingX spacing.

	auto ColumnMaxWidth = TArray<int32>{};
	ColumnMaxWidth.SetNumZeroed(MaxRank + 1);
	auto HasAnyWidth = false;
	for (auto i = 0; i < NodeCount; ++i)
	{
		const auto W = InNodes[i].EstimatedWidth;
		if (W <= 0) { continue; }
		HasAnyWidth = true;
		const auto Rank = Ranks[i];
		if (Rank >= 0 && Rank <= MaxRank)
		{
			ColumnMaxWidth[Rank] = FMath::Max(ColumnMaxWidth[Rank], W);
		}
	}

	// Column start X positions: col0 at 0, colR at colR-1 + width[R-1] + SpacingX.
	auto ColumnX = TArray<int32>{};
	ColumnX.SetNumZeroed(MaxRank + 1);
	if (HasAnyWidth)
	{
		for (auto R = 1; R <= MaxRank; ++R)
		{
			ColumnX[R] = ColumnX[R - 1] + ColumnMaxWidth[R - 1] + InParams.SpacingX;
		}
	}
	else
	{
		for (auto R = 0; R <= MaxRank; ++R)
		{
			ColumnX[R] = R * InParams.SpacingX;
		}
	}

	// Per-row vertical extent: a node with a height hint occupies
	// max(SpacingY, Height + MinVerticalGap) so tall nodes push subsequent
	// rows down instead of overlapping them. Dummies and hint-less nodes keep
	// the legacy SpacingY, which makes this reduce exactly to the legacy
	// uniform-row layout when no caller provides heights.
	constexpr auto MinVerticalGap = 40;

	for (auto R = 0; R <= MaxRank; ++R)
	{
		auto& Layer = RankLayers[R];
		auto Count = Layer.Num();
		if (Count == 0) { continue; }

		auto RowOrigins = TArray<int32>{};
		RowOrigins.SetNum(Count);

		auto Cumulative = 0;
		for (auto S = 0; S < Count; ++S)
		{
			RowOrigins[S] = Cumulative;

			auto Extent = InParams.SpacingY;
			auto Idx = Layer[S];
			if (LayoutNodes[Idx].OriginalIndex >= 0)
			{
				const auto H = InNodes[LayoutNodes[Idx].OriginalIndex].EstimatedHeight;
				if (H > 0)
				{ Extent = FMath::Max(Extent, H + MinVerticalGap); }
			}
			Cumulative += Extent;
		}

		// Centre the column on the span of row origins — identical to the
		// legacy (Count - 1) * SpacingY / 2 offset when all extents match.
		const auto Span = RowOrigins[Count - 1];

		for (auto S = 0; S < Count; ++S)
		{
			auto Idx = Layer[S];
			if (LayoutNodes[Idx].OriginalIndex >= 0)
			{
				auto Orig = LayoutNodes[Idx].OriginalIndex;
				auto ExternalIdx = InNodes[Orig].Index;

				Result.Positions.Add(ExternalIdx, FIntPoint(
					ColumnX[R],
					RowOrigins[S] - Span / 2));
			}
		}
	}

	return Result;
}

// --------------------------------------------------------------------------------------------------------------------
