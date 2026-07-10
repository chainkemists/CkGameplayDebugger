// --------------------------------------------------------------------------------------------------------------------
// Showcases for the smallest common primitives:
//   StatusPill, CategoryDot, CountBadge, SectionHeader.
// --------------------------------------------------------------------------------------------------------------------

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CountBadge.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

namespace
{
	// Label + widget stacked horizontally — "Ok:  [pill]"  — shared helper.
	auto MakeLabeledRow(const FString& InLabel, TSharedRef<SWidget> InWidget) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(120.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(InLabel))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				InWidget
			];
	}

	auto MakeColumn() -> TSharedRef<SVerticalBox>
	{
		return SNew(SVerticalBox);
	}
}

// ====================================================================================================================
// StatusPill
// ====================================================================================================================

class FCkGallery_StatusPill : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Status Pill")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Single-word tone-colored badge. Six tones; optional leading dot."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 10; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = MakeColumn();

		// All six tones, dot visible.
		auto DottedRow = SNew(SHorizontalBox);
		for (const auto& Tone : TArray<TPair<FString, ECk_Tone>>{
				{ TEXT("Neutral"), ECk_Tone::Neutral },
				{ TEXT("Info"),    ECk_Tone::Info    },
				{ TEXT("Ok"),      ECk_Tone::Ok      },
				{ TEXT("Warn"),    ECk_Tone::Warn    },
				{ TEXT("Err"),     ECk_Tone::Err     },
				{ TEXT("Accent"),  ECk_Tone::Accent  },
			})
		{
			DottedRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
				[
					SNew(SCkDebug_StatusPill)
					.Text(FText::FromString(Tone.Key))
					.Tone(Tone.Value)
				];
		}

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("With leading dot (default)")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)[ DottedRow ];

		// Dot suppressed.
		auto NoDotRow = SNew(SHorizontalBox);
		for (const auto& Tone : TArray<TPair<FString, ECk_Tone>>{
				{ TEXT("Neutral"), ECk_Tone::Neutral },
				{ TEXT("Ok"),      ECk_Tone::Ok      },
				{ TEXT("Err"),     ECk_Tone::Err     },
			})
		{
			NoDotRow->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
				[
					SNew(SCkDebug_StatusPill)
					.Text(FText::FromString(Tone.Key))
					.Tone(Tone.Value)
					.ShowDot(false)
				];
		}

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("ShowDot(false) — tone still shows via text color")) ];
		Col->AddSlot().AutoHeight()[ NoDotRow ];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_StatusPill)

// ====================================================================================================================
// CategoryDot
// ====================================================================================================================

class FCkGallery_CategoryDot : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Category Dot")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Small colored circle. Six GOAP-category colors plus any custom FLinearColor. Diameter-configurable."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 20; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		const auto Categories = TArray<TPair<FString, FLinearColor>>{
			{ TEXT("Gather"),   CkStyle::CategoryGather()   },
			{ TEXT("Build"),    CkStyle::CategoryBuild()    },
			{ TEXT("Research"), CkStyle::CategoryResearch() },
			{ TEXT("Train"),    CkStyle::CategoryTrain()    },
			{ TEXT("Age"),      CkStyle::CategoryAge()      },
			{ TEXT("Trade"),    CkStyle::CategoryTrade()    },
		};

		auto MakeRow = [&](float InDiameter)
		{
			auto Row = SNew(SHorizontalBox);
			for (const auto& Cat : Categories)
			{
				Row->AddSlot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SCkDebug_CategoryDot)
							.Color(Cat.Value)
							.Diameter(InDiameter)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Cat.Key))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
							.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
						]
					];
			}
			return Row;
		};

		auto Col = MakeColumn();
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("Diameter 8 (default)")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)[ MakeRow(8.0f) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("Diameter 12")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)[ MakeRow(12.0f) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("Diameter 6")) ];
		Col->AddSlot().AutoHeight()[ MakeRow(6.0f) ];
		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_CategoryDot)

// ====================================================================================================================
// CountBadge
// ====================================================================================================================

class FCkGallery_CountBadge : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Count Badge")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Rounded chip with a value + optional suffix. Fully restyleable via args."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 30; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Row = SNew(SHorizontalBox);

		auto AddBadge = [&](TSharedRef<SWidget> InBadge)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)[ InBadge ];
		};

		AddBadge(SNew(SCkDebug_CountBadge).ValueText(FText::FromString(TEXT("5"))));
		AddBadge(SNew(SCkDebug_CountBadge).ValueText(FText::FromString(TEXT("5"))).SuffixText(FText::FromString(TEXT("pre"))));
		AddBadge(SNew(SCkDebug_CountBadge).ValueText(FText::FromString(TEXT("$3"))));
		AddBadge(SNew(SCkDebug_CountBadge).ValueText(FText::FromString(TEXT("priority"))).SuffixText(FText::FromString(TEXT("10"))));

		// Color-overridden (warning red err chip)
		AddBadge(
			SNew(SCkDebug_CountBadge)
			.ValueText(FText::FromString(TEXT("3")))
			.SuffixText(FText::FromString(TEXT("err")))
			.ValueColor(CkStyle::Err())
			.BorderColor(CkStyle::Err())
		);

		auto Col = MakeColumn();
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)[ Caption(TEXT("Value-only · value+suffix · currency · color-override")) ];
		Col->AddSlot().AutoHeight()[ Row ];
		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_CountBadge)

// ====================================================================================================================
// SectionHeader
// ====================================================================================================================

class FCkGallery_SectionHeader : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Section Header")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Subsection heading inside an inspector body. Uppercase label + optional count."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 40; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = MakeColumn();

		Col->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
			[
				SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Preconditions")))
			];

		Col->AddSlot()
			.AutoHeight()
			[
				SNew(SCkDebug_SectionHeader)
				.Label(FText::FromString(TEXT("Effects")))
				.CountText(FText::FromString(TEXT("4")))
			];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_SectionHeader)

// --------------------------------------------------------------------------------------------------------------------
