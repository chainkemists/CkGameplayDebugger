#include "SCkGoapDebug_MacroNodesPanel.h"

#include "CkGoapDebug_ActionCategorizer.h"
#include "SCkGoapDebug_ActionRow.h"
#include "SCkGoapDebug_GoalCard.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ExpandableColumn.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_LabeledGroup.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
	SCkGoapDebug_MacroNodesPanel::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;
	_OnActionClicked = InArgs._OnActionClicked;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Tiered action columns (horizontal scroll so narrow panels stay usable)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(_ColumnsBox, SHorizontalBox)
			]
		]

		// Goals row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(_GoalsBox, SHorizontalBox)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkGoapDebug_MacroNodesPanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const auto Hash = ComputeContentHash();
	if (Hash != _LastContentHash)
	{
		_LastContentHash = Hash;
		RebuildPanel();
	}
}

auto
	SCkGoapDebug_MacroNodesPanel::
	ComputeContentHash() const
	-> uint32
{
	if (NOT _ViewModel.IsValid()) { return 0; }
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr) { return 0; }

	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(Info->Actions.Num()));
	Hash = HashCombine(Hash, GetTypeHash(Info->Goals.Num()));
	for (const auto& A : Info->Actions)
	{
		Hash = HashCombine(Hash, GetTypeHash(A.ClassName));
		Hash = HashCombine(Hash, GetTypeHash(A.Preconditions.Num()));
		Hash = HashCombine(Hash, GetTypeHash(A.Effects.Num()));
	}
	for (const auto& G : Info->Goals)
	{
		Hash = HashCombine(Hash, GetTypeHash(G.ClassName));
		Hash = HashCombine(Hash, GetTypeHash(G.IsActiveGoal));
		Hash = HashCombine(Hash, GetTypeHash(G.Priority));
	}
	return Hash;
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkGoapDebug_MacroNodesPanel::
	RebuildPanel()
	-> void
{
	if (NOT _ColumnsBox.IsValid() || NOT _GoalsBox.IsValid()) { return; }
	_ColumnsBox->ClearChildren();
	_GoalsBox->ClearChildren();

	const auto* Info = _ViewModel.IsValid() ? _ViewModel->Get_CurrentGoapInfo() : nullptr;
	if (Info == nullptr || Info->Actions.Num() == 0)
	{
		_ColumnsBox->AddSlot()
		.AutoWidth()
		.Padding(16.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No GOAP entity selected")))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.42f, 0.44f, 0.50f)))
		];
		return;
	}

	const auto Tiers = FCkGoapDebug_ActionCategorizer::DiscoverTiers(Info->Actions, Info->Goals);

	// Bucket actions by tier then by category. Nested map uses uint8 keys to
	// keep Unreal's generic GetTypeHash happy with the enum-class category.
	using FCategoryBucket = TMap<uint8, TArray<const FCkGoapDebugger_ActionInfo*>>;
	auto ByTierByCat = TMap<int32, FCategoryBucket>{};
	for (const auto& A : Info->Actions)
	{
		const auto Tier = FCkGoapDebug_ActionCategorizer::TierOfAction(A, Tiers);
		const auto Cat = FCkGoapDebug_ActionCategorizer::ClassifyCategory(A);
		ByTierByCat.FindOrAdd(Tier).FindOrAdd(static_cast<uint8>(Cat)).Add(&A);
	}

	const auto& CatOrder = FCkGoapDebug_ActionCategorizer::CategoryDisplayOrder();

	// Build a column per tier.
	for (const auto& Tier : Tiers)
	{
		const auto* CatMap = ByTierByCat.Find(Tier.Index);
		auto ActionCount = 0;
		if (CatMap) { for (const auto& P : *CatMap) { ActionCount += P.Value.Num(); } }

		// Skip tiers with nothing to show to reduce horizontal noise on sparse gyms.
		if (ActionCount == 0) { continue; }

		// Build the expanded body: one LabeledGroup per category used.
		auto ExpandedBody = SNew(SVerticalBox);
		for (const auto Cat : CatOrder)
		{
			if (NOT CatMap) { continue; }
			const auto* List = CatMap->Find(static_cast<uint8>(Cat));
			if (List == nullptr || List->Num() == 0) { continue; }

			// Sort within a category: cheaper cost first, ties by name.
			auto Sorted = *List;
			Sorted.Sort([](const FCkGoapDebugger_ActionInfo& A, const FCkGoapDebugger_ActionInfo& B)
			{
				if (A.Cost != B.Cost) { return A.Cost < B.Cost; }
				return A.ClassName < B.ClassName;
			});

			auto Group = SNew(SCkDebug_LabeledGroup)
				.Label(FCkGoapDebug_ActionCategorizer::CategoryLabel(Cat))
				.DotColor(FCkGoapDebug_ActionCategorizer::CategoryColor(Cat))
				.CountText(FText::AsNumber(Sorted.Num()));

			for (const auto* A : Sorted)
			{
				Group->AddChild(
					SNew(SCkGoapDebug_ActionRow)
					.Action(*A)
					.Category(Cat)
					.OnClicked_Lambda([this](FString InName)
					{
						_OnActionClicked.ExecuteIfBound(InName);
					})
				);
			}

			ExpandedBody->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Group
			];
		}

		// Collapsed summary: count + avg cost
		auto TotalCost = 0.0f;
		if (CatMap) { for (const auto& P : *CatMap) { for (const auto* A : P.Value) { TotalCost += A->Cost; } } }
		const auto AvgCost = ActionCount > 0 ? TotalCost / ActionCount : 0.0f;
		auto CollapsedSummary = SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d actions hidden · avg cost %.1f"), ActionCount, AvgCost)))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()));

		const auto TierIdx = Tier.Index;
		const auto StartExpanded = NOT _CollapsedTiers.Contains(TierIdx);

		auto PillText = Tier.GateTagKey.IsNone()
			? FText::FromString(TEXT("no gate"))
			: FText::FromString(FString::Printf(TEXT("gate: %s"), *Tier.GateTagKey.ToString()));

		auto Column = SNew(SCkDebug_ExpandableColumn)
			.Title(FText::FromString(Tier.Label))
			.PillText(PillText)
			.StartExpanded(StartExpanded)
			.OnExpansionChanged_Lambda([this, TierIdx](bool bExpanded)
			{
				if (bExpanded) { _CollapsedTiers.Remove(TierIdx); }
				else           { _CollapsedTiers.Add(TierIdx); }
			})
			.ExpandedBody() [ ExpandedBody ]
			.CollapsedSummary() [ CollapsedSummary ];

		_ColumnsBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
		[
			SNew(SBox)
			.MinDesiredWidth(280.0f)
			.MaxDesiredWidth(360.0f)
			[
				Column
			]
		];
	}

	// Goals strip (active goals first, highest priority first).
	auto SortedGoals = Info->Goals;
	SortedGoals.Sort([](const FCkGoapDebugger_GoalInfo& A, const FCkGoapDebugger_GoalInfo& B)
	{
		if (A.IsActiveGoal != B.IsActiveGoal) { return A.IsActiveGoal; }
		return A.Priority > B.Priority;
	});
	for (const auto& G : SortedGoals)
	{
		_GoalsBox->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
		[
			SNew(SBox)
			.MinDesiredWidth(220.0f)
			[
				SNew(SCkGoapDebug_GoalCard)
				.Goal(G)
			]
		];
	}
}

// ====================================================================================================================
