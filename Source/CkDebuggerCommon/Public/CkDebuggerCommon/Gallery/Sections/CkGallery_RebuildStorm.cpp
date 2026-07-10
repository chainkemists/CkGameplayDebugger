// --------------------------------------------------------------------------------------------------------------------
// Repro + A/B fix harness for the "scrunching" bug. When a debugger panel
// destroys + recreates its body every Tick, rows/pills collapse to minimum
// size until rebuilds stop. This section lets you pick:
//
//   - a REBUILD RATE (0 Hz / 1 / 10 / 60 / every-tick), and
//   - a STRATEGY:
//       Baseline           — the broken pattern: SBox::SetContent(new tree)
//       InPlace            — keep outer container, ClearChildren + AddSlot
//       InPlacePrepass     — InPlace + Invalidate(Prepass) on the container
//       DataOnly           — build the tree ONCE, bind TAttribute<FText> to live data
//
// Three scopes are exercised in parallel, mirroring the three real-world
// patterns (ECS inspector body, GOAP plan strip / gallery node grid, and
// the gallery body itself).
// --------------------------------------------------------------------------------------------------------------------

#include "CkCore/Public/CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

namespace
{
	// ================================================================================================================
	// Rate enum (controls WHEN we rebuild)
	// ================================================================================================================

	enum class ECkGallery_StormRate : uint8
	{
		Paused,        // 0 Hz — baseline, should always look correct
		Hz1,           // 1 rebuild / sec
		Hz10,          // 10 / sec
		Hz60,          // 60 / sec
		EveryTick,     // no pacing at all — the worst-case real-world pattern
	};

	auto RateLabel(ECkGallery_StormRate InRate) -> FString
	{
		switch (InRate)
		{
			case ECkGallery_StormRate::Paused:    return TEXT("Paused (0 Hz)");
			case ECkGallery_StormRate::Hz1:       return TEXT("1 Hz");
			case ECkGallery_StormRate::Hz10:      return TEXT("10 Hz");
			case ECkGallery_StormRate::Hz60:      return TEXT("60 Hz");
			case ECkGallery_StormRate::EveryTick: return TEXT("Every tick (worst case)");
			default:                              return TEXT("?");
		}
	}

	auto RateIntervalSeconds(ECkGallery_StormRate InRate) -> float
	{
		switch (InRate)
		{
			case ECkGallery_StormRate::Paused:    return -1.0f;
			case ECkGallery_StormRate::Hz1:       return 1.0f;
			case ECkGallery_StormRate::Hz10:      return 0.1f;
			case ECkGallery_StormRate::Hz60:      return 1.0f / 60.0f;
			case ECkGallery_StormRate::EveryTick: return 0.0f;
			default:                              return -1.0f;
		}
	}

	// ================================================================================================================
	// Strategy enum (controls HOW we rebuild)
	// ================================================================================================================

	enum class ECkGallery_StormFix : uint8
	{
		Baseline,        // current broken pattern: destroy + SetContent a fresh tree each tick
		InPlace,         // keep the outer container; ClearChildren + AddSlot within
		InPlacePrepass,  // InPlace + explicit Invalidate(Prepass) after mutation
		DataOnly,        // build the tree ONCE; TAttribute<FText> bindings pull live data
	};

	auto FixLabel(ECkGallery_StormFix InFix) -> FString
	{
		switch (InFix)
		{
			case ECkGallery_StormFix::Baseline:       return TEXT("Baseline (SetContent new tree)");
			case ECkGallery_StormFix::InPlace:        return TEXT("In-place ClearChildren + AddSlot");
			case ECkGallery_StormFix::InPlacePrepass: return TEXT("In-place + Invalidate(Prepass)");
			case ECkGallery_StormFix::DataOnly:       return TEXT("Data-only (TAttribute<FText>, no rebuild)");
			default:                                  return TEXT("?");
		}
	}
}

// ====================================================================================================================
// Interactive storm widget — owns the rate + strategy dropdowns + three scopes.
// ====================================================================================================================

