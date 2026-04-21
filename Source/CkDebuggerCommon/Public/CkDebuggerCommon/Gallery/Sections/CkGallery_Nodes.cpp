// --------------------------------------------------------------------------------------------------------------------
// Graph / node-style primitives:
//   - NodePill exhaustive variants
//   - Graph-node showcase: every node visual the Ck debuggers render,
//     laid out in a flow grid, clickable for selection-ring testing.
// --------------------------------------------------------------------------------------------------------------------

#include "CkCore/Public/CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Gallery/CkDebuggerGallery_Registry.h"
#include "CkGallery_SectionUtils.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NodePill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using ck::gallery::Caption;

namespace
{
	// Body row that looks like a real graph-action precondition/effect summary.
	auto MakeSamplePillBody(const FString& InPre, const FString& InEff) -> TSharedRef<SWidget>
	{
		auto Row = SNew(SHorizontalBox);

		Row->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InPre))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Err()))
			];

		Row->AddSlot()
			.AutoWidth()
			.Padding(CkDebugStyle::SpaceL, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InEff))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::NodeMetaFontSize()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Ok()))
			];

		return Row;
	}
}

// ====================================================================================================================
// NodePill — exhaustive static variants (no selection).
// ====================================================================================================================

class FCkGallery_NodePill : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Node Pill")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Rounded pill used for graph-action nodes AND plan-strip step pills. One widget, five palette variants, composable step badge / cost / body / accent strip / border override."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 300; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		auto Col = SNew(SVerticalBox);

		const auto Variants = TArray<TPair<FString, ECkDebug_NodePillVariant>>{
			{ TEXT("Inactive"), ECkDebug_NodePillVariant::Inactive },
			{ TEXT("InPlan"),   ECkDebug_NodePillVariant::InPlan   },
			{ TEXT("Pending"),  ECkDebug_NodePillVariant::Pending  },
			{ TEXT("Active"),   ECkDebug_NodePillVariant::Active   },
			{ TEXT("Done"),     ECkDebug_NodePillVariant::Done     },
		};

		// ---- Row 1: bare pills, one per variant ---------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
			[ Caption(TEXT("All five variants — bare (no badge, no cost, no body).")) ];

		auto BareRow = SNew(SWrapBox).UseAllottedSize(true);
		for (const auto& V : Variants)
		{
			BareRow->AddSlot().Padding(FMargin(0.0f, 0.0f, CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SBox).MinDesiredWidth(180.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(V.Value)
						.Title(FText::FromString(V.Key))
						.ShowCost(false)
					]
				];
		}
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)[ BareRow ];

		// ---- Row 2: with step badge + cost --------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
			[ Caption(TEXT("Step badge (StepIndex >= 0) + cost. Badge palette matches variant.")) ];

		auto BadgedRow = SNew(SWrapBox).UseAllottedSize(true);
		auto StepIdx = 0;
		for (const auto& V : Variants)
		{
			BadgedRow->AddSlot().Padding(FMargin(0.0f, 0.0f, CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SBox).MinDesiredWidth(180.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(V.Value)
						.StepIndex(StepIdx++)
						.Title(FText::FromString(V.Key + TEXT(" + cost")))
						.CostValue(4.0f + StepIdx)
					]
				];
		}
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)[ BadgedRow ];

		// ---- Row 3: with body content -------------------------------------
		Col->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
			[ Caption(TEXT("With BodyContent — here a pre/eff summary like the real GOAP graph node.")) ];

		auto BodyRow = SNew(SWrapBox).UseAllottedSize(true);
		for (const auto& V : Variants)
		{
			BodyRow->AddSlot().Padding(FMargin(0.0f, 0.0f, CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					SNew(SBox).MinDesiredWidth(220.0f)
					[
						SNew(SCkDebug_NodePill)
						.Variant(V.Value)
						.Title(FText::FromString(V.Key + TEXT(" + body")))
						.CostValue(5.0f)
						.BodyContent()[ MakeSamplePillBody(TEXT("• hasFuel"), TEXT("• fedUp ▲")) ]
					]
				];
		}
		Col->AddSlot().AutoHeight()[ BodyRow ];

		return Col;
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_NodePill)

// ====================================================================================================================
// Graph-node showcase — every node visual the Ck debuggers render, clickable
// for selection-ring testing. Shows GOAP-mode pills alongside ECS-mode pills
// (accent strip + edge-type border override) so both consumers are covered.
// ====================================================================================================================

class SCkGallery_GraphNodePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCkGallery_GraphNodePicker) {}
	SLATE_END_ARGS()

	auto Construct(const FArguments& InArgs) -> void
	{
		BuildSamples();

		ChildSlot
		[
			SNew(SVerticalBox)

			// ---- Status echo (swappable container) ----
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
			[
				SAssignNew(_EchoContainer, SBox)
			]

			// ---- GOAP-style row ----
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
			[
				SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("GOAP Variants")))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceL)
			[
				SAssignNew(_GoapRow, SWrapBox).UseAllottedSize(true)
			]

			// ---- ECS-style row ----
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
			[
				SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("ECS Variants — accent strip + edge-type border override")))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(_EcsRow, SWrapBox).UseAllottedSize(true)
			]
		];

		Rebuild();
	}

