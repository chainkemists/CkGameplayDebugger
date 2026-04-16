#include "SCkGoapDebugger_StatsPanel.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
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
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Action Details")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_ContentBox, SVerticalBox)
			]
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
	_SelectedAction = InAction;
	_PlanStepIndex = InPlanStepIndex;
	RebuildContent();
}

auto
	SCkGoapDebugger_StatsPanel::
	ClearSelection()
	-> void
{
	_SelectedAction = nullptr;
	_PlanStepIndex = -1;
	RebuildContent();
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	RebuildContent()
	-> void
{
	_ContentBox->ClearChildren();

	if (_SelectedAction == nullptr)
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
			BuildActionContent()
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
		.Padding(FMargin(CkGoapDebuggerStyle::PanelPadding, 24.0f))
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Click a node to inspect")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	BuildActionContent()
	-> TSharedRef<SWidget>
{
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const auto& A = *_SelectedAction;
	const auto P = CkGoapDebuggerStyle::PanelPadding;

	auto Box = SNew(SVerticalBox);

	// Name + step badge + cost
	Box->AddSlot().AutoHeight().Padding(P, 4.0f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(A.ClassName))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(_PlanStepIndex >= 0
					? FText::FromString(FString::Printf(TEXT("Step %d"), _PlanStepIndex + 1))
					: FText::GetEmpty())
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(0.23f, 0.51f, 0.96f))
				.Visibility(_PlanStepIndex >= 0 ? EVisibility::Visible : EVisibility::Collapsed)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("Cost: %.0f"), A.Cost)))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.ColorAndOpacity(CkGoapDebuggerStyle::PlanCostText)
		]
	];

	// Preconditions
	Box->AddSlot().AutoHeight().Padding(P, 8.0f, P, 2.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("PRECONDITIONS")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
	];
	for (const auto& [Key, Value] : A.Preconditions)
	{
		const auto Satisfied = Info != nullptr ? Info->WorldState.Contains(Key) && Info->WorldState[Key] == Value : false;
		Box->AddSlot().AutoHeight().Padding(P, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Satisfied ? TEXT("\u2713") : TEXT("\u2717")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(Satisfied ? CkGoapDebuggerStyle::WorldStateTrue : CkGoapDebuggerStyle::WorldStateFalse)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Value ? TEXT("= true") : TEXT("= false")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			]
		];
	}

	// Effects
	Box->AddSlot().AutoHeight().Padding(P, 8.0f, P, 2.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("EFFECTS")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
	];
	for (const auto& [Key, Value] : A.Effects)
	{
		Box->AddSlot().AutoHeight().Padding(P, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u2192")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(0.23f, 0.51f, 0.96f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Value ? TEXT("= true") : TEXT("= false")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			]
		];
	}

	// Contributing goals
	if (Info != nullptr)
	{
		Box->AddSlot().AutoHeight().Padding(P, 8.0f, P, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("CONTRIBUTING GOALS")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		];

		auto FoundGoal = false;
		for (const auto& Goal : Info->Goals)
		{
			auto Contributes = false;
			for (const auto& [GKey, GVal] : Goal.Conditions)
			{
				if (A.Effects.Contains(GKey)) { Contributes = true; break; }
			}
			if (NOT Contributes) { continue; }
			FoundGoal = true;

			Box->AddSlot().AutoHeight().Padding(P, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Goal.ClassName))
					.ColorAndOpacity(CkGoapDebuggerStyle::PlanCostText)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("P:%d"), Goal.Priority)))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
				]
			];
		}

		if (NOT FoundGoal)
		{
			Box->AddSlot().AutoHeight().Padding(P, 1.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("None")))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			];
		}

		// Alternatives
		Box->AddSlot().AutoHeight().Padding(P, 8.0f, P, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("ALTERNATIVE ACTIONS")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		];
		auto FoundAlt = false;
		for (const auto& Other : Info->Actions)
		{
			if (Other.ClassName == A.ClassName) { continue; }
			auto Overlaps = false;
			for (const auto& [EKey, EVal] : Other.Effects)
			{
				if (A.Effects.Contains(EKey)) { Overlaps = true; break; }
			}
			if (NOT Overlaps) { continue; }
			FoundAlt = true;

			Box->AddSlot().AutoHeight().Padding(P, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Other.ClassName))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), Other.Cost)))
					.ColorAndOpacity(CkGoapDebuggerStyle::PlanCostText)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]
			];
		}
		if (NOT FoundAlt)
		{
			Box->AddSlot().AutoHeight().Padding(P, 1.0f)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("None")))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			];
		}
	}

	return Box;
}

// ====================================================================================================================
