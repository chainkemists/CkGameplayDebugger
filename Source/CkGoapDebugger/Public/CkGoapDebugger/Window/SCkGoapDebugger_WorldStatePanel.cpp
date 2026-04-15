#include "SCkGoapDebugger_WorldStatePanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SScrollBox.h"

// ====================================================================================================================

auto
	SCkGoapDebugger_WorldStatePanel::
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
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("World State")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		]

		// State list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_StateListBox, SVerticalBox)
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_WorldStatePanel::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const auto CurrentCount = Info != nullptr ? Info->WorldState.Num() : 0;

	if (CurrentCount != _LastWorldStateCount)
	{
		_LastWorldStateCount = CurrentCount;
		RebuildWorldState();
	}
}

// ====================================================================================================================

auto
	SCkGoapDebugger_WorldStatePanel::
	RebuildWorldState()
	-> void
{
	_StateListBox->ClearChildren();

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr || Info->WorldState.Num() == 0)
	{
		_StateListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No world state")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
		return;
	}

	// Sort keys alphabetically
	auto SortedKeys = TArray<FGameplayTag>{};
	Info->WorldState.GetKeys(SortedKeys);
	SortedKeys.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const auto& Key : SortedKeys)
	{
		const auto Value = Info->WorldState[Key];
		const auto ValueColor = Value
			? CkGoapDebuggerStyle::WorldStateTrue
			: CkGoapDebuggerStyle::WorldStateFalse;
		const auto ValueText = Value ? TEXT("true") : TEXT("false");

		_StateListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Key.ToString()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ValueText))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(ValueColor)
			]
		];
	}
}

// ====================================================================================================================
