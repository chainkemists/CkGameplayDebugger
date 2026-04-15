#include "SCkGoapDebugger_StatsPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"

// ====================================================================================================================

namespace
{
	auto MakeStatRow(const FString& InLabel, TSharedPtr<STextBlock>& OutText) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InLabel))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(OutText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
			];
	}
}

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
			SNew(SVerticalBox)

			// Header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Search Stats")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
			]

			// Stats rows
			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Iterations"), _IterationsText) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Open Set"), _OpenSetText) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Closed Set"), _ClosedSetText) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Plan Length"), _PlanLengthText) ]

			// Budget
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Budget")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(_BudgetBar, SProgressBar)
					.FillColorAndOpacity(CkGoapDebuggerStyle::BudgetNormal)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(_BudgetText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			]

			// Entity info
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Entity")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Actions"), _ActionsCountText) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("Goals"), _GoalsCountText) ]

			+ SVerticalBox::Slot().AutoHeight().Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
			[ MakeStatRow(TEXT("World State Keys"), _WorldStateCountText) ]

			// Plan history
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Plan History")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(_HistoryBox, SVerticalBox)
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	RefreshFromGoapInfo(_ViewModel->Get_CurrentGoapInfo());
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StatsPanel::
	RefreshFromGoapInfo(const FCkGoapDebugger_GoapInfo* InInfo)
	-> void
{
	if (InInfo == nullptr)
	{
		_IterationsText->SetText(FText::FromString(TEXT("-")));
		_OpenSetText->SetText(FText::FromString(TEXT("-")));
		_ClosedSetText->SetText(FText::FromString(TEXT("-")));
		_PlanLengthText->SetText(FText::FromString(TEXT("-")));
		_BudgetBar->SetPercent(0.0f);
		_BudgetText->SetText(FText::FromString(TEXT("-")));
		_ActionsCountText->SetText(FText::FromString(TEXT("-")));
		_GoalsCountText->SetText(FText::FromString(TEXT("-")));
		_WorldStateCountText->SetText(FText::FromString(TEXT("-")));
		return;
	}

	_IterationsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->IterationsThisFrame)));
	_OpenSetText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->OpenSetSize)));
	_ClosedSetText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->ClosedSetSize)));
	_PlanLengthText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->PlanActionNames.Num())));

	// Budget
	const auto BudgetPercent = FMath::Clamp(InInfo->BudgetUsagePercent / 100.0f, 0.0f, 1.0f);
	_BudgetBar->SetPercent(BudgetPercent);

	auto BudgetColor = CkGoapDebuggerStyle::BudgetNormal;
	if (InInfo->BudgetUsagePercent > 100.0f) { BudgetColor = CkGoapDebuggerStyle::BudgetOver; }
	else if (InInfo->BudgetUsagePercent > 80.0f) { BudgetColor = CkGoapDebuggerStyle::BudgetWarning; }
	_BudgetBar->SetFillColorAndOpacity(BudgetColor);

	_BudgetText->SetText(FText::FromString(
		FString::Printf(TEXT("%.0f%%"), InInfo->BudgetUsagePercent)));
	_BudgetText->SetColorAndOpacity(BudgetColor);

	// Entity info
	_ActionsCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->Actions.Num())));
	_GoalsCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->Goals.Num())));
	_WorldStateCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), InInfo->WorldState.Num())));

	// Plan history
	const auto* History = _ViewModel->Get_PlanHistory(_ViewModel->Get_SelectedEntityHandle());
	const auto HistoryCount = History != nullptr ? History->Num() : 0;

	if (HistoryCount != _LastHistoryCount)
	{
		_LastHistoryCount = HistoryCount;
		_HistoryBox->ClearChildren();

		if (History == nullptr || History->Num() == 0)
		{
			_HistoryBox->AddSlot()
			.AutoHeight()
			.Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("No history")))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			];
		}
		else
		{
			// Show newest first, limit to 10
			const auto StartIndex = FMath::Max(0, History->Num() - 10);
			for (auto Index = History->Num() - 1; Index >= StartIndex; --Index)
			{
				const auto& Entry = (*History)[Index];
				const auto StatusColor = CkGoapDebuggerStyle::GetStatusColor(Entry.FinalStatus);
				const auto StatusText = CkGoapDebuggerStyle::GetStatusString(Entry.FinalStatus);

				_HistoryBox->AddSlot()
				.AutoHeight()
				.Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
						.ColorAndOpacity(StatusColor)
						.DesiredSizeOverride(FVector2D{6.0f, 6.0f})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s  %d actions  %.1f cost"),
							*StatusText, Entry.PlanLength, Entry.PlanCost)))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
					]
				];
			}
		}
	}
}

// ====================================================================================================================
