#include "CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_NavmeshStatusPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentDetailPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_StatsPanel.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_EventLogPanel.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkCrowdDebuggerWindow::WindowId{TEXT("CkCrowdDebugger")};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebuggerWindow::Construct(const FArguments& InArgs) -> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkCrowdDebugger_ViewModel>();

	_NavmeshStatusPanel = SNew(SCkCrowdDebugger_NavmeshStatusPanel).ViewModel(_ViewModel);
	_AgentListPanel     = SNew(SCkCrowdDebugger_AgentListPanel).ViewModel(_ViewModel);
	_AgentDetailPanel   = SNew(SCkCrowdDebugger_AgentDetailPanel).ViewModel(_ViewModel);
	_StatsPanel         = SNew(SCkCrowdDebugger_StatsPanel).ViewModel(_ViewModel);
	_EventLogPanel      = SNew(SCkCrowdDebugger_EventLogPanel).ViewModel(_ViewModel);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight() [ BuildToolbar() ]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			+ SSplitter::Slot().Value(0.22f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.30f) [ _NavmeshStatusPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.70f) [ _AgentListPanel.ToSharedRef() ]
			]
			+ SSplitter::Slot().Value(0.53f)
			[
				_AgentDetailPanel.ToSharedRef()
			]
			+ SSplitter::Slot().Value(0.25f)
			[
				SNew(SSplitter).Orientation(Orient_Vertical)
				+ SSplitter::Slot().Value(0.40f) [ _StatsPanel.ToSharedRef() ]
				+ SSplitter::Slot().Value(0.60f) [ _EventLogPanel.ToSharedRef() ]
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

	// Find the first PIE world. If none (editor idle, no PIE), the data collector
	// will early-out.
	auto* World = static_cast<UWorld*>(nullptr);
	if (GEngine != nullptr)
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}

	_ViewModel->Tick(World, InDeltaTime);
}

// --------------------------------------------------------------------------------------------------------------------

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
