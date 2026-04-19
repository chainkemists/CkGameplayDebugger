#include "SCkGoapDebug_GoalCard.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebug_GoalCard::
	Construct(const FArguments& InArgs)
	-> void
{
	const auto& G = InArgs._Goal;

	auto Conds = FString{};
	for (auto i = 0; i < G.Conditions.Num(); ++i)
	{
		if (i > 0) { Conds += TEXT(", "); }
		Conds += G.Conditions[i].Key.ToString();
	}

	const auto IsActive = G.IsActiveGoal;
	const auto BorderColor = IsActive ? CkDebugStyle::NodeBorder_Goal() : CkDebugStyle::BorderStrong();
	const auto FillColor   = IsActive ? CkDebugStyle::NodeFill_Goal()   : CkDebugStyle::NodeFill_GoalInactive();
	const auto TitleColor  = IsActive ? CkDebugStyle::Accent()          : CkDebugStyle::TextDim();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(BorderColor))
		.Padding(FMargin(CkDebugStyle::NodeBorderThickness()))
		[
			SNew(SBorder)
			.BorderImage(CkDebugStyle::GetFilledBrush())
			.BorderBackgroundColor(FSlateColor(FillColor))
			.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(InArgs._DisplayName.IsEmpty() ? G.ClassName : InArgs._DisplayName))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::NodeTitleFontSize()))
						.ColorAndOpacity(FSlateColor(TitleColor))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_CountBadge)
						.ValueText(FText::AsNumber(G.Priority))
						.SuffixText(FText::FromString(TEXT("priority")))
						.ValueColor(CkDebugStyle::Accent())
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, CkDebugStyle::SpaceXS, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Conds))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
					.AutoWrapText(true)
				]
			]
		]
	];
}

// ====================================================================================================================
