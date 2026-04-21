#include "SCkGoapDebugger_PlanView.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkGoapDebugger/Window/SCkGoapDebuggerWindow.h"

// ====================================================================================================================

auto
	SCkGoapDebugger_PlanView::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Action Plan")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(_PlanCostText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(CkGoapDebuggerStyle::PlanCostText)
			]
		]

		// Plan list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_PlanListBox, SVerticalBox)
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_PlanView::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(SCkGoapDebuggerWindow::WindowId))
	{ return; }

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr)
	{
		if (_LastPlanLength != 0)
		{
			_LastPlanLength = 0;
			_PlanListBox->ClearChildren();
			_PlanCostText->SetText(FText::GetEmpty());
		}
		return;
	}

	const auto NeedsRebuild = Info->PlanActionNames.Num() != _LastPlanLength
		|| Info->PlanStatus != _LastStatus;

	if (NeedsRebuild)
	{
		_LastPlanLength = Info->PlanActionNames.Num();
		_LastStatus = Info->PlanStatus;
		RebuildPlanList();
	}

	// Update cost
	if (Info->PlanActionNames.Num() > 0)
	{
		_PlanCostText->SetText(FText::FromString(
			FString::Printf(TEXT("Total Cost: %.1f"), Info->PlanCost)));
	}
	else
	{
		_PlanCostText->SetText(FText::GetEmpty());
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_PlanView::
	RebuildPlanList()
	-> void
{
	_PlanListBox->ClearChildren();

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr || Info->PlanActionNames.Num() == 0)
	{
		_PlanListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No plan available")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
		return;
	}

	for (auto Index = int32{0}; Index < Info->PlanActionNames.Num(); ++Index)
	{
		const auto& ActionName = Info->PlanActionNames[Index];

		// Find matching action info for cost display
		auto ActionCost = 0.0f;
		for (const auto& ActionInfo : Info->Actions)
		{
			if (ActionInfo.ClassName == ActionName)
			{
				ActionCost = ActionInfo.Cost;
				break;
			}
		}

		_PlanListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(SBorder)
			.BorderBackgroundColor(CkGoapDebuggerStyle::PlanActionBackground)
			.Padding(FMargin{8.0f, 4.0f})
			[
				SNew(SHorizontalBox)

				// Step number
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%d."), Index + 1)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
				]

				// Action name
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ActionName))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
				]

				// Cost
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%.1f"), ActionCost)))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(CkGoapDebuggerStyle::PlanCostText)
				]
			]
		];
	}
}

// ====================================================================================================================
