#include "SCkGoapDebuggerWindow.h"

#include "SCkGoapDebugger_WorldStatePanel.h"
#include "SCkGoapDebugger_StatsPanel.h"
#include "SCkGoapDebugger_FailureAnalysisPanel.h"
#include "SCkGoapDebug_PlanStrip.h"
#include "SCkGoapDebug_HistoryRail.h"
#include "MacroNodes/SCkGoapDebug_MacroNodesPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraphSchema.h"
#include "CkGoapDebugger/Graph/CkGoapDebugNode_Action.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "GraphEditor.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

// ====================================================================================================================

const FName SCkGoapDebuggerWindow::WindowId = FName(TEXT("GoapDebugger"));

auto
	SCkGoapDebuggerWindow::
	Construct(const FArguments& InArgs)
	-> void
{
	Register_WithGate();

	_ViewModel = MakeShared<FCkGoapDebugger_ViewModel>();

	_ViewModel->OnGoapListChanged.AddLambda([this](const auto&)
	{
		RefreshEntitySelector();
	});

	_Graph = NewObject<UCkGoapDebugGraph>(GetTransientPackage());
	_Graph->AddToRoot();
	_Graph->Schema = UCkGoapDebugGraphSchema::StaticClass();

	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateRaw(
		this, &SCkGoapDebuggerWindow::OnGraphSelectionChanged);

	_GraphEditor = SNew(SGraphEditor)
		.GraphToEdit(_Graph)
		.IsEditable(true)
		.GraphEvents(GraphEvents);

	// Layout (mockup 5):
	//   toolbar
	//   diagnostics banner
	//   H-split: [ history rail | center (tabs + viewport + plan strip) | inspector stack ]
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(_DiagnosticsBanner, SVerticalBox)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// LEFT RAIL — Plan History
			+ SSplitter::Slot()
			.Value(0.18f)
			[
				SAssignNew(_HistoryRail, SCkGoapDebug_HistoryRail)
				.ViewModel(_ViewModel)
				.Graph(_Graph)
			]

			// CENTER — top tabs (Graph | Macro) + resizable hero plan strip
			+ SSplitter::Slot()
			.Value(0.55f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.78f)
				[
					BuildTopTabs()
				]

				+ SSplitter::Slot()
				.Value(0.22f)
				[
					SAssignNew(_PlanStrip, SCkGoapDebug_PlanStrip)
					.ViewModel(_ViewModel)
					.Graph(_Graph)
					.OnStepClicked(FOnCkGoapDebug_PlanStepClicked::CreateSP(this, &SCkGoapDebuggerWindow::OnPlanStepClicked))
				]
			]

			// RIGHT RAIL — stacked inspectors (World State / Action Details / Failure Analysis)
			+ SSplitter::Slot()
			.Value(0.27f)
			[
				BuildRightInspectorStack()
			]
		]
	];
}

// ====================================================================================================================

