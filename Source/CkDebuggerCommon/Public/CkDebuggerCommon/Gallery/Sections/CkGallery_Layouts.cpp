// --------------------------------------------------------------------------------------------------------------------
// Showcases for layout-style primitives:
//   LabeledGroup, RailContainer, ExpandableColumn, InspectorPanel.
// --------------------------------------------------------------------------------------------------------------------

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ExpandableColumn.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_LabeledGroup.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_RailContainer.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	auto Caption(const FString& InText) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::FromString(InText))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()));
	}

	auto MakeSampleKV(const FString& K, const FString& V, ECkDebug_KeyValueTone InTone, const FLinearColor& InColor) -> TSharedRef<SWidget>
	{
		return SNew(SCkDebug_KeyValueRow)
			.KeyText(FText::FromString(K))
			.ValueText(FText::FromString(V))
			.Tone(InTone)
			.CustomValueColor(InColor);
	}
}

// ====================================================================================================================
// LabeledGroup
// ====================================================================================================================

class FCkGallery_LabeledGroup : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Labeled Group")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Titled box with optional dot + count. Body accepts any vertical stack of children."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 110; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = SNew(SVerticalBox);

		// Plain labelled group.
		auto Plain = SNew(SCkDebug_LabeledGroup).Label(FText::FromString(TEXT("Plain")));
		Plain->AddChild(MakeSampleKV(TEXT("health"), TEXT("100"), ECkDebug_KeyValueTone::Custom, CkDebugStyle::Value_Numeric()));
		Plain->AddChild(MakeSampleKV(TEXT("alive"),  TEXT("true"), ECkDebug_KeyValueTone::Bool, FLinearColor::White));

		// Dotted + count.
		auto Dotted = SNew(SCkDebug_LabeledGroup)
			.Label(FText::FromString(TEXT("With Dot + Count")))
			.DotColor(CkDebugStyle::CategoryBuild())
			.CountText(FText::FromString(TEXT("2")));
		Dotted->AddChild(MakeSampleKV(TEXT("progress"), TEXT("0.65"), ECkDebug_KeyValueTone::Custom, CkDebugStyle::Value_Numeric()));
		Dotted->AddChild(MakeSampleKV(TEXT("stage"),    TEXT("foundation"), ECkDebug_KeyValueTone::Custom, CkDebugStyle::Value_String()));

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("Plain — no dot, no count")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)[ Plain ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("With dot + count")) ];
		Col->AddSlot().AutoHeight()[ Dotted ];
		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_LabeledGroup)

// ====================================================================================================================
// RailContainer
// ====================================================================================================================

class FCkGallery_RailContainer : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Rail Container")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Sidebar rail — styled header + scrollable body. Used for plan history and similar timeline rails."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 120; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Rail = SNew(SCkDebug_RailContainer)
			.Title(FText::FromString(TEXT("Plan History")))
			.CountText(FText::FromString(TEXT("4")));

		Rail->AddChild(SNew(SCkDebug_HistoryRow)
			.Tone(ECkDebug_Tone::Ok)
			.TitleText(FText::FromString(TEXT("MoveToKitchen")))
			.MetaText(FText::FromString(TEXT("$4")))
			.RightText(FText::FromString(TEXT("#142")))
			.SubtitleText(FText::FromString(TEXT("took 0.42s"))));

		Rail->AddChild(SNew(SCkDebug_HistoryRow)
			.Tone(ECkDebug_Tone::Info)
			.TitleText(FText::FromString(TEXT("PrepareMeal")))
			.MetaText(FText::FromString(TEXT("$7")))
			.RightText(FText::FromString(TEXT("#143"))));

		Rail->AddChild(SNew(SCkDebug_HistoryRow)
			.Tone(ECkDebug_Tone::Warn)
			.TitleText(FText::FromString(TEXT("ServeMeal")))
			.RightText(FText::FromString(TEXT("#144")))
			.SubtitleText(FText::FromString(TEXT("precondition failed"))));

		Rail->AddChild(SNew(SCkDebug_HistoryRow)
			.Tone(ECkDebug_Tone::Err)
			.TitleText(FText::FromString(TEXT("RetrySequence")))
			.RightText(FText::FromString(TEXT("#145")))
			.SubtitleText(FText::FromString(TEXT("aborted"))));

		return SNew(SBox)
			.MaxDesiredHeight(260.0f)
			.WidthOverride(320.0f)
			[
				Rail
			];
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_RailContainer)

