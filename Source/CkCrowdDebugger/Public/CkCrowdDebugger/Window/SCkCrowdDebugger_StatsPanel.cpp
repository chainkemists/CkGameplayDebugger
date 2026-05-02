#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_StatsPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SBorder).Padding(FMargin(8, 6))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("STATS")))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text_Lambda([this]() -> FText
				{
					if (NOT _ViewModel.IsValid())
					{ return FText::FromString(TEXT("Total agents: —")); }
					return FText::FromString(FString::Printf(TEXT("Total agents: %d"),
						_ViewModel->Get_AgentCount()));
				})
			]
			// Gate 3 — Avg neighbors. Sums NeighborCount across all snapshots / agent count.
			// Cheap to recompute every frame at typical agent counts; if it ever shows on a
			// profile, push to a cached value updated only when the list changes.
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(STextBlock).Text_Lambda([this]() -> FText
				{
					if (NOT _ViewModel.IsValid())
					{ return FText::FromString(TEXT("Avg neighbors: —")); }
					const auto& Agents = _ViewModel->Get_AllAgents();
					if (Agents.Num() == 0)
					{ return FText::FromString(TEXT("Avg neighbors: 0.0  (max 0)")); }
					int32 TotalN = 0;
					int32 MaxN = 0;
					for (const auto& A : Agents)
					{
						TotalN += A.NeighborCount;
						MaxN = FMath::Max(MaxN, A.NeighborCount);
					}
					const float Avg = static_cast<float>(TotalN) / static_cast<float>(Agents.Num());
					return FText::FromString(FString::Printf(
						TEXT("Avg neighbors: %.1f  (max %d)"), Avg, MaxN));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Awake / Asleep / Replanning / Failed populate in Gate 4+.\n"
				                             "Neighbor query ms breakdown lands when 3B's perf scope is wired in.")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
				.AutoWrapText(true)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------