SCkGoapDebuggerWindow::~SCkGoapDebuggerWindow()
{
	// At engine shutdown the UObject array may already be torn down by the
	// time this Slate widget is destroyed. Skip RemoveFromRoot in that case.
	if (_Graph != nullptr && UObjectInitialized())
	{
		_Graph->RemoveFromRoot();
	}
	_Graph = nullptr;
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Gate: skip the whole data-collection + graph-update chain when the
	// window is hidden / paused / rate-capped. Huge editor-CPU win.
	if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
	{ return; }

	// Find PIE world
	auto* PieWorld = static_cast<UWorld*>(nullptr);
	if (GEngine != nullptr)
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				PieWorld = Context.World();
				break;
			}
		}
	}

	_CachedWorld = PieWorld;
	_ViewModel->Tick(PieWorld, InDeltaTime);

	// Update graph from current data
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info != nullptr)
	{
		_Graph->UpdateFromGoapInfo(*Info);
	}

	RebuildDiagnosticsBanner();

	// Status badge
	if (_StatusBadge.IsValid())
	{
		if (Info != nullptr)
		{
			_StatusBadge->SetText(FText::FromString(CkGoapDebuggerStyle::GetStatusString(Info->PlanStatus)));
			_StatusBadge->SetColorAndOpacity(CkGoapDebuggerStyle::GetStatusColor(Info->PlanStatus));
		}
		else
		{
			_StatusBadge->SetText(FText::FromString(TEXT("No Entity")));
			_StatusBadge->SetColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()));
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	OnGraphSelectionChanged(const TSet<UObject*>& InSelection)
	-> void
{
	if (InSelection.Num() == 0)
	{
		_ActionDetailPanel->ClearSelection();
		return;
	}

	for (auto* Obj : InSelection)
	{
		auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Obj);
		if (ActionNode == nullptr) { continue; }

		const auto* Info = _ViewModel->Get_CurrentGoapInfo();
		if (Info == nullptr) { break; }

		for (const auto& Action : Info->Actions)
		{
			if (Action.ClassName == ActionNode->Get_ActionName())
			{
				_ActionDetailPanel->SetSelectedAction(&Action, ActionNode->Get_PlanStepIndex());
				return;
			}
		}
	}

	_ActionDetailPanel->ClearSelection();
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkGoapDebuggerWindow::
	OnMacroActionClicked(FString InClassName)
	-> void
{
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr || !_ActionDetailPanel.IsValid()) { return; }

	for (const auto& Action : Info->Actions)
	{
		if (Action.ClassName == InClassName)
		{
			_ActionDetailPanel->SetSelectedAction(&Action, INDEX_NONE);
			return;
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	SCkGoapDebuggerWindow::
	OnPlanStepClicked(FString InClassName)
	-> void
{
	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	if (Info == nullptr || !_ActionDetailPanel.IsValid()) { return; }

	auto StepIdx = -1;
	for (auto i = 0; i < Info->PlanActionNames.Num(); ++i)
	{
		if (Info->PlanActionNames[i] == InClassName) { StepIdx = i; break; }
	}

	for (const auto& Action : Info->Actions)
	{
		if (Action.ClassName == InClassName)
		{
			_ActionDetailPanel->SetSelectedAction(&Action, StepIdx);
			return;
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	BuildRightInspectorStack()
	-> TSharedRef<SWidget>
{
	SAssignNew(_WorldStatePanel, SCkGoapDebugger_WorldStatePanel)
		.ViewModel(_ViewModel);

	SAssignNew(_ActionDetailPanel, SCkGoapDebugger_StatsPanel)
		.ViewModel(_ViewModel);

	SAssignNew(_FailureAnalysisPanel, SCkGoapDebugger_FailureAnalysisPanel)
		.ViewModel(_ViewModel);

	return SNew(SSplitter)
		.Orientation(Orient_Vertical)

		+ SSplitter::Slot()
		.Value(0.34f)
		[
			SNew(SCkDebug_InspectorPanel)
			.Title(FText::FromString(TEXT("World State")))
			.Body()
			[
				_WorldStatePanel.ToSharedRef()
			]
		]

		+ SSplitter::Slot()
		.Value(0.40f)
		[
			SNew(SCkDebug_InspectorPanel)
			.Title(FText::FromString(TEXT("Action Details")))
			.Body()
			[
				_ActionDetailPanel.ToSharedRef()
			]
		]

		+ SSplitter::Slot()
		.Value(0.26f)
		[
			SNew(SCkDebug_InspectorPanel)
			.Title(FText::FromString(TEXT("Failure Analysis")))
			.Body()
			[
				_FailureAnalysisPanel.ToSharedRef()
			]
		];
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	BuildTopTabs()
	-> TSharedRef<SWidget>
{
	SAssignNew(_MacroNodesPanel, SCkGoapDebug_MacroNodesPanel)
		.ViewModel(_ViewModel)
		.Graph(_Graph)
		.OnActionClicked(FOnCkGoapDebugMacroActionClicked::CreateSP(this, &SCkGoapDebuggerWindow::OnMacroActionClicked));

	auto TabButton = [this](int32 InIndex, const FString& InLabel) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ContentPadding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceS))
			.OnClicked_Lambda([this, InIndex]()
			{
				_TopTabIndex = InIndex;
				if (_TopTabSwitcher.IsValid()) { _TopTabSwitcher->SetActiveWidgetIndex(InIndex); }
				return FReply::Handled();
			})
			[
				SNew(SBorder)
				.BorderImage(CkDebugStyle::GetFilledBrush())
				.BorderBackgroundColor_Lambda([this, InIndex]()
				{
					return _TopTabIndex == InIndex
						? FSlateColor(CkDebugStyle::OverlayOf(CkDebugStyle::Info(), 0.16f))
						: FSlateColor(FLinearColor::Transparent);
				})
				.Padding(FMargin(CkDebugStyle::SpaceM, 3.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(InLabel))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeBody()))
					.ColorAndOpacity_Lambda([this, InIndex]()
					{
						return _TopTabIndex == InIndex
							? FSlateColor(CkDebugStyle::Text())
							: FSlateColor(CkDebugStyle::TextDim());
					})
				]
			];
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(CkDebugStyle::GetFilledBrush())
			.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
			.Padding(FMargin(CkDebugStyle::SpaceL, 2.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f) [ TabButton(0, TEXT("Graph")) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f) [ TabButton(1, TEXT("Macro")) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(_TopTabSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)

			+ SWidgetSwitcher::Slot()
			[
				_GraphEditor.ToSharedRef()
			]

			+ SWidgetSwitcher::Slot()
			[
				_MacroNodesPanel.ToSharedRef()
			]
		];
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	BuildToolbar()
	-> TSharedRef<SWidget>
{
	// Toolbar renders directly against the editor panel background — no
	// explicit fill, or it turns into a gray layer-cake when nested inside
	// the editor chrome.
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(FMargin(CkDebugStyle::SpaceXL, CkDebugStyle::SpaceM))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceS, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Entity:")))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextDim()))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(CkDebugStyle::SpaceS, 0.0f)
			[
				SAssignNew(_EntitySelector, STextComboBox)
				.OptionsSource(&_EntitySelectorItems)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InSel, ESelectInfo::Type)
				{
					if (InSel.IsValid())
					{
						const auto Idx = _EntitySelectorItems.IndexOfByKey(InSel);
						if (_EntitySelectorHandles.IsValidIndex(Idx))
						{
							_ViewModel->Set_SelectedEntityHandle(_EntitySelectorHandles[Idx]);
							_Graph->ForceRebuild();
						}
					}
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceM, 0.0f)
			[
				SAssignNew(_StatusBadge, STextBlock)
				.Text(FText::FromString(TEXT("Idle")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeBody()))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Name")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute()))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("\x25C0")))
				.OnClicked_Lambda([this]()
				{
					if (_Graph)
					{
						auto& Depth = _Graph->NameDepth;
						if (Depth > 1) { --Depth; }
						else { Depth = 0; }
						_Graph->ForceRebuild();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (!_Graph) { return FText::FromString(TEXT("1")); }
					auto D = _Graph->NameDepth;
					return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeSmall()))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::Text()))
				.Justification(ETextJustify::Center)
				.MinDesiredWidth(24.0f)
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("\x25B6")))
				.OnClicked_Lambda([this]()
				{
					if (_Graph)
					{
						auto& Depth = _Graph->NameDepth;
						if (Depth == 0) { Depth = 1; }
						else { ++Depth; }
						_Graph->ForceRebuild();
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Relayout")))
				.OnClicked_Lambda([this]()
				{
					if (_Graph) { _Graph->ForceRebuild(); }
					return FReply::Handled();
				})
			]

			// H Spacing
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceM, 0.0f, 0.0f, 0.0f)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("H"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro())).ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute())) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("-"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingX=FMath::Max(100,_Graph->SpacingX-50);_Graph->ForceRebuild();} return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(STextBlock).Text_Lambda([this](){ return FText::FromString(_Graph?FString::Printf(TEXT("%d"),_Graph->SpacingX):TEXT("300")); }).Font(FCoreStyle::GetDefaultFontStyle("Bold",CkDebugStyle::FontSizeSmall())).Justification(ETextJustify::Center).MinDesiredWidth(32.0f) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("+"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingX=FMath::Min(800,_Graph->SpacingX+50);_Graph->ForceRebuild();} return FReply::Handled(); }) ]

			// V Spacing
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("V"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeMicro())).ColorAndOpacity(FSlateColor(CkDebugStyle::TextMute())) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("-"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingY=FMath::Max(40,_Graph->SpacingY-20);_Graph->ForceRebuild();} return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(STextBlock).Text_Lambda([this](){ return FText::FromString(_Graph?FString::Printf(TEXT("%d"),_Graph->SpacingY):TEXT("100")); }).Font(FCoreStyle::GetDefaultFontStyle("Bold",CkDebugStyle::FontSizeSmall())).Justification(ETextJustify::Center).MinDesiredWidth(32.0f) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("+"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingY=FMath::Min(400,_Graph->SpacingY+20);_Graph->ForceRebuild();} return FReply::Handled(); }) ]

			+ SHorizontalBox::Slot().FillWidth(1.0f)

			+ SHorizontalBox::Slot().AutoWidth().Padding(CkDebugStyle::SpaceS, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Back to Live")))
				.Visibility_Lambda([this]()
				{
					return (_ViewModel.IsValid() && _ViewModel->Get_ViewMode() == ECk_GoapDebugger_ViewMode::Scrub)
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]()
				{
					if (_ViewModel.IsValid())
					{
						_ViewModel->Set_ViewMode(ECk_GoapDebugger_ViewMode::Live);
						if (_Graph) { _Graph->ForceRebuild(); }
					}
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(CkDebugStyle::SpaceS, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Zoom to Fit")))
				.OnClicked_Lambda([this]()
				{
					if (_GraphEditor.IsValid()) { _GraphEditor->ZoomToFit(false); }
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(CkDebugStyle::SpaceS, 0.0f)
			[
				SNew(SButton)
				.Text_Lambda([this]()
				{
					return _ViewModel->Get_Paused()
						? FText::FromString(TEXT("Resume"))
						: FText::FromString(TEXT("Pause"));
				})
				.OnClicked_Lambda([this]()
				{
					_ViewModel->Set_Paused(!_ViewModel->Get_Paused());
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(CkDebugStyle::SpaceL, 0.0f, 0.0f, 0.0f)
			[
				SNew(SCkDebugger_RefreshControls)
					.WindowId(SCkGoapDebuggerWindow::WindowId)
			]
		];
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	RefreshEntitySelector()
	-> void
{
	_EntitySelectorItems.Reset();
	_EntitySelectorHandles.Reset();

	for (const auto& Info : _ViewModel->Get_AllGoapEntities())
	{
		_EntitySelectorItems.Add(MakeShared<FString>(Info.DebugName));
		_EntitySelectorHandles.Add(Info.Handle);
	}

	if (_EntitySelector.IsValid())
	{
		_EntitySelector->RefreshOptions();
		if (_EntitySelectorItems.Num() > 0)
		{
			_EntitySelector->SetSelectedItem(_EntitySelectorItems[0]);
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	RebuildDiagnosticsBanner()
	-> void
{
	if (!_DiagnosticsBanner.IsValid()) { return; }

	const auto* Info = _ViewModel->Get_CurrentGoapInfo();
	const auto& Diag = Info ? Info->Diagnostics : FCkGoapDebugger_Diagnostics{};

	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(Diag.DependencyCycles.Num()));
	Hash = HashCombine(Hash, GetTypeHash(Diag.UnreachableGoalConditions.Num()));
	Hash = HashCombine(Hash, GetTypeHash(Diag.LastFailedGoalName));
	for (const auto& Cycle : Diag.DependencyCycles)
	{
		for (const auto& N : Cycle.ActionNames) { Hash = HashCombine(Hash, GetTypeHash(N)); }
	}
	for (const auto& C : Diag.UnreachableGoalConditions)
	{
		Hash = HashCombine(Hash, GetTypeHash(C.Key));
		Hash = HashCombine(Hash, GetTypeHash(C.Value));
	}

	if (Hash == _LastDiagnosticsHash) { return; }
	_LastDiagnosticsHash = Hash;

	_DiagnosticsBanner->ClearChildren();
	if (!Diag.HasAnyWarning()) { return; }

	const auto WarnBg     = CkDebugStyle::OverlayOf(CkDebugStyle::Err(), 0.22f);
	const auto WarnText   = CkDebugStyle::OverlayOf(CkDebugStyle::Warn(), 0.95f);
	const auto HeaderText = CkDebugStyle::Warn();

	auto BuildRow = [&](const FString& InText, const FLinearColor& InColor) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::FromString(InText))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
			.ColorAndOpacity(FSlateColor(InColor))
			.AutoWrapText(true);
	};

	auto Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, CkDebugStyle::SpaceS)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("\u26A0  GOAP Graph Diagnostics")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::FontSizeH3()))
			.ColorAndOpacity(FSlateColor(HeaderText))
		];

	for (const auto& Cycle : Diag.DependencyCycles)
	{
		auto ActionList = FString::Join(Cycle.ActionNames, TEXT(", "));
		auto CondList = FString{};
		for (const auto& T : Cycle.CycleConditions)
		{
			if (CondList.Len() > 0) { CondList += TEXT(", "); }
			CondList += T.ToString();
		}
		const auto Line = FString::Printf(
			TEXT("Dependency cycle: %s  —  conditions [%s] cannot be produced unless seeded by the initial world state."),
			*ActionList, *CondList);
		Content->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[ BuildRow(Line, WarnText) ];
	}

	if (Diag.UnreachableGoalConditions.Num() > 0)
	{
		auto CondList = FString{};
		for (const auto& C : Diag.UnreachableGoalConditions)
		{
			if (CondList.Len() > 0) { CondList += TEXT(", "); }
			CondList += C.AsString();
		}
		const auto GoalText = Diag.LastFailedGoalName.IsEmpty()
			? FString(TEXT("last goal"))
			: FString::Printf(TEXT("goal [%s]"), *Diag.LastFailedGoalName);
		const auto Line = FString::Printf(
			TEXT("Unreachable from current world state: %s requires [%s]. Planner returned PlanFailed."),
			*GoalText, *CondList);
		Content->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[ BuildRow(Line, WarnText) ];
	}

	_DiagnosticsBanner->AddSlot().AutoHeight()
	[
		SNew(SBorder)
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(WarnBg))
		.Padding(FMargin(CkDebugStyle::SpaceL, CkDebugStyle::SpaceM))
		[
			Content
		]
	];
}

// ====================================================================================================================
