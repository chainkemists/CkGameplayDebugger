#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCrowdDebugger/Window/CkCrowdDebugger_PanelAxes.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_StatsPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	const auto NumColor = CkStyle::Value_Numeric();

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding_Lambda([]() { return ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
			[
				ck::crowd_debugger_axes::Make_PaneHeading(TEXT("Stats"))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("Total agents")))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.CustomValueColor(NumColor)
				.ValueText_Lambda([this]
				{
					return _ViewModel.IsValid()
						? FText::AsNumber(_ViewModel->Get_AgentCount())
						: FText::FromString(TEXT("—"));
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("Avg neighbors")))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.CustomValueColor(NumColor)
				.ValueText_Lambda([this]
				{
					if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
					const auto& Agents = _ViewModel->Get_AllAgents();
					if (Agents.Num() == 0) { return FText::FromString(TEXT("0.0")); }
					auto Total = 0;
					for (const auto& Agent : Agents) { Total += Agent.NeighborCount; }
					return FText::FromString(FString::Printf(TEXT("%.1f"), static_cast<float>(Total) / Agents.Num()));
				})
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SCkDebug_KeyValueRow)
				.KeyText(FText::FromString(TEXT("Max neighbors")))
				.Tone(ECkDebug_KeyValueTone::Custom)
				.CustomValueColor(NumColor)
				.ValueText_Lambda([this]
				{
					if (NOT _ViewModel.IsValid()) { return FText::FromString(TEXT("—")); }
					auto MaxN = 0;
					for (const auto& Agent : _ViewModel->Get_AllAgents()) { MaxN = FMath::Max(MaxN, Agent.NeighborCount); }
					return FText::AsNumber(MaxN);
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Awake / Asleep / Replanning / Failed populate in Gate 4+. Neighbor query ms breakdown lands when 3B's perf scope is wired in.")))
				.ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
				.AutoWrapText(true)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------
