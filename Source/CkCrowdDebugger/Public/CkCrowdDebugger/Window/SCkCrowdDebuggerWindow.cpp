#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_ViewportPanel.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkCrowdDebuggerWindow::WindowId{TEXT("CkCrowdDebugger")};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Construct(const FArguments& InArgs) -> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkCrowdDebugger_ViewModel>();
	_WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();

	_NavmeshStatusPanel = SNew(SCkCrowdDebugger_NavmeshStatusPanel).ViewModel(_ViewModel);
	_AgentListPanel     = SNew(SCkCrowdDebugger_AgentListPanel).ViewModel(_ViewModel);
	_AgentDetailPanel   = SNew(SCkCrowdDebugger_AgentDetailPanel).ViewModel(_ViewModel);
	_StatsPanel         = SNew(SCkCrowdDebugger_StatsPanel).ViewModel(_ViewModel);
	_EventLogPanel      = SNew(SCkCrowdDebugger_EventLogPanel).ViewModel(_ViewModel);
	_ViewportPanel      = SNew(SCkCrowdDebugger_ViewportPanel).ViewModel(_ViewModel);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ BuildToolbar() ]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			// Left rail: navmesh status + agent list + stats + event log.
			+ SSplitter::Slot().Value(0.20f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.22f) [ _NavmeshStatusPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.46f) [ _AgentListPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.14f) [ _StatsPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.18f) [ _EventLogPanel.ToSharedRef() ]
			]
			// Center: the viewport, full height (the mockup's centerpiece).
			+ SSplitter::Slot().Value(0.52f)
			[
				_ViewportPanel.ToSharedRef()
			]
			// Right: agent detail + tuners + diagnostics, full height (no scrolling).
			+ SSplitter::Slot().Value(0.28f)
			[
				_AgentDetailPanel.ToSharedRef()
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
	SCkDebugger_WindowBase::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (NOT _ViewModel.IsValid())
	{ return; }

	// Resolve the inspected world via the shared selector. If none (editor idle,
	// no PIE), the data collector will early-out on a null world.
	_WorldModel->Ensure_AutoSelect();
	auto* World = _WorldModel->Get_SelectedWorld();

	_ViewModel->Tick(World, InDeltaTime);
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	// Read/write helpers for the two CkCrowd-side overlay CVars. The toolbar checkboxes call
	// these so any external `console set` is reflected on the next paint without us caching
	// state — the CVar is the single source of truth.
	auto Get_CVarBool(const TCHAR* InName) -> bool
	{
		const auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
		return CVar != nullptr && CVar->GetInt() != 0;
	}

	auto Set_CVarBool(const TCHAR* InName, bool InValue) -> void
	{
		auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
		if (CVar != nullptr)
		{ CVar->Set(InValue ? 1 : 0, ECVF_SetByConsole); }
	}
}

auto SCkCrowdDebuggerWindow::BuildToolbar() -> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.Padding(FMargin(8, 4))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("CK Crowd Debugger")))
			]
			// World selector (shared across all CK debuggers)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SCkDebug_WorldSelector, _WorldModel)
				.ShowHeaderLabel(false)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Run Health Check")))
				.ToolTipText(FText::FromString(TEXT("Run a synthetic FindPathSync probe (origin → +200) and surface the result in the Navmesh Status panel. Bypasses the request/processor pipeline entirely — a green probe proves the nav stack works in isolation from any gym wiring.")))
				.OnClicked_Lambda([this]() -> FReply
				{
					if (_ViewModel.IsValid())
					{ _ViewModel->Run_HealthCheckProbe(); }
					return FReply::Handled();
				})
			]
			// ---- Overlay toggles -----------------------------------------------------------
			// IsChecked is a lambda so external `console set` flips the visible state too.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 4, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw a breadcrumb trail (the agent's actually-traversed path) for every agent that has the recorder feature. The selected agent is always drawn regardless of this toggle.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.DrawBreadcrumbs")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.DrawBreadcrumbs"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Breadcrumbs")))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw the planned-path waypoints for every agent that has a path result. The selected agent is always drawn regardless of this toggle.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.DrawPlannedPaths")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.DrawPlannedPaths"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Planned Paths")))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw path-trouble diagnostics in the game world for every affected agent: marker, sidewalk/Unreal-nav status, attempted goal, dashed line, and Euclidean distance.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.DrawPathTrouble")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.DrawPathTrouble"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("World Trouble")))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw a body capsule + forward-facing cone for every crowd agent. Color comes from the agent's debug color (per-agent override or hash-derived stable fallback). PathPending agents tint yellow; Asleep agents desaturate.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.Debug.AgentBody")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.Debug.AgentBody"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Agent Body")))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw separation diagnostics for every awake crowd agent: yellow separation-radius circle on the floor, cyan lines to each neighbor in the steering cache, orange arrow showing the active separation force.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.Debug")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.Debug"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Separation")))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 8, 0)
			[
				SNew(SCheckBox)
				.ToolTipText(FText::FromString(TEXT("Draw the orbit-diagnosis rings for every crowd agent: arrival ring (green) + predicted-orbit ring (red) at the goal, the turn-radius circle (blue) tangent to the agent, and the velocity vector (yellow). The selected agent is always drawn regardless of this toggle.")))
				.IsChecked_Lambda([]() -> ECheckBoxState
				{
					return Get_CVarBool(TEXT("ck.Crowd.DrawAgentRings")) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState InNewState)
				{
					Set_CVarBool(TEXT("ck.Crowd.DrawAgentRings"), InNewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Rings")))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					if (NOT _ViewModel.IsValid())
					{ return FText::FromString(TEXT("(no view-model)")); }
					return FText::FromString(FString::Printf(TEXT("Agents: %d"), _ViewModel->Get_AgentCount()));
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("ck.CrowdDebugger 1")))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			]
		];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebuggerWindow::~SCkCrowdDebuggerWindow()
{
	_ViewModel.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
