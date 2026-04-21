// --------------------------------------------------------------------------------------------------------------------
// Showcases for row-style primitives:
//   HistoryRow (tone-colored selectable rows with an active-selection accent),
//   KeyValueRow (mono label/value pairs — exhaustive color/interaction variants).
// --------------------------------------------------------------------------------------------------------------------

#include "CkCore/Public/CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_HistoryRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

namespace
{
}

// ====================================================================================================================
// HistoryRow — exhaustive static variants + one interactive list that echoes
// current selection by rebuilding on click.
// ====================================================================================================================

// Tiny wrapper widget that owns a "selected index" and rebuilds rows on click.
class SCkGallery_HistoryRowInteractive : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGallery_HistoryRowInteractive) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void
	{
		_Items = {
			{ ECkDebug_Tone::Ok,   TEXT("MoveTo"),     TEXT("$3"),  TEXT("#101"), TEXT("ok") },
			{ ECkDebug_Tone::Info, TEXT("Interact"),   TEXT("$2"),  TEXT("#102"), TEXT("") },
			{ ECkDebug_Tone::Warn, TEXT("Retry"),      TEXT("$7"),  TEXT("#103"), TEXT("precondition miss") },
			{ ECkDebug_Tone::Err,  TEXT("Abort"),      TEXT("$0"),  TEXT("#104"), TEXT("out of resources") },
		};

		ChildSlot
		[
			SAssignNew(_Host, SVerticalBox)
		];

		Rebuild();
	}

private:
	struct FItem
	{
		ECkDebug_Tone Tone;
		FString Title;
		FString Meta;
		FString Right;
		FString Subtitle;
	};

	auto Rebuild() -> void
	{
		if (NOT _Host.IsValid()) { return; }
		_Host->ClearChildren();

		for (auto Idx = 0; Idx < _Items.Num(); ++Idx)
		{
			const auto& Item = _Items[Idx];
			const auto IsSelected = (Idx == _SelectedIdx);
			const auto CapturedIdx = Idx;

			_Host->AddSlot()
				.AutoHeight()
				[
					SNew(SCkDebug_HistoryRow)
					.Tone(Item.Tone)
					.TitleText(FText::FromString(Item.Title))
					.MetaText(FText::FromString(Item.Meta))
					.RightText(FText::FromString(Item.Right))
					.SubtitleText(FText::FromString(Item.Subtitle))
					.IsSelected(IsSelected)
					.OnClicked(FOnCkDebugHistoryRowClicked::CreateLambda([this, CapturedIdx]()
					{
						_SelectedIdx = (_SelectedIdx == CapturedIdx) ? -1 : CapturedIdx;
						Rebuild();
					}))
				];
		}
	}

	TArray<FItem> _Items;
	TSharedPtr<SVerticalBox> _Host;
	int32 _SelectedIdx = 0;
};

class FCkGallery_HistoryRow : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("History Row")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Compact row for timeline/history lists: tone dot + title + optional right code + optional subtitle. Selection draws a left accent bar."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 200; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = SNew(SVerticalBox);

		// One row per tone, static.
		auto Tones = TArray<TPair<FString, ECkDebug_Tone>>{
			{ TEXT("Neutral"), ECkDebug_Tone::Neutral },
			{ TEXT("Info"),    ECkDebug_Tone::Info    },
			{ TEXT("Ok"),      ECkDebug_Tone::Ok      },
			{ TEXT("Warn"),    ECkDebug_Tone::Warn    },
			{ TEXT("Err"),     ECkDebug_Tone::Err     },
			{ TEXT("Accent"),  ECkDebug_Tone::Accent  },
		};

		auto Static = SNew(SVerticalBox);
		for (const auto& T : Tones)
		{
			Static->AddSlot()
				.AutoHeight()
				[
					SNew(SCkDebug_HistoryRow)
					.Tone(T.Value)
					.TitleText(FText::FromString(T.Key))
					.RightText(FText::FromString(TEXT("#142")))
					.SubtitleText(FText::FromString(TEXT("example subtitle")))
				];
		}

		// One row with subtitle disabled.
		Static->AddSlot()
			.AutoHeight()
			[
				SNew(SCkDebug_HistoryRow)
				.Tone(ECkDebug_Tone::Info)
				.TitleText(FText::FromString(TEXT("NoSubtitle")))
				.RightText(FText::FromString(TEXT("#999")))
			];

		// One selected example.
		Static->AddSlot()
			.AutoHeight()
			[
				SNew(SCkDebug_HistoryRow)
				.Tone(ECkDebug_Tone::Ok)
				.TitleText(FText::FromString(TEXT("Selected (forced)")))
				.RightText(FText::FromString(TEXT("#143")))
				.IsSelected(true)
			];

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("One row per tone, plus a no-subtitle row and a forced-selected row.")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)[ Static ];

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceM)
			[
				SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Interactive")))
			];

		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("Click any row to toggle selection. Re-clicking the selected row clears it.")) ];
		Col->AddSlot().AutoHeight()[ SNew(SCkGallery_HistoryRowInteractive) ];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_HistoryRow)

