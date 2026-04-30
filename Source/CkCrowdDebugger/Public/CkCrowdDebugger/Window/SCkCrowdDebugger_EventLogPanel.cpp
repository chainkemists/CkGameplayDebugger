#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_EventLogPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	ChildSlot
	[
		SNew(SBorder).Padding(FMargin(8, 6))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("EVENT LOG")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("(events appear as gates land — Path/Sleep/Pierce/Proxy categories from Gate 1+)")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
				.AutoWrapText(true)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------