// ====================================================================================================================
// ExpandableColumn
// ====================================================================================================================

class FCkGallery_ExpandableColumn : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Expandable Column")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Click-to-toggle vertical column. Separate slots for expanded body and collapsed summary, so you can show \"N hidden\" while collapsed."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 130; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto MakeBody = [](const FString& InText)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(InText))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				.AutoWrapText(true);
		};

		auto MakeSummary = [](const FString& InText)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(InText))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", CkDebugStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()));
		};

		auto Row = SNew(SHorizontalBox);

		Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkDebugStyle::SpaceL, 0.0f)
			[
				SNew(SCkDebug_ExpandableColumn)
				.Title(FText::FromString(TEXT("Expanded")))
				.StartExpanded(true)
				.ExpandedBody()[ MakeBody(TEXT("Full body content is visible. Click the header to collapse.")) ]
				.CollapsedSummary()[ MakeSummary(TEXT("(hidden)")) ]
			];

		Row->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkDebugStyle::SpaceL, 0.0f)
			[
				SNew(SCkDebug_ExpandableColumn)
				.Title(FText::FromString(TEXT("Collapsed")))
				.StartExpanded(false)
				.ExpandedBody()[ MakeBody(TEXT("You'd see this if expanded.")) ]
				.CollapsedSummary()[ MakeSummary(TEXT("3 items hidden")) ]
			];

		Row->AddSlot().FillWidth(1.0f)
			[
				SNew(SCkDebug_ExpandableColumn)
				.Title(FText::FromString(TEXT("With Pill")))
				.PillText(FText::FromString(TEXT("dirty")))
				.StartExpanded(true)
				.ExpandedBody()[ MakeBody(TEXT("The pill chip sits in the header.")) ]
				.CollapsedSummary()[ MakeSummary(TEXT("(hidden)")) ]
			];

		return Row;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_ExpandableColumn)

// ====================================================================================================================
// InspectorPanel
// ====================================================================================================================

class FCkGallery_InspectorPanel : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Inspector Panel")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Top-level collapsible panel. Uppercase title, optional count badge, optional status pill in the header, body slot. This whole gallery window is built out of these."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 140; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = SNew(SVerticalBox);

		auto MakeBody = [](const FString& InText)
		{
			return SNew(SBox).Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(STextBlock)
					.Text(FText::FromString(InText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
					.AutoWrapText(true)
				];
		};

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
			[
				SNew(SCkDebug_InspectorPanel)
				.Title(FText::FromString(TEXT("Plain")))
				.Body()[ MakeBody(TEXT("Just a title and body. Click the header to toggle.")) ]
			];

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
			[
				SNew(SCkDebug_InspectorPanel)
				.Title(FText::FromString(TEXT("With Count")))
				.CountText(FText::FromString(TEXT("12")))
				.Body()[ MakeBody(TEXT("Count badge renders beside the title.")) ]
			];

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
			[
				SNew(SCkDebug_InspectorPanel)
				.Title(FText::FromString(TEXT("With Status Pill")))
				.StatusPillText(FText::FromString(TEXT("simulating")))
				.StatusPillTone(ECkDebug_Tone::Info)
				.Body()[ MakeBody(TEXT("Status pill renders in the header next to the count.")) ]
			];

		Col->AddSlot().AutoHeight()
			[
				SNew(SCkDebug_InspectorPanel)
				.Title(FText::FromString(TEXT("Starts Collapsed")))
				.StartExpanded(false)
				.StatusPillText(FText::FromString(TEXT("failed")))
				.StatusPillTone(ECkDebug_Tone::Err)
				.Body()[ MakeBody(TEXT("You only see this after clicking the header to expand.")) ]
			];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_InspectorPanel)

// --------------------------------------------------------------------------------------------------------------------
