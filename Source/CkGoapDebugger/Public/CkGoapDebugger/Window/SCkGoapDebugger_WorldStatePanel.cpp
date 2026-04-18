#include "SCkGoapDebugger_WorldStatePanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"

namespace
{
	constexpr auto FlashDurationSeconds = 0.5;
	constexpr auto FlashMaxAlpha = 0.55f;
}

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
		.Padding(CkGoapDebuggerStyle::PanelPadding, 8.0f, CkGoapDebuggerStyle::PanelPadding, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("World State")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		]

		// Search + toggle
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 2.0f, CkGoapDebuggerStyle::PanelPadding, 6.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(_SearchBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("Filter tags...")))
				.OnTextChanged(this, &SCkGoapDebugger_WorldStatePanel::OnSearchTextChanged)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(_ShowOnlyTrueCheckBox, SCheckBox)
				.IsChecked(ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SCkGoapDebugger_WorldStatePanel::OnShowOnlyTrueChanged)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Only true")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextPrimary)
				]
			]
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

	// Diff current vs previous values to record flash-change timestamps.
	// Flash state persists independent of the content-hash rebuild gate so
	// re-filters / scroll don't reset the fade.
	if (Info != nullptr)
	{
		for (const auto& Entry : Info->WorldState)
		{
			const auto* PrevValue = _PrevValues.Find(Entry.Key);
			if (PrevValue == nullptr || *PrevValue != Entry.Value)
			{
				auto& Record = _TagChanges.FindOrAdd(Entry.Key);
				Record.NewValue = Entry.Value;
				Record.ChangeTime = InCurrentTime;
			}
			_PrevValues.Add(Entry.Key, Entry.Value);
		}
	}

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
	OnSearchTextChanged(const FText& InText)
	-> void
{
	_SearchFilter = InText.ToString();
	RebuildWorldState();
}

auto
	SCkGoapDebugger_WorldStatePanel::
	OnShowOnlyTrueChanged(ECheckBoxState InState)
	-> void
{
	_ShowOnlyTrue = (InState == ECheckBoxState::Checked);
	RebuildWorldState();
}

// ====================================================================================================================

auto
	SCkGoapDebugger_WorldStatePanel::
	GetRowBackground(FGameplayTag InKey) const
	-> FSlateColor
{
	const auto* Record = _TagChanges.Find(InKey);
	if (Record == nullptr)
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	const auto Now = FSlateApplication::Get().GetCurrentTime();
	const auto Elapsed = Now - Record->ChangeTime;
	if (Elapsed < 0.0 || Elapsed >= FlashDurationSeconds)
	{
		return FSlateColor(FLinearColor::Transparent);
	}

	const auto FadeAlpha = static_cast<float>(1.0 - (Elapsed / FlashDurationSeconds)) * FlashMaxAlpha;
	auto Color = Record->NewValue
		? CkGoapDebuggerStyle::WorldStateTrue
		: CkGoapDebuggerStyle::WorldStateFalse;
	Color.A = FadeAlpha;
	return FSlateColor(Color);
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

	// Sort entries by key for stable display, then filter.
	auto Sorted = Info->WorldState;
	Sorted.Sort([](const FCkGoapDebugger_WorldStateEntry& A, const FCkGoapDebugger_WorldStateEntry& B)
	{
		return A.Key.ToString() < B.Key.ToString();
	});

	const auto FilterLower = _SearchFilter.ToLower();
	auto VisibleCount = 0;

	for (const auto& Entry : Sorted)
	{
		if (_ShowOnlyTrue && NOT Entry.Value)
		{
			continue;
		}

		if (NOT FilterLower.IsEmpty() && NOT Entry.Key.ToString().ToLower().Contains(FilterLower))
		{
			continue;
		}

		++VisibleCount;

		const auto ValueColor = Entry.Value
			? CkGoapDebuggerStyle::WorldStateTrue
			: CkGoapDebuggerStyle::WorldStateFalse;
		const auto ValueText = Entry.Value ? FString(TEXT("true")) : FString(TEXT("false"));

		const auto Key = Entry.Key;
		const auto BackgroundAttr = TAttribute<FSlateColor>::Create(
			TAttribute<FSlateColor>::FGetter::CreateSP(this, &SCkGoapDebugger_WorldStatePanel::GetRowBackground, Key));

		_StateListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 1.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(BackgroundAttr)
			.Padding(FMargin{2.0f, 1.0f})
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
			]
		];
	}

	if (VisibleCount == 0)
	{
		_StateListBox->AddSlot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 4.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No entries match filter")))
			.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
		];
	}
}

// ====================================================================================================================