// ====================================================================================================================
// KeyValueRow — demonstrates every tone, every value-type color, every flag.
// ====================================================================================================================

class FCkGallery_KeyValueRow : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Key/Value Row")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Mono-font label/value row. Bool tone auto-colors by string. Custom tone takes any color. Supports marker dots, clickable keys, and custom value widgets."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 210; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = SNew(SVerticalBox);

		// ---- Bool tone: true vs false ------------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("Bool tone — value text auto-colors green for \"true\", red otherwise.")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)[ SNew(SCkDebug_KeyValueRow).KeyText(FText::FromString(TEXT("alive"))).ValueText(FText::FromString(TEXT("true"))).Tone(ECkDebug_KeyValueTone::Bool) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, CkDebugStyle::SpaceL)[ SNew(SCkDebug_KeyValueRow).KeyText(FText::FromString(TEXT("dead"))).ValueText(FText::FromString(TEXT("false"))).Tone(ECkDebug_KeyValueTone::Bool) ];

		// ---- Custom tone, every value-type color --------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("Custom tone — one row per Value_* color token in CkDebugStyle.")) ];

		auto AddCustomRow = [&](const FString& K, const FString& V, FLinearColor InColor)
		{
			Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(K))
					.ValueText(FText::FromString(V))
					.Tone(ECkDebug_KeyValueTone::Custom)
					.CustomValueColor(InColor)
				];
		};

		AddCustomRow(TEXT("numeric"), TEXT("42"),                CkDebugStyle::Value_Numeric());
		AddCustomRow(TEXT("string"),  TEXT("\"hello\""),         CkDebugStyle::Value_String());
		AddCustomRow(TEXT("math"),    TEXT("(3.14, 0.0, 1.0)"),  CkDebugStyle::Value_Math());
		AddCustomRow(TEXT("tag"),     TEXT("Ck.Goap.Plan"),      CkDebugStyle::Value_Tag());
		AddCustomRow(TEXT("enum"),    TEXT("Active"),            CkDebugStyle::Value_Enum());
		AddCustomRow(TEXT("object"),  TEXT("BP_NPC_Villager"),   CkDebugStyle::Value_Object());
		AddCustomRow(TEXT("handle"),  TEXT("FCk_Handle[e:142]"), CkDebugStyle::Value_Handle());

		// ---- With marker -------------------------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceL, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("ShowMarker — small colored dot before the key.")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("hasTarget")))
				.ValueText(FText::FromString(TEXT("true")))
				.Tone(ECkDebug_KeyValueTone::Bool)
				.ShowMarker(true)
				.MarkerColor(CkDebugStyle::Ok())
			];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("inCombat")))
				.ValueText(FText::FromString(TEXT("false")))
				.Tone(ECkDebug_KeyValueTone::Bool)
				.ShowMarker(true)
				.MarkerColor(CkDebugStyle::Err())
			];

		// ---- Clickable key -----------------------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceL, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("OnKeyClicked — key renders as a selection-colored button. Click logs to output.")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("navigateToEntity")))
				.ValueText(FText::FromString(TEXT("Villager_03")))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.CustomValueColor(CkDebugStyle::Value_Handle())
				.OnKeyClicked(FOnCkDebugKeyValueRow_KeyClicked::CreateLambda([]()
				{
					UE_LOG(LogTemp, Display, TEXT("Gallery: navigateToEntity clicked"));
				}))
			];

		// ---- Custom value widget -----------------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceL, 0.0f, CkDebugStyle::SpaceS)[ Caption(TEXT("ValueWidget slot — hosts any widget in place of the text value (here: a button).")) ];
		Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("spatialGrid")))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.ValueWidget()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Open Tilemap…")))
					.OnClicked_Lambda([]()
					{
						UE_LOG(LogTemp, Display, TEXT("Gallery: Open Tilemap clicked"));
						return FReply::Handled();
					})
				]
			];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_KeyValueRow)

// --------------------------------------------------------------------------------------------------------------------