class SCkGallery_RebuildStorm : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGallery_RebuildStorm) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void
	{
		_Rate = ECkGallery_StormRate::Paused;
		_Fix  = ECkGallery_StormFix::Baseline;

		_RateOptions.Add(MakeShared<ECkGallery_StormRate>(ECkGallery_StormRate::Paused));
		_RateOptions.Add(MakeShared<ECkGallery_StormRate>(ECkGallery_StormRate::Hz1));
		_RateOptions.Add(MakeShared<ECkGallery_StormRate>(ECkGallery_StormRate::Hz10));
		_RateOptions.Add(MakeShared<ECkGallery_StormRate>(ECkGallery_StormRate::Hz60));
		_RateOptions.Add(MakeShared<ECkGallery_StormRate>(ECkGallery_StormRate::EveryTick));

		_FixOptions.Add(MakeShared<ECkGallery_StormFix>(ECkGallery_StormFix::Baseline));
		_FixOptions.Add(MakeShared<ECkGallery_StormFix>(ECkGallery_StormFix::InPlace));
		_FixOptions.Add(MakeShared<ECkGallery_StormFix>(ECkGallery_StormFix::InPlacePrepass));
		_FixOptions.Add(MakeShared<ECkGallery_StormFix>(ECkGallery_StormFix::DataOnly));

		ChildSlot
		[
			SNew(SVerticalBox)

			// ---- Controls row ------------------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Rebuild rate:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
				[
					SAssignNew(_RateCombo, SComboBox<TSharedPtr<ECkGallery_StormRate>>)
					.OptionsSource(&_RateOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<ECkGallery_StormRate> InItem)
					{
						return SNew(STextBlock).Text(FText::FromString(RateLabel(*InItem)));
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<ECkGallery_StormRate> InItem, ESelectInfo::Type)
					{
						if (InItem.IsValid())
						{
							_Rate = *InItem;
							_TimeSinceLast = 0.0f;
							_RatePerSecond = 0.0f;
							_RebuildsThisSecond = 0;
							_RateSampleTimer = 0.0f;
						}
					})
					[
						SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(RateLabel(_Rate)); })
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Strategy:")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkStyle::FontSizeSmall()))
					.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
				[
					SAssignNew(_FixCombo, SComboBox<TSharedPtr<ECkGallery_StormFix>>)
					.OptionsSource(&_FixOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<ECkGallery_StormFix> InItem)
					{
						return SNew(STextBlock).Text(FText::FromString(FixLabel(*InItem)));
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<ECkGallery_StormFix> InItem, ESelectInfo::Type)
					{
						if (InItem.IsValid())
						{
							_Fix = *InItem;
							RebuildAll(); // reset layout to reflect new strategy
						}
					})
					[
						SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FixLabel(_Fix)); })
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(_StatusContainer, SBox)
				]
			]

			// ---- Scope 1: Vertical list ------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Scope 1 — Vertical list of KeyValueRows"))) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ Caption(TEXT("Mirrors the ECS inspector body and GOAP stats panel.")) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
			[
				SAssignNew(_VListHost, SBox)
			]

			// ---- Scope 2: Wrap box -----------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Scope 2 — Wrap box of NodePills"))) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ Caption(TEXT("Mirrors the GOAP plan-strip and the gallery's own node grid.")) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceL)
			[
				SAssignNew(_WrapHost, SBox)
			]

			// ---- Scope 3: Scroll of panels ---------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Scope 3 — Scroll box of nested InspectorPanels"))) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[ Caption(TEXT("Mirrors the gallery body itself and the ECS multi-entity inspector.")) ]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(_ScrollHost, SBox)
				.HeightOverride(280.0f)
			]
		];

		RebuildAll();
		RefreshStatus();
	}

	virtual auto Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override
	{
		SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		_RateSampleTimer += InDeltaTime;
		if (_RateSampleTimer >= 1.0f)
		{
			_RatePerSecond = static_cast<float>(_RebuildsThisSecond) / _RateSampleTimer;
			_RebuildsThisSecond = 0;
			_RateSampleTimer = 0.0f;
			RefreshStatus();
		}

		const auto Interval = RateIntervalSeconds(_Rate);
		if (Interval < 0.0f) { return; }

		_TimeSinceLast += InDeltaTime;
		if (_TimeSinceLast < Interval) { return; }
		_TimeSinceLast = 0.0f;

		++_Generation;
		++_RebuildsThisSecond;

		// DataOnly strategy: widget tree is built once in RebuildAll().
		// TAttribute<FText> bindings pull live data from _Generation on every
		// paint; there's nothing to rebuild here.
		if (_Fix == ECkGallery_StormFix::DataOnly) { return; }

		ApplyStrategy();
	}

