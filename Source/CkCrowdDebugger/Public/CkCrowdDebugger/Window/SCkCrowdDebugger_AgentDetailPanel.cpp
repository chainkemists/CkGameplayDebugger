#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"

#include "CkCrowdDebugger/Commands/CkCrowdDebugger_PathNetworkCommand.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Handle/CkHandle.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"
#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	if (_ViewModel.IsValid())
	{
		_OnRefreshedHandle = _ViewModel->OnAgentDataRefreshed.AddSP(
			SharedThis(this), &SCkCrowdDebugger_AgentDetailPanel::OnAgentDataRefreshed);
	}

	const auto NumColor    = CkStyle::Value_Numeric();
	const auto MathColor   = CkStyle::Value_Math();
	const auto TagColor    = CkStyle::Value_Tag();
	const auto HandleColor = CkStyle::Value_Handle();

	const auto SectionPad = FMargin(0.0f, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(CkStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
		.Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// ---- Header: pane title + entity pill --------------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceS)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("AGENT DETAIL")))
						.ColorAndOpacity(FSlateColor(CkStyle::PaneHeadingColor()))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::PaneHeadingFontSize()))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCkDebug_EntityRef)
						.ShowName(true)
						.Entity_Lambda([this]() -> FCk_Handle
						{
							return _ViewModel.IsValid() ? _ViewModel->Get_SelectedHandle() : FCk_Handle{};
						})
					]
				]

				// ---- Stat strip: status pill + key live metrics ---------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, CkStyle::SpaceXS)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ StatusBadge() ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_StatPair)
						.ValueColor(FSlateColor(CkStyle::Value_Numeric()))
						.Label(FText::FromString(TEXT("cm/s")))
						.Value_Lambda([this]{ return _HasSelection ? FText::AsNumber(FMath::RoundToInt(Diag_Speed())) : FText::FromString(TEXT("—")); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_StatPair)
						.ValueColor(FSlateColor(CkStyle::Value_Numeric()))
						.Label(FText::FromString(TEXT("nbrs")))
						.Value_Lambda([this]{ return _HasSelection ? FText::AsNumber(_Snapshot.NeighborCount) : FText::FromString(TEXT("—")); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f)
					[
						SNew(SCkDebug_StatPair)
						.ValueColor(FSlateColor(CkStyle::Value_Numeric()))
						.Label(FText::FromString(TEXT("turn r")))
						.Value_Lambda([this]{ return _HasSelection ? FText::FromString(FString::Printf(TEXT("%.0f"), Diag_TurnRadius())) : FText::FromString(TEXT("—")); })
					]
				]

				// ---- Identity --------------------------------------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Identity"))) ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(TEXT("Entity")))
					.Tone(ECkDebug_KeyValueTone::Custom)
					.ValueWidget()
					[
						SNew(SCkDebug_EntityRef)
						.ShowName(false)
						.Entity_Lambda([this]() -> FCk_Handle { return _HasSelection ? _Snapshot.Handle : FCk_Handle{}; })
					]
				]
				// Who this agent belongs to — the crowd-agent feature entity is lifetime-owned
				// by the actual NPC/player entity. Pill click navigates to it in the ECS debugger.
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SCkDebug_KeyValueRow)
					.KeyText(FText::FromString(TEXT("Owner")))
					.Tone(ECkDebug_KeyValueTone::Custom)
					.ValueWidget()
					[
						SNew(SCkDebug_EntityRef)
						.ShowName(true)
						.Entity_Lambda([this]() -> FCk_Handle
						{
							if (NOT _HasSelection || ck::Is_NOT_Valid(_Snapshot.Handle))
							{ return {}; }
							return UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(_Snapshot.Handle);
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Primary tag"), [this]{ return _Snapshot.PrimaryTag; }, TagColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Tag count"),   [this]{ return FString::FromInt(_Snapshot.Tags.Num()); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Radius"),      [this]{ return FString::Printf(TEXT("%.1f cm"), _Snapshot.Radius); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Height"),      [this]{ return FString::Printf(TEXT("%.1f cm"), _Snapshot.Height); }, NumColor) ]

				// ---- Orbit Diagnosis -------------------------------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Orbit Diagnosis"))) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("MaxSpeed"),       [this]{ return FString::Printf(TEXT("%.0f cm/s"), _Snapshot.MaxSpeed); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("MaxTurnRate"),    [this]{ return FString::Printf(TEXT("%.2f rad/s"), _Snapshot.MaxTurnRate); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Speed (now)"),    [this]{ return FString::Printf(TEXT("%.0f cm/s"), Diag_Speed()); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Arrival r"),      [this]{ return FString::Printf(TEXT("%.0f cm"), Diag_EffArrival()); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Turn radius"),    [this]{ return FString::Printf(TEXT("%.0f cm"), Diag_TurnRadius()); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Orbit r (pred)"), [this]{ return FString::Printf(TEXT("%.0f cm"), Diag_PredictedOrbit()); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Dist to goal"),   [this]{ return _Snapshot.IsWalking ? FString::Printf(TEXT("%.0f cm"), FVector::Dist(_Snapshot.ActiveGoal, _Snapshot.Position)) : FString(TEXT("—")); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(CkStyle::GetRoundedBrush())
					.BorderBackgroundColor_Lambda([this]
					{
						const auto Tone = (_HasSelection && Diag_WillOrbit()) ? CkStyle::Err() : CkStyle::Ok();
						return FSlateColor(CkStyle::OverlayOf(Tone, 0.20f));
					})
					.Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeBody()))
						.ColorAndOpacity_Lambda([this]
						{
							return FSlateColor((_HasSelection && Diag_WillOrbit()) ? CkStyle::Err() : CkStyle::Ok());
						})
						.Text_Lambda([this]
						{
							if (NOT _HasSelection) { return FText::FromString(TEXT("(no agent selected)")); }
							return Diag_WillOrbit()
								? FText::FromString(FString::Printf(TEXT("WILL ORBIT — turn radius %.0f > arrival %.0f cm"), Diag_PredictedOrbit(), Diag_EffArrival()))
								: FText::FromString(TEXT("CAN SETTLE — turn radius fits the stop-ring"));
						})
					]
				]

				// ---- Debug Control (take over the agent) -----------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Debug Control"))) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SButton)
						.IsEnabled_Lambda([this]{ return _HasSelection; })
						.OnClicked(this, &SCkCrowdDebugger_AgentDetailPanel::Toggle_DebugOverride)
						.ToolTipText(FText::FromString(TEXT("Take manual control: the NPC AI stops issuing its own MoveTo for this agent. Right-click a destination on the 2D map to command it — commanding auto-takes control, so this button mostly matters for releasing back to the AI.")))
						[
							SNew(STextBlock).Text_Lambda([this]{ return Get_OverrideButtonText(); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.IsEnabled_Lambda([this]{ return _HasSelection && ck::DebugFocus::Get_CanFocus(); })
						.OnClicked_Lambda([this]() -> FReply
						{
							if (_HasSelection)
							{ ck::DebugFocus::Focus_Entity(_Snapshot.Handle); }
							return FReply::Handled();
						})
						.ToolTipText(FText::FromString(TEXT("Frame this agent in the editor viewport (auto-ejects while possessed).")))
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("Focus (F)")))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(CkStyle::Err()))
						.Text_Lambda([this]
						{
							return Get_HasDebugOverride()
								? FText::FromString(TEXT("DEBUG OVERRIDE — right-click a map destination to command"))
								: FText::GetEmpty();
						})
					]
				]

				// ---- Steering / Neighbors --------------------------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Steering / Neighbors"))) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Neighbors"),  [this]{ return FString::FromInt(_Snapshot.NeighborCount); }, NumColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Separation"), [this]{ return FString::Printf(TEXT("|%.0f|  w=%.2f"), _Snapshot.SeparationForce.Size(), _Snapshot.SeparationWeight); }, MathColor) ]
				+ SVerticalBox::Slot().AutoHeight()[ Kv(TEXT("Sep radius"), [this]{ return FString::Printf(TEXT("%.0f cm"), _Snapshot.SeparationRadius); }, NumColor) ]

				// ---- Tuners ----------------------------------------------------------------------
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[ SNew(SCkDebug_SectionHeader).Label(FText::FromString(TEXT("Tuners (live)"))) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeTunerRow(TEXT("MaxSpeed"),        [this]{ return _Tuner_MaxSpeed; },        [this](float InV){ _Tuner_MaxSpeed = InV; WriteSelectedParams(); },        60.0f, 600.0f, 10.0f) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeTunerRow(TEXT("MaxTurnRate"),     [this]{ return _Tuner_MaxTurnRate; },     [this](float InV){ _Tuner_MaxTurnRate = InV; WriteSelectedParams(); },     0.5f, 20.0f, 0.1f) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeTunerRow(TEXT("MaxAcceleration"), [this]{ return _Tuner_MaxAcceleration; }, [this](float InV){ _Tuner_MaxAcceleration = InV; WriteSelectedParams(); }, 120.0f, 2000.0f, 20.0f) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeTunerRow(TEXT("ArrivalRadius"),   [this]{ return _Tuner_ArrivalRadius; },   [this](float InV){ _Tuner_ArrivalRadius = InV; WriteSelectedParams(); },   10.0f, 120.0f, 5.0f) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeTunerRow(TEXT("SeparationRadius"),[this]{ return _Tuner_SeparationRadius; },[this](float InV){ _Tuner_SeparationRadius = InV; WriteSelectedParams(); },40.0f, 200.0f, 10.0f) ]
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_AgentDetailPanel::~SCkCrowdDebugger_AgentDetailPanel()
{
	if (_ViewModel.IsValid() && _OnRefreshedHandle.IsValid())
	{
		_ViewModel->OnAgentDataRefreshed.Remove(_OnRefreshedHandle);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Kv(
	const FString& InKey,
	TFunction<FString()> InValue,
	FLinearColor InColor)
	-> TSharedRef<SWidget>
{
	return SNew(SCkDebug_KeyValueRow)
		.KeyText(FText::FromString(InKey))
		.Tone(ECkDebug_KeyValueTone::Custom)
		.CustomValueColor(InColor)
		.ValueText_Lambda([this, InValue]()
		{
			return _HasSelection ? FText::FromString(InValue()) : FText::FromString(TEXT("—"));
		});
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Status_Text() const -> FText
{
	if (NOT _HasSelection) { return FText::FromString(TEXT("—")); }
	switch (_Snapshot.Status)
	{
		case ECkCrowdDebugger_AgentStatus::Walking:     return FText::FromString(TEXT("Walking"));
		case ECkCrowdDebugger_AgentStatus::Idle:        return FText::FromString(TEXT("Idle"));
		case ECkCrowdDebugger_AgentStatus::Replanning:  return FText::FromString(TEXT("Pending"));
		case ECkCrowdDebugger_AgentStatus::Failed:      return FText::FromString(TEXT("Failed"));
		case ECkCrowdDebugger_AgentStatus::Asleep:      return FText::FromString(TEXT("Asleep"));
		case ECkCrowdDebugger_AgentStatus::PlayerProxy: return FText::FromString(TEXT("Proxy"));
		default:                                        return FText::FromString(TEXT("—"));
	}
}

auto SCkCrowdDebugger_AgentDetailPanel::Status_Color() const -> FLinearColor
{
	if (NOT _HasSelection) { return CkStyle::TextMute(); }
	switch (_Snapshot.Status)
	{
		case ECkCrowdDebugger_AgentStatus::Walking:     return CkStyle::Info();
		case ECkCrowdDebugger_AgentStatus::Replanning:  return CkStyle::Warn();
		case ECkCrowdDebugger_AgentStatus::Failed:      return CkStyle::Err();
		case ECkCrowdDebugger_AgentStatus::Asleep:      return CkStyle::TextMute();
		case ECkCrowdDebugger_AgentStatus::PlayerProxy: return CkStyle::Accent();
		default:                                        return CkStyle::TextDim();
	}
}

auto SCkCrowdDebugger_AgentDetailPanel::StatusBadge() -> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderImage(CkStyle::GetRoundedBrush())
		.BorderBackgroundColor_Lambda([this]{ return FSlateColor(CkStyle::OverlayOf(Status_Color(), 0.22f)); })
		.Padding(FMargin(CkStyle::SpaceM, 2.0f))
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
			[
				SNew(SBox).WidthOverride(8.0f).HeightOverride(8.0f)
				[
					SNew(SImage)
					.Image(CkStyle::GetRoundedBrush())
					.ColorAndOpacity_Lambda([this]{ return FSlateColor(Status_Color()); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkStyle::FontSizeSmall()))
				.ColorAndOpacity_Lambda([this]{ return FSlateColor(Status_Color()); })
				.Text_Lambda([this]{ return Status_Text(); })
			]
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Diag_EffArrival() const -> float
{ return _Snapshot.IsWalking ? _Snapshot.ActiveArrivalRadius : _Snapshot.ArrivalRadius; }

auto SCkCrowdDebugger_AgentDetailPanel::Diag_Speed() const -> double
{ return _Snapshot.Velocity.Size(); }

auto SCkCrowdDebugger_AgentDetailPanel::Diag_TurnRadius() const -> double
{ return _Snapshot.MaxTurnRate > 0.0f ? Diag_Speed() / _Snapshot.MaxTurnRate : 0.0; }

auto SCkCrowdDebugger_AgentDetailPanel::Diag_PredictedOrbit() const -> float
{ return _Snapshot.MaxTurnRate > 0.0f ? _Snapshot.MaxSpeed / _Snapshot.MaxTurnRate : 0.0f; }

auto SCkCrowdDebugger_AgentDetailPanel::Diag_WillOrbit() const -> bool
{ return _Snapshot.MaxTurnRate > 0.0f && Diag_PredictedOrbit() > Diag_EffArrival(); }

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::MakeTunerRow(
	const FString& InLabel,
	TFunction<float()> InGet,
	TFunction<void(float)> InSet,
	float InMin,
	float InMax,
	float InDelta)
	-> TSharedRef<SWidget>
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center).Padding(0.0f, 1.0f, CkStyle::SpaceM, 1.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(InLabel))
			.ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
		]
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center).Padding(0.0f, 1.0f)
		[
			SNew(SSpinBox<float>)
			.MinValue(InMin).MaxValue(InMax)
			.MinSliderValue(InMin).MaxSliderValue(InMax)
			.Delta(InDelta)
			.IsEnabled_Lambda([this]() { return Get_TunersEnabled(); })
			.Value_Lambda([InGet]() { return InGet(); })
			.OnValueChanged_Lambda([InSet](float InV) { InSet(InV); })
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Get_TunersEnabled() const -> bool
{ return _HasSelection; }

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::WriteSelectedParams() -> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	auto Handle = _ViewModel->Get_SelectedHandle();
	if (ck::Is_NOT_Valid(Handle))
	{ return; }

	if (NOT Handle.Has<ck::FFragment_CrowdAgent_Params>())
	{ return; }

	auto& Params = Handle.Get<ck::FFragment_CrowdAgent_Params>();
	Params.Set_MaxSpeed(_Tuner_MaxSpeed);
	Params.Set_MaxTurnRate(_Tuner_MaxTurnRate);
	Params.Set_MaxAcceleration(_Tuner_MaxAcceleration);
	Params.Set_ArrivalRadius(_Tuner_ArrivalRadius);
	Params.Set_SeparationRadius(_Tuner_SeparationRadius);
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::Get_HasDebugOverride() const -> bool
{
	if (NOT _ViewModel.IsValid())
	{ return false; }
	auto Handle = _ViewModel->Get_SelectedHandle();
	auto Agent = UCk_Utils_CrowdAgent_UE::Cast(Handle);
	return ck::IsValid(Agent) && UCk_Utils_CrowdAgent_UE::Get_HasDebugOverride(Agent);
}

auto SCkCrowdDebugger_AgentDetailPanel::Get_OverrideButtonText() const -> FText
{
	return Get_HasDebugOverride()
		? FText::FromString(TEXT("Release to AI"))
		: FText::FromString(TEXT("Take Control"));
}

auto SCkCrowdDebugger_AgentDetailPanel::Toggle_DebugOverride() -> FReply
{
	if (NOT _ViewModel.IsValid())
	{ return FReply::Handled(); }
	auto Handle = _ViewModel->Get_SelectedHandle();
	auto Agent = UCk_Utils_CrowdAgent_UE::Cast(Handle);
	if (ck::Is_NOT_Valid(Agent))
	{ return FReply::Handled(); }

	const auto NewOverride = NOT UCk_Utils_CrowdAgent_UE::Get_HasDebugOverride(Agent);
	if (NewOverride == false)
	{ ck::crowd_debugger::ReleaseManualMove(Agent); }
	UCk_Utils_CrowdAgent_UE::Request_SetDebugOverride(Agent, NewOverride, {});
	return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentDetailPanel::OnAgentDataRefreshed(
	const FCkCrowdDebugger_AgentSnapshot* InSnapshot)
	-> void
{
	if (InSnapshot == nullptr)
	{
		_HasSelection = false;
		return;
	}

	_Snapshot = *InSnapshot;
	_HasSelection = true;
	_Tuner_MaxSpeed         = InSnapshot->MaxSpeed;
	_Tuner_MaxTurnRate      = InSnapshot->MaxTurnRate;
	_Tuner_MaxAcceleration  = InSnapshot->MaxAcceleration;
	_Tuner_ArrivalRadius    = InSnapshot->ArrivalRadius;
	_Tuner_SeparationRadius = InSnapshot->SeparationRadius;
}

// --------------------------------------------------------------------------------------------------------------------
