#include "SCkGoapDebugger_StyleTest.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

// Style definitions — colors and rendering approach per style variant
struct FNodeStyle
{
	FString Name;

	FLinearColor PlanBorder;
	FLinearColor DimBorder;
	FLinearColor PlanFill;
	FLinearColor DimFill;
	FLinearColor PlanText;
	FLinearColor DimText;
	FLinearColor CostColor;
	FLinearColor PlanArrow;
	FLinearColor DimArrow;

	float DimAlpha;
	bool UseBodyBrush;      // true = Graph.StateNode.Body, false = plain SBorder
	bool ShowStepBadge;     // show colored step number square
	bool ShowPortDots;      // show precondition/effect indicator dots
};

static const FNodeStyle GStyles[] =
{
	// 0: SM-Style (current — Body+ColorSpill, blue border)
	{
		TEXT("A: SM-Style (Body+ColorSpill, blue)"),
		FLinearColor(0.15f, 0.45f, 0.85f),   // PlanBorder
		FLinearColor(0.25f, 0.25f, 0.28f),    // DimBorder
		FLinearColor(0.02f, 0.02f, 0.03f),    // PlanFill
		FLinearColor(0.02f, 0.02f, 0.03f),    // DimFill
		FLinearColor(0.9f, 0.9f, 0.9f),       // PlanText
		FLinearColor(0.5f, 0.5f, 0.5f),       // DimText
		FLinearColor(0.96f, 0.62f, 0.04f),    // CostColor
		FLinearColor(0.13f, 0.77f, 0.37f),    // PlanArrow
		FLinearColor(0.3f, 0.3f, 0.3f),       // DimArrow
		0.4f, true, true, true
	},
	// 1: Bright plan, dark non-plan
	{
		TEXT("B: Bright Cyan Plan, Dark Gray Non-plan"),
		FLinearColor(0.0f, 0.75f, 0.85f),
		FLinearColor(0.18f, 0.18f, 0.20f),
		FLinearColor(0.0f, 0.08f, 0.10f),
		FLinearColor(0.05f, 0.05f, 0.06f),
		FLinearColor(1.0f, 1.0f, 1.0f),
		FLinearColor(0.4f, 0.4f, 0.4f),
		FLinearColor(1.0f, 0.85f, 0.0f),
		FLinearColor(0.0f, 0.9f, 0.7f),
		FLinearColor(0.2f, 0.2f, 0.2f),
		0.35f, true, true, true
	},
	// 2: Green plan, warm gray non-plan
	{
		TEXT("C: Green Plan, Warm Gray Non-plan"),
		FLinearColor(0.13f, 0.77f, 0.37f),
		FLinearColor(0.22f, 0.20f, 0.18f),
		FLinearColor(0.02f, 0.06f, 0.03f),
		FLinearColor(0.04f, 0.04f, 0.04f),
		FLinearColor(0.95f, 0.95f, 0.95f),
		FLinearColor(0.45f, 0.42f, 0.40f),
		FLinearColor(0.96f, 0.62f, 0.04f),
		FLinearColor(0.13f, 0.77f, 0.37f),
		FLinearColor(0.25f, 0.23f, 0.20f),
		0.4f, true, true, false
	},
	// 3: Flat style — no Body brush, plain borders
	{
		TEXT("D: Flat (no Body brush, sharp corners)"),
		FLinearColor(0.3f, 0.5f, 1.0f),
		FLinearColor(0.2f, 0.2f, 0.22f),
		FLinearColor(0.08f, 0.10f, 0.16f),
		FLinearColor(0.06f, 0.06f, 0.07f),
		FLinearColor(0.9f, 0.9f, 0.95f),
		FLinearColor(0.4f, 0.4f, 0.4f),
		FLinearColor(1.0f, 0.7f, 0.2f),
		FLinearColor(0.3f, 0.6f, 1.0f),
		FLinearColor(0.15f, 0.15f, 0.17f),
		0.35f, false, true, true
	},
	// 4: Orange plan accent
	{
		TEXT("E: Orange/Amber Plan Accent"),
		FLinearColor(0.96f, 0.62f, 0.04f),
		FLinearColor(0.20f, 0.20f, 0.22f),
		FLinearColor(0.06f, 0.04f, 0.01f),
		FLinearColor(0.04f, 0.04f, 0.04f),
		FLinearColor(1.0f, 0.95f, 0.9f),
		FLinearColor(0.45f, 0.45f, 0.45f),
		FLinearColor(0.96f, 0.62f, 0.04f),
		FLinearColor(0.96f, 0.62f, 0.04f),
		FLinearColor(0.2f, 0.2f, 0.2f),
		0.4f, true, false, true
	},
	// 5: High contrast — white on dark, purple plan
	{
		TEXT("F: High Contrast Purple Plan"),
		FLinearColor(0.6f, 0.3f, 0.9f),
		FLinearColor(0.15f, 0.15f, 0.18f),
		FLinearColor(0.04f, 0.02f, 0.06f),
		FLinearColor(0.03f, 0.03f, 0.04f),
		FLinearColor(1.0f, 1.0f, 1.0f),
		FLinearColor(0.35f, 0.35f, 0.38f),
		FLinearColor(0.4f, 0.9f, 0.4f),
		FLinearColor(0.6f, 0.3f, 0.9f),
		FLinearColor(0.15f, 0.15f, 0.18f),
		0.3f, true, true, false
	},
	// 6: Minimal — thin border, no fill tint
	{
		TEXT("G: Minimal Thin Border"),
		FLinearColor(0.4f, 0.7f, 1.0f),
		FLinearColor(0.15f, 0.15f, 0.15f),
		FLinearColor(0.03f, 0.03f, 0.04f),
		FLinearColor(0.03f, 0.03f, 0.04f),
		FLinearColor(0.85f, 0.85f, 0.85f),
		FLinearColor(0.35f, 0.35f, 0.35f),
		FLinearColor(0.8f, 0.6f, 0.2f),
		FLinearColor(0.4f, 0.7f, 1.0f),
		FLinearColor(0.12f, 0.12f, 0.12f),
		0.3f, true, false, true
	},
};