private:
	// ================================================================================================================
	// Initial build — called once on Construct, and again when strategy changes.
	// ================================================================================================================
	auto RebuildAll() -> void
	{
		if (NOT _VListHost.IsValid() || NOT _WrapHost.IsValid() || NOT _ScrollHost.IsValid()) { return; }

		// For InPlace* strategies, we need persistent outer containers to
		// mutate. Create them fresh when strategy changes.
		_VListPersistent   = SNew(SVerticalBox);
		_WrapPersistent    = SNew(SWrapBox).UseAllottedSize(true);
		_ScrollPersistent  = SNew(SScrollBox).Orientation(Orient_Vertical);

		if (_Fix == ECkGallery_StormFix::Baseline)
		{
			// No persistent container — Baseline replaces content each tick.
			_VListHost->SetContent(BuildVerticalList_Fresh(_Generation));
			_WrapHost->SetContent(BuildWrapBox_Fresh(_Generation));
			_ScrollHost->SetContent(BuildScrollOfPanels_Fresh(_Generation));
		}
		else if (_Fix == ECkGallery_StormFix::InPlace || _Fix == ECkGallery_StormFix::InPlacePrepass)
		{
			PopulateVerticalList_InPlace();
			PopulateWrapBox_InPlace();
			PopulateScroll_InPlace();

			_VListHost->SetContent(_VListPersistent.ToSharedRef());
			_WrapHost->SetContent(_WrapPersistent.ToSharedRef());
			_ScrollHost->SetContent(_ScrollPersistent.ToSharedRef());
		}
		else // DataOnly
		{
			_VListHost->SetContent(BuildVerticalList_DataOnly());
			_WrapHost->SetContent(BuildWrapBox_DataOnly());
			_ScrollHost->SetContent(BuildScrollOfPanels_DataOnly());
		}
	}

	// ================================================================================================================
	// Per-tick strategy application.
	// ================================================================================================================
	auto ApplyStrategy() -> void
	{
		switch (_Fix)
		{
			case ECkGallery_StormFix::Baseline:
			{
				if (_VListHost.IsValid())  { _VListHost->SetContent(BuildVerticalList_Fresh(_Generation)); }
				if (_WrapHost.IsValid())   { _WrapHost->SetContent(BuildWrapBox_Fresh(_Generation)); }
				if (_ScrollHost.IsValid()) { _ScrollHost->SetContent(BuildScrollOfPanels_Fresh(_Generation)); }
				break;
			}
			case ECkGallery_StormFix::InPlace:
			{
				PopulateVerticalList_InPlace();
				PopulateWrapBox_InPlace();
				PopulateScroll_InPlace();
				break;
			}
			case ECkGallery_StormFix::InPlacePrepass:
			{
				PopulateVerticalList_InPlace();
				PopulateWrapBox_InPlace();
				PopulateScroll_InPlace();
				if (_VListPersistent.IsValid())  { _VListPersistent->Invalidate(EInvalidateWidgetReason::Prepass); }
				if (_WrapPersistent.IsValid())   { _WrapPersistent->Invalidate(EInvalidateWidgetReason::Prepass); }
				if (_ScrollPersistent.IsValid()) { _ScrollPersistent->Invalidate(EInvalidateWidgetReason::Prepass); }
				break;
			}
			case ECkGallery_StormFix::DataOnly:
			default:
				break; // nothing to do — bound attributes pick up _Generation
		}
	}

	// ================================================================================================================
	// Builders — BASELINE (fresh tree each call)
	// ================================================================================================================
	auto BuildVerticalList_Fresh(int32 InGen) -> TSharedRef<SWidget>
	{
		auto Col = SNew(SVerticalBox);
		for (auto Idx = 0; Idx < 8; ++Idx)
		{
			const auto Value = (Idx * 7 + InGen) % 100;
			Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(FString::Printf(TEXT("metric_%d"), Idx)))
					.ValueText(FText::FromString(FString::Printf(TEXT("%d"), Value)))
					.Tone(ECkDebug_KeyValueTone::Custom)
					.CustomValueColor(CkStyle::Value_Numeric())
				];
		}
		return Col;
	}

	auto BuildWrapBox_Fresh(int32 InGen) -> TSharedRef<SWidget>
	{
		auto Wrap = SNew(SWrapBox).UseAllottedSize(true);
		const auto Variants = TArray<ECkDebug_NodePillVariant>{
			ECkDebug_NodePillVariant::Inactive, ECkDebug_NodePillVariant::InPlan,
			ECkDebug_NodePillVariant::Pending,  ECkDebug_NodePillVariant::Active,
			ECkDebug_NodePillVariant::Done,
		};
		for (auto Idx = 0; Idx < Variants.Num(); ++Idx)
		{
			Wrap->AddSlot().Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS))
				[
					SNew(SBox).MinDesiredWidth(160.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(Variants[Idx])
						.StepIndex(Idx)
						.Title(FText::FromString(FString::Printf(TEXT("Step #%d"), InGen + Idx)))
						.CostValue(static_cast<float>((Idx + InGen) % 10))
					]
				];
		}
		return Wrap;
	}

	auto BuildScrollOfPanels_Fresh(int32 InGen) -> TSharedRef<SWidget>
	{
		auto Scroll = SNew(SScrollBox).Orientation(Orient_Vertical);
		for (auto Idx = 0; Idx < 4; ++Idx)
		{
			auto Body = SNew(SVerticalBox);
			for (auto Inner = 0; Inner < 4; ++Inner)
			{
				Body->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
					[
						SNew(SCkDebug_KeyValueRow)
						.KeyText(FText::FromString(FString::Printf(TEXT("panel_%d.field_%d"), Idx, Inner)))
						.ValueText(FText::FromString(FString::Printf(TEXT("%d"), (Inner * 3 + InGen) % 100)))
						.Tone(ECkDebug_KeyValueTone::Custom)
						.CustomValueColor(CkStyle::Value_Numeric())
					];
			}
			Scroll->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
				[
					SNew(SCkDebug_InspectorPanel)
					.Title(FText::FromString(FString::Printf(TEXT("Panel %d"), Idx)))
					.CountText(FText::FromString(TEXT("4")))
					.Body()
					[
						SNew(SBox).Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
						[ Body ]
					]
				];
		}
		return Scroll;
	}

	// ================================================================================================================
	// Builders — IN-PLACE (clear + add within persistent outer containers)
	// ================================================================================================================
	auto PopulateVerticalList_InPlace() -> void
	{
		if (NOT _VListPersistent.IsValid()) { return; }
		_VListPersistent->ClearChildren();
		for (auto Idx = 0; Idx < 8; ++Idx)
		{
			const auto Value = (Idx * 7 + _Generation) % 100;
			_VListPersistent->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(FString::Printf(TEXT("metric_%d"), Idx)))
					.ValueText(FText::FromString(FString::Printf(TEXT("%d"), Value)))
					.Tone(ECkDebug_KeyValueTone::Custom)
					.CustomValueColor(CkStyle::Value_Numeric())
				];
		}
	}

	auto PopulateWrapBox_InPlace() -> void
	{
		if (NOT _WrapPersistent.IsValid()) { return; }
		_WrapPersistent->ClearChildren();
		const auto Variants = TArray<ECkDebug_NodePillVariant>{
			ECkDebug_NodePillVariant::Inactive, ECkDebug_NodePillVariant::InPlan,
			ECkDebug_NodePillVariant::Pending,  ECkDebug_NodePillVariant::Active,
			ECkDebug_NodePillVariant::Done,
		};
		for (auto Idx = 0; Idx < Variants.Num(); ++Idx)
		{
			_WrapPersistent->AddSlot().Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS))
				[
					SNew(SBox).MinDesiredWidth(160.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(Variants[Idx])
						.StepIndex(Idx)
						.Title(FText::FromString(FString::Printf(TEXT("Step #%d"), _Generation + Idx)))
						.CostValue(static_cast<float>((Idx + _Generation) % 10))
					]
				];
		}
	}

	auto PopulateScroll_InPlace() -> void
	{
		if (NOT _ScrollPersistent.IsValid()) { return; }
		_ScrollPersistent->ClearChildren();
		for (auto Idx = 0; Idx < 4; ++Idx)
		{
			auto Body = SNew(SVerticalBox);
			for (auto Inner = 0; Inner < 4; ++Inner)
			{
				Body->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
					[
						SNew(SCkDebug_KeyValueRow)
						.KeyText(FText::FromString(FString::Printf(TEXT("panel_%d.field_%d"), Idx, Inner)))
						.ValueText(FText::FromString(FString::Printf(TEXT("%d"), (Inner * 3 + _Generation) % 100)))
						.Tone(ECkDebug_KeyValueTone::Custom)
						.CustomValueColor(CkStyle::Value_Numeric())
					];
			}
			_ScrollPersistent->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
				[
					SNew(SCkDebug_InspectorPanel)
					.Title(FText::FromString(FString::Printf(TEXT("Panel %d"), Idx)))
					.CountText(FText::FromString(TEXT("4")))
					.Body()
					[
						SNew(SBox).Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
						[ Body ]
					]
				];
		}
	}

	// ================================================================================================================
	// Builders — DATA-ONLY (build tree once, TAttribute bindings pull _Generation)
	// ================================================================================================================
	auto BuildVerticalList_DataOnly() -> TSharedRef<SWidget>
	{
		auto Col = SNew(SVerticalBox);
		for (auto Idx = 0; Idx < 8; ++Idx)
		{
			const auto CapturedIdx = Idx;
			auto ValueAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
				[this, CapturedIdx]()
				{
					const auto Value = (CapturedIdx * 7 + _Generation) % 100;
					return FText::FromString(FString::Printf(TEXT("%d"), Value));
				}));

			Col->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(FString::Printf(TEXT("metric_%d"), Idx)))
					.ValueText(ValueAttr)
					.Tone(ECkDebug_KeyValueTone::Custom)
					.CustomValueColor(CkStyle::Value_Numeric())
				];
		}
		return Col;
	}

	auto BuildWrapBox_DataOnly() -> TSharedRef<SWidget>
	{
		// NodePill currently takes static Title/Cost args — a true data-only
		// variant would need NodePill to accept TAttribute<FText> for its title
		// / cost / step index. For the A/B we just build the pills once with
		// their initial values and let them stay put; the "data change" won't
		// reflect but we can still see whether the structural pattern scrunches.
		auto Wrap = SNew(SWrapBox).UseAllottedSize(true);
		const auto Variants = TArray<ECkDebug_NodePillVariant>{
			ECkDebug_NodePillVariant::Inactive, ECkDebug_NodePillVariant::InPlan,
			ECkDebug_NodePillVariant::Pending,  ECkDebug_NodePillVariant::Active,
			ECkDebug_NodePillVariant::Done,
		};
		for (auto Idx = 0; Idx < Variants.Num(); ++Idx)
		{
			Wrap->AddSlot().Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, CkStyle::SpaceS))
				[
					SNew(SBox).MinDesiredWidth(160.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(Variants[Idx])
						.StepIndex(Idx)
						.Title(FText::FromString(FString::Printf(TEXT("Step #%d (static)"), Idx)))
						.CostValue(static_cast<float>(Idx))
					]
				];
		}
		return Wrap;
	}

	auto BuildScrollOfPanels_DataOnly() -> TSharedRef<SWidget>
	{
		auto Scroll = SNew(SScrollBox).Orientation(Orient_Vertical);
		for (auto Idx = 0; Idx < 4; ++Idx)
		{
			auto Body = SNew(SVerticalBox);
			for (auto Inner = 0; Inner < 4; ++Inner)
			{
				const auto CapturedIdx = Idx;
				const auto CapturedInner = Inner;
				auto ValueAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
					[this, CapturedInner]()
					{
						return FText::FromString(FString::Printf(TEXT("%d"), (CapturedInner * 3 + _Generation) % 100));
					}));

				Body->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
					[
						SNew(SCkDebug_KeyValueRow)
						.KeyText(FText::FromString(FString::Printf(TEXT("panel_%d.field_%d"), CapturedIdx, CapturedInner)))
						.ValueText(ValueAttr)
						.Tone(ECkDebug_KeyValueTone::Custom)
						.CustomValueColor(CkStyle::Value_Numeric())
					];
			}
			Scroll->AddSlot().Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
				[
					SNew(SCkDebug_InspectorPanel)
					.Title(FText::FromString(FString::Printf(TEXT("Panel %d"), Idx)))
					.CountText(FText::FromString(TEXT("4")))
					.Body()
					[
						SNew(SBox).Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceM))
						[ Body ]
					]
				];
		}
		return Scroll;
	}

	// ================================================================================================================
	// Status echo
	// ================================================================================================================
	auto RefreshStatus() -> void
	{
		if (NOT _StatusContainer.IsValid()) { return; }
		const auto PillText = FString::Printf(TEXT("gen %d   ·   %.1f rebuilds/s"), _Generation, _RatePerSecond);
		_StatusContainer->SetContent(
			SNew(SCkDebug_StatusPill)
			.Text(FText::FromString(PillText))
			.Tone(_Rate == ECkGallery_StormRate::Paused ? ECk_Tone::Neutral : ECk_Tone::Info)
		);
	}

	// ---- State ------------------------------------------------------------
	ECkGallery_StormRate _Rate = ECkGallery_StormRate::Paused;
	ECkGallery_StormFix  _Fix  = ECkGallery_StormFix::Baseline;

	TArray<TSharedPtr<ECkGallery_StormRate>> _RateOptions;
	TArray<TSharedPtr<ECkGallery_StormFix>>  _FixOptions;
	TSharedPtr<SComboBox<TSharedPtr<ECkGallery_StormRate>>> _RateCombo;
	TSharedPtr<SComboBox<TSharedPtr<ECkGallery_StormFix>>>  _FixCombo;

	// Scope hosts (always present — strategy swaps their content).
	TSharedPtr<SBox> _VListHost;
	TSharedPtr<SBox> _WrapHost;
	TSharedPtr<SBox> _ScrollHost;
	TSharedPtr<SBox> _StatusContainer;

	// Persistent inner containers — only used by InPlace strategies.
	TSharedPtr<SVerticalBox> _VListPersistent;
	TSharedPtr<SWrapBox>     _WrapPersistent;
	TSharedPtr<SScrollBox>   _ScrollPersistent;

	// Telemetry
	float _TimeSinceLast     = 0.0f;
	int32 _Generation        = 0;
	int32 _RebuildsThisSecond = 0;
	float _RatePerSecond      = 0.0f;
	float _RateSampleTimer    = 0.0f;
};

// ====================================================================================================================
// Gallery section wrapper.
// ====================================================================================================================

class FCkGallery_RebuildStorm : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Rebuild Storm (Repro + Fix A/B)")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT(
			"Reproduces the scrunching bug AND A/Bs candidate fixes. Pick a Strategy to change HOW rebuilds happen:\n"
			"  • Baseline — destroys + recreates the widget tree each tick (broken pattern).\n"
			"  • In-place — keeps the outer container, ClearChildren + AddSlot within.\n"
			"  • In-place + Invalidate(Prepass) — as above plus an explicit prepass hint.\n"
			"  • Data-only — builds the tree once and binds TAttribute<FText> to live data (no structural rebuild).\n"
			"Pick a Rate to control HOW OFTEN. Run at 60 Hz / every-tick and watch whether each strategy scrunches."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 900; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		return SNew(SCkGallery_RebuildStorm);
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_RebuildStorm)

// --------------------------------------------------------------------------------------------------------------------
