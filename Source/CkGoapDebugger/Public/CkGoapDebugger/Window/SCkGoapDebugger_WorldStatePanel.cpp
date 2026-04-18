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

	// Hash content (keys + values) so flips of existing keys trigger a rebuild.
	// Commutative XOR so map iteration order doesn't fake a change.
	auto Hash = uint32{0};
	if (Info != nullptr)
	{
		for (const auto& Entry : Info->WorldState)
		{
			auto PairHash = GetTypeHash(Entry.Key);
			PairHash = HashCombine(PairHash, GetTypeHash(Entry.Value));
			Hash ^= PairHash;
		}
	}

	if (Hash != _LastWorldStateHash)
	{
		_LastWorldStateHash = Hash;
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

	// Sort entries by key for stable display.
	auto Sorted = Info->WorldState;
	Sorted.Sort([](const FCkGoapDebugger_WorldStateEntry& A, const FCkGoapDebugger_WorldStateEntry& B)
	{
		return A.Key.ToString() < B.Key.ToString();
	});

	for (const auto& Entry : Sorted)
	{
		const auto ValueColor = Entry.Value
			? CkGoapDebuggerStyle::WorldStateTrue
			: CkGoapDebuggerStyle::WorldStateFalse;
		const auto ValueText = Entry.Value ? FString(TEXT("true")) : FString(TEXT("false"));

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
				.Text(FText::FromString(Entry.Key.ToString()))
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
