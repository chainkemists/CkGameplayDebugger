#include "SCkGoapDebugger_GoalPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"

// ====================================================================================================================

auto
	SCkGoapDebugger_GoalPanel::
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
			.Text(FText::FromString(TEXT("Goals")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_GoalListBox, SVerticalBox)
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_GoalPanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const auto CurrentCount = Info != nullptr ? Info->Goals.Num() : 0;

	if (CurrentCount != _LastGoalCount)
	{
		_LastGoalCount = CurrentCount;
		RebuildGoalList();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_GoalPanel::
	RebuildGoalList()
	-> void
{
	_GoalListBox->ClearChildren();

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr || Info->Goals.Num() == 0)
	{
		_GoalListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No goals registered")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
		return;
	}

	for (const auto& Goal : Info->Goals)
	{
		const auto GoalColor = Goal.IsActiveGoal
			? CkGoapDebuggerStyle::GoalActive
			: CkGoapDebuggerStyle::GoalInactive;

		_GoalListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(SVerticalBox)

			// Goal header
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Active indicator
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.ColorAndOpacity(GoalColor)
					.DesiredSizeOverride(FVector2D{8.0f, 8.0f})
				]

				// Name
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Goal.ClassName))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
				]

				// Priority
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("P:%d"), Goal.Priority)))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
				]
			]

			// Goal conditions
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 2.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Conditions = Goal.Conditions]()
				{
					auto Text = FString{};
					for (const auto& [Key, Value] : Conditions)
					{
						if (NOT Text.IsEmpty()) { Text += TEXT(", "); }
						Text += FString::Printf(TEXT("%s=%s"),
							*Key.ToString(), Value ? TEXT("T") : TEXT("F"));
					}
					return FText::FromString(Text.IsEmpty() ? TEXT("(no conditions)") : Text);
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
			]
		];
	}
}

// ====================================================================================================================
