#include "SCkGoapDebugger_StatsPanel.h"

#include "CkGoapDebugger/Window/MacroNodes/CkGoapDebug_ActionCategorizer.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SAssignNew(_ContentBox, SVerticalBox)
		]
	];

	ClearSelection();
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	SetSelectedAction(const FCkGoapDebugger_ActionInfo* InAction, int32 InPlanStepIndex)
	-> void
{
	// Capture by name, not pointer — the DataCollector rebuilds its entity
	// list every tick and the pointer would dangle.
	_SelectedActionName = InAction != nullptr ? InAction->ClassName : FString{};
	_PlanStepIndex = InPlanStepIndex;
	_LastContentHash = 0;  // force rebuild
	RebuildContent();
}

auto
	SCkGoapDebugger_StatsPanel::
	ClearSelection()
	-> void
{
	_SelectedActionName.Reset();
	_PlanStepIndex = -1;
	_LastContentHash = 0;
	RebuildContent();
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const FCkGoapDebugger_ActionInfo* Action = nullptr;

	if (Info != nullptr && NOT _SelectedActionName.IsEmpty())
	{
		for (const auto& A : Info->Actions)
		{
			if (A.ClassName == _SelectedActionName) { Action = &A; break; }
		}
	}

	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(_SelectedActionName));
	Hash = HashCombine(Hash, GetTypeHash(_PlanStepIndex));
	if (Action != nullptr)
	{
		Hash = HashCombine(Hash, GetTypeHash(Action->Cost));
		Hash = HashCombine(Hash, GetTypeHash(Action->Preconditions.Num()));
		Hash = HashCombine(Hash, GetTypeHash(Action->Effects.Num()));
	}
	if (Info != nullptr)
	{
		for (const auto& Entry : Info->WorldState)
		{
			auto Pair = GetTypeHash(Entry.Key);
			Pair = HashCombine(Pair, GetTypeHash(Entry.Value));
			Hash ^= Pair;
		}
	}

	if (Hash != _LastContentHash)
	{
		_LastContentHash = Hash;
		RebuildContent();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	RebuildContent()
	-> void
{
	_ContentBox->ClearChildren();

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const FCkGoapDebugger_ActionInfo* Action = nullptr;
	if (Info != nullptr && NOT _SelectedActionName.IsEmpty())
	{
		for (const auto& A : Info->Actions)
		{
			if (A.ClassName == _SelectedActionName) { Action = &A; break; }
		}
	}

	if (Action == nullptr)
	{
		_ContentBox->AddSlot().AutoHeight()
		[
			BuildNoSelectionContent()
		];
	}
	else
	{
		_ContentBox->AddSlot().AutoHeight()
		[
			BuildActionContent(*Action)
		];
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	BuildNoSelectionContent()
	-> TSharedRef<SWidget>
{
	return SNew(SBox)
		.Padding(FMargin(CkDebugStyle::SpaceXL, CkDebugStyle::SpaceXXL))
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Click a node in the graph or macro view to inspect")))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeBody()))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	BuildActionContent(const FCkGoapDebugger_ActionInfo& InAction)
	-> TSharedRef<SWidget>
{
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const auto& A = InAction;

	const auto Category = FCkGoapDebug_ActionCategorizer::ClassifyCategory(A);
	const auto CategoryColor = FCkGoapDebug_ActionCategorizer::CategoryColor(Category);
	const auto CategoryLabel = FCkGoapDebug_ActionCategorizer::CategoryLabel(Category);

	// Header block — category dot + category label / title / summary meta / state.
	auto Header = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(SCkDebug_CategoryDot)
				.Color(CategoryColor)
				.Diameter(8.0f)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(CategoryLabel)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH4()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(A.ClassName))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH2()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, CkDebugStyle::SpaceS, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("cost $%.0f"), A.Cost)))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeBody()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Accent()))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceM, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("· %d pre · %d eff"), A.Preconditions.Num(), A.Effects.Num())))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeBody()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
			]
		];

	if (_PlanStepIndex >= 0)
	{
		Header->AddSlot()
			.AutoHeight()
			.Padding(0.0f, CkDebugStyle::SpaceM, 0.0f, 0.0f)
			[
				SNew(SCkDebug_StatusPill)
				.Text(FText::FromString(FString::Printf(TEXT("Plan step %d"), _PlanStepIndex + 1)))
				.Tone(ECkDebug_Tone::Info)
			];
	}

	auto HeaderBlock = SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
		.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceL))
		[
			Header
		];

	// Sections.
	auto Sections = SNew(SVerticalBox);

	// Preconditions — precondition satisfaction check against live world state.
	const auto CheckSatisfied = [&](const FCkGoapDebugger_Condition& C) -> bool
	{
		if (Info == nullptr) { return false; }
		for (const auto& Entry : Info->WorldState)
		{
			if (Entry.Key == C.Key) { return Entry.Value == C.Value; }
		}
		return false;
	};

	Sections->AddSlot()
		.AutoHeight()
		.Padding(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM, CkDebugStyle::SpaceL, 0.0f)
		[
			SNew(SCkDebug_SectionHeader)
			.Label(FText::FromString(TEXT("Preconditions")))
			.CountText(FText::AsNumber(A.Preconditions.Num()))
		];

	for (const auto& Pre : A.Preconditions)
	{
		const auto Satisfied = CheckSatisfied(Pre);
		auto Row = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(Pre.Key.ToString()))
				.ValueText(FText::FromString(Pre.Value ? TEXT("true") : TEXT("false")))
				.ShowMarker(true)
				.MarkerColor(CkDebugStyle::Err())
				.BackgroundColor(CkDebugStyle::Bg2())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCkDebug_StatusPill)
				.Text(FText::FromString(Satisfied ? TEXT("satisfied") : TEXT("unmet")))
				.Tone(Satisfied ? ECkDebug_Tone::Ok : ECkDebug_Tone::Err)
				.ShowDot(false)
			];

		Sections->AddSlot()
			.AutoHeight()
			.Padding(CkDebugStyle::SpaceL, 2.0f, CkDebugStyle::SpaceL, 0.0f)
			[
				Row
			];
	}

	// Effects.
	Sections->AddSlot()
		.AutoHeight()
		.Padding(CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, 0.0f)
		[
			SNew(SCkDebug_SectionHeader)
			.Label(FText::FromString(TEXT("Effects")))
			.CountText(FText::AsNumber(A.Effects.Num()))
		];

	for (const auto& Eff : A.Effects)
	{
		Sections->AddSlot()
			.AutoHeight()
			.Padding(CkDebugStyle::SpaceL, 2.0f, CkDebugStyle::SpaceL, 0.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(Eff.Key.ToString()))
				.ValueText(FText::FromString(FString::Printf(TEXT(":= %s"), Eff.Value ? TEXT("true") : TEXT("false"))))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.CustomValueColor(CkDebugStyle::Ok())
				.ShowMarker(true)
				.MarkerColor(CkDebugStyle::Ok())
				.BackgroundColor(CkDebugStyle::Bg2())
			];
	}

	// Consumed by — actions whose preconditions include any of this action's effects.
	if (Info != nullptr)
	{
		auto Consumers = TArray<const FCkGoapDebugger_ActionInfo*>{};
		const auto HasEffectKey = [&](FGameplayTag InKey) -> bool
		{
			for (const auto& E : A.Effects) { if (E.Key == InKey) { return true; } }
			return false;
		};
		for (const auto& Other : Info->Actions)
		{
			if (Other.ClassName == A.ClassName) { continue; }
			for (const auto& P : Other.Preconditions)
			{
				if (HasEffectKey(P.Key)) { Consumers.Add(&Other); break; }
			}
		}

		Sections->AddSlot()
			.AutoHeight()
			.Padding(CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, 0.0f)
			[
				SNew(SCkDebug_SectionHeader)
				.Label(FText::FromString(TEXT("Consumed by")))
				.CountText(FText::AsNumber(Consumers.Num()))
			];

		if (Consumers.Num() == 0)
		{
			Sections->AddSlot()
				.AutoHeight()
				.Padding(CkDebugStyle::SpaceL, 2.0f, CkDebugStyle::SpaceL, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("(no downstream consumers)")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
				];
		}
		else
		{
			for (const auto* Other : Consumers)
			{
				Sections->AddSlot()
					.AutoHeight()
					.Padding(CkDebugStyle::SpaceL, 2.0f, CkDebugStyle::SpaceL, 0.0f)
					[
						SNew(SCkDebug_KeyValueRow)
						.KeyText(FText::FromString(Other->ClassName))
						.ValueText(FText::FromString(FString::Printf(TEXT("$%.0f"), Other->Cost)))
						.Tone(ECkDebug_KeyValueTone::Custom)
						.CustomValueColor(CkDebugStyle::Accent())
						.BackgroundColor(CkDebugStyle::Bg2())
					];
			}
		}
	}

	// Contributing goals — goals whose conditions include any of this action's effects.
	if (Info != nullptr)
	{
		auto ContributingGoals = TArray<const FCkGoapDebugger_GoalInfo*>{};
		const auto HasEffectKey = [&](FGameplayTag InKey) -> bool
		{
			for (const auto& E : A.Effects) { if (E.Key == InKey) { return true; } }
			return false;
		};
		for (const auto& Goal : Info->Goals)
		{
			for (const auto& C : Goal.Conditions)
			{
				if (HasEffectKey(C.Key)) { ContributingGoals.Add(&Goal); break; }
			}
		}

		if (ContributingGoals.Num() > 0)
		{
			Sections->AddSlot()
				.AutoHeight()
				.Padding(CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, CkDebugStyle::SpaceL, 0.0f)
				[
					SNew(SCkDebug_SectionHeader)
					.Label(FText::FromString(TEXT("Contributes to goal")))
					.CountText(FText::AsNumber(ContributingGoals.Num()))
				];

			for (const auto* Goal : ContributingGoals)
			{
				Sections->AddSlot()
					.AutoHeight()
					.Padding(CkDebugStyle::SpaceL, 2.0f, CkDebugStyle::SpaceL, 0.0f)
					[
						SNew(SCkDebug_KeyValueRow)
						.KeyText(FText::FromString(Goal->ClassName))
						.ValueText(FText::FromString(FString::Printf(TEXT("P:%d"), Goal->Priority)))
						.Tone(ECkDebug_KeyValueTone::Custom)
						.CustomValueColor(CkDebugStyle::Accent())
						.BackgroundColor(CkDebugStyle::Bg2())
					];
			}
		}
	}

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			HeaderBlock
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
		[
			Sections
		];
}

// ====================================================================================================================