static constexpr auto GStyleCount = sizeof(GStyles) / sizeof(GStyles[0]);

// ====================================================================================================================

auto
	SCkGoapDebugger_StyleTest::
	Construct(const FArguments& InArgs)
	-> void
{
	auto ScrollContent = SNew(SVerticalBox);

	for (auto StyleIdx = 0; StyleIdx < static_cast<int32>(GStyleCount); ++StyleIdx)
	{
		ScrollContent->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			BuildStyleRow(StyleIdx, GStyles[StyleIdx].Name)
		];

		if (StyleIdx < static_cast<int32>(GStyleCount) - 1)
		{
			ScrollContent->AddSlot()
			.AutoHeight()
			.Padding(16.0f, 8.0f)
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.15f, 0.15f, 0.18f))
			];
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f))
		.Padding(FMargin(16.0f))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				ScrollContent
			]
		]
	];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StyleTest::
	BuildStyleRow(int32 InStyleIndex, const FString& InLabel)
	-> TSharedRef<SWidget>
{
	return SNew(SVerticalBox)

		// Style label
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InLabel))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
		]

		// Plan nodes row
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("PickUpWeapon"), 2, true, 0, false) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildArrow(true) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("LoadAmmo"), 1, true, 1, false) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildArrow(true) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("Scout"), 3, true, 2, false) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildArrow(true) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("Attack"), 4, true, 3, false) ]
		]

		// Dimmed nodes row
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("TakeCover"), 1, false, -1, true) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildArrow(false) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("MeleeAttack"), 6, false, -1, true) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildArrow(false) ]
			+ SHorizontalBox::Slot().AutoWidth() [ BuildNode(InStyleIndex, TEXT("SecureArea"), 2, false, -1, true) ]
		];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StyleTest::
	BuildNode(int32 InStyleIndex, const FString& InName, float InCost, bool InPlan, int32 InStep, bool InDimmed)
	-> TSharedRef<SWidget>
{
	const auto& S = GStyles[InStyleIndex];
	const auto BorderColor = InPlan ? S.PlanBorder : S.DimBorder;
	const auto FillColor = InPlan ? S.PlanFill : S.DimFill;
	const auto TextColor = InPlan ? S.PlanText : S.DimText;
	const auto Alpha = InDimmed ? S.DimAlpha : 1.0f;

	auto Content = SNew(SHorizontalBox);

	// Step badge
	if (S.ShowStepBadge && InPlan && InStep >= 0)
	{
		Content->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(8.0f)
			.HeightOverride(8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(S.PlanBorder)
			]
		];
	}

	// Name
	Content->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text(FText::FromString(InName))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.ColorAndOpacity(TextColor)
	];

	// Cost
	Content->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(6.0f, 0.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(FString::Printf(TEXT("$%.0f"), InCost)))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		.ColorAndOpacity(S.CostColor)
	];

	// Port dots row
	TSharedRef<SWidget> PortRow = SNullWidget::NullWidget;
	if (S.ShowPortDots)
	{
		PortRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(FLinearColor(0.13f, 0.77f, 0.37f))
				.DesiredSizeOverride(FVector2D(5.0f, 5.0f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Pre")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Eff")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
				.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("WhiteBrush"))
				.ColorAndOpacity(FLinearColor(0.15f, 0.45f, 0.85f))
				.DesiredSizeOverride(FVector2D(5.0f, 5.0f))
			];
	}

	auto InnerContent = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ Content ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f) [ PortRow ];

	TSharedRef<SWidget> NodeWidget = SNullWidget::NullWidget;

	if (S.UseBodyBrush)
	{
		NodeWidget = SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
			.Padding(0.0f)
			.BorderBackgroundColor(BorderColor)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill")))
				.BorderBackgroundColor(FillColor)
				.Padding(FMargin(8.0f, 5.0f))
				[
					InnerContent
				]
			];
	}
	else
	{
		NodeWidget = SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
			.Padding(FMargin(8.0f, 5.0f))
			.BorderBackgroundColor(FillColor)
			.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, Alpha))
			.ForegroundColor(BorderColor)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
				.Padding(FMargin(0.0f))
				[
					InnerContent
				]
			];
	}

	return SNew(SBox)
		.Padding(FMargin(4.0f, 2.0f))
		[
			NodeWidget
		];
}

// ====================================================================================================================

auto
	SCkGoapDebugger_StyleTest::
	BuildArrow(bool InPlanEdge)
	-> TSharedRef<SWidget>
{
	const auto ArrowColor = InPlanEdge
		? FLinearColor(0.13f, 0.77f, 0.37f)
		: FLinearColor(0.2f, 0.2f, 0.2f);

	return SNew(SBox)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\u2192")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			.ColorAndOpacity(ArrowColor)
		];
}

// ====================================================================================================================