private:
	struct FSample
	{
		FString Label;
		ECkDebug_NodePillVariant Variant;
		int32 StepIndex = -1;
		float Cost = 0.0f;
		bool ShowCost = true;
		FLinearColor AccentColor = FLinearColor::Transparent;
		FLinearColor BorderOverride = FLinearColor::Transparent;
		bool UseEcsRow = false;
	};

	auto BuildSamples() -> void
	{
		// GOAP-style samples — mirror every variant so selection ring contrasts
		// against every palette.
		_Samples.Add({ TEXT("GOAP · Inactive"), ECkDebug_NodePillVariant::Inactive,  -1, 4.0f, true });
		_Samples.Add({ TEXT("GOAP · InPlan"),   ECkDebug_NodePillVariant::InPlan,     1, 5.0f, true });
		_Samples.Add({ TEXT("GOAP · Pending"),  ECkDebug_NodePillVariant::Pending,    2, 3.0f, true });
		_Samples.Add({ TEXT("GOAP · Active"),   ECkDebug_NodePillVariant::Active,     3, 7.0f, true });
		_Samples.Add({ TEXT("GOAP · Done"),     ECkDebug_NodePillVariant::Done,       4, 2.0f, true });

		// ECS-style samples — Inactive variant with an accent strip AND an
		// edge-type border override, one per edge kind. Plus the center node
		// which is InPlan with no border override.
		const auto MakeEcs = [this](const FString& Label, ECkDebug_NodePillVariant V,
		                            const FLinearColor& Accent, const FLinearColor& Border)
		{
			FSample S;
			S.Label          = Label;
			S.Variant        = V;
			S.StepIndex      = -1;
			S.ShowCost       = false;
			S.AccentColor    = Accent;
			S.BorderOverride = Border;
			S.UseEcsRow      = true;
			_Samples.Add(S);
		};

		MakeEcs(TEXT("ECS · Center (no border override)"),
		        ECkDebug_NodePillVariant::InPlan,
		        CkDebugStyle::Value_Handle(),
		        FLinearColor::Transparent);

		MakeEcs(TEXT("ECS · LifetimeOwner"),
		        ECkDebug_NodePillVariant::Inactive,
		        CkDebugStyle::Value_Numeric(),
		        CkDebugStyle::Relationship());

		MakeEcs(TEXT("ECS · ContextOwner"),
		        ECkDebug_NodePillVariant::Inactive,
		        CkDebugStyle::Value_String(),
		        CkDebugStyle::Reference());

		MakeEcs(TEXT("ECS · LifetimeDependent"),
		        ECkDebug_NodePillVariant::Inactive,
		        CkDebugStyle::Value_Tag(),
		        CkDebugStyle::Transform());

		MakeEcs(TEXT("ECS · Default edge"),
		        ECkDebug_NodePillVariant::Inactive,
		        CkDebugStyle::Value_Object(),
		        CkDebugStyle::Graph_Node_Border_Default());
	}

	auto Rebuild() -> void
	{
		if (NOT _GoapRow.IsValid() || NOT _EcsRow.IsValid()) { return; }

		_GoapRow->ClearChildren();
		_EcsRow->ClearChildren();

		for (auto Idx = 0; Idx < _Samples.Num(); ++Idx)
		{
			const auto& S = _Samples[Idx];
			const auto IsSelected = (Idx == _SelectedIdx);
			const auto CapturedIdx = Idx;

			auto Pill = SNew(SCkDebug_NodePill)
				.Variant(S.Variant)
				.StepIndex(S.StepIndex)
				.ShowCost(S.ShowCost)
				.Title(FText::FromString(S.Label))
				.CostValue(S.Cost)
				.AccentColor(S.AccentColor)
				.BorderColorOverride(S.BorderOverride)
				.Selected(IsSelected)
				.OnClicked(FOnCkDebug_NodePillClicked::CreateLambda([this, CapturedIdx]()
				{
					_SelectedIdx = (_SelectedIdx == CapturedIdx) ? -1 : CapturedIdx;
					Rebuild();
				}));

			auto Sized = SNew(SBox)
				.MinDesiredWidth(240.0f)
				.Padding(FMargin(0.0f, 0.0f, CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
				[
					Pill
				];

			auto* Target = S.UseEcsRow ? _EcsRow.Get() : _GoapRow.Get();
			Target->AddSlot()[ Sized ];
		}

		// Echo selection via the echo container (SBox wrapping a StatusPill).
		if (_EchoContainer.IsValid())
		{
			const auto SelectedText = _Samples.IsValidIndex(_SelectedIdx)
				? FString(TEXT("selected: ")) + _Samples[_SelectedIdx].Label
				: FString(TEXT("no selection"));

			const auto Tone = _Samples.IsValidIndex(_SelectedIdx)
				? ECkDebug_Tone::Info
				: ECkDebug_Tone::Neutral;

			_EchoContainer->SetContent(
				SNew(SCkDebug_StatusPill)
				.Text(FText::FromString(SelectedText))
				.Tone(Tone)
			);
		}
	}

	TArray<FSample> _Samples;
	int32 _SelectedIdx = -1;
	TSharedPtr<SWrapBox> _GoapRow;
	TSharedPtr<SWrapBox> _EcsRow;
	TSharedPtr<SBox> _EchoContainer;
};

class FCkGallery_GraphNodes : public ICkDebuggerGallery_Section
{
public:
	virtual auto Get_Name() const -> FText override { return FText::FromString(TEXT("Graph Nodes")); }
	virtual auto Get_Description() const -> FText override
	{
		return FText::FromString(TEXT("Every graph-node visual shipped by the Ck debuggers — GOAP variants in the top row, ECS accent + edge-type variants in the bottom row. Click a node to draw its selection ring; click again to clear."));
	}
	virtual auto Get_SortPriority() const -> int32 override { return 310; }

	virtual auto Build_Widget() -> TSharedRef<SWidget> override
	{
		return SNew(SCkGallery_GraphNodePicker);
	}
};

CK_REGISTER_DEBUGGER_GALLERY_SECTION(FCkGallery_GraphNodes)

// --------------------------------------------------------------------------------------------------------------------
