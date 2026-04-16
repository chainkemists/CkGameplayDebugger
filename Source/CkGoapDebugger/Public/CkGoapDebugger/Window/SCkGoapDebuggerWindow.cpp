#include "SCkGoapDebuggerWindow.h"

#include "SCkGoapDebugger_WorldStatePanel.h"
#include "SCkGoapDebugger_StatsPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraphSchema.h"
#include "CkGoapDebugger/Graph/CkGoapDebugNode_Action.h"

#include "GraphEditor.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	Construct(const FArguments& InArgs)
	-> void
{
	_ViewModel = MakeShared<FCkGoapDebugger_ViewModel>();

	_ViewModel->OnGoapListChanged.AddLambda([this](const auto&)
	{
		RefreshEntitySelector();
	});

	// Create the graph
	_Graph = NewObject<UCkGoapDebugGraph>(GetTransientPackage());
	_Graph->AddToRoot();
	_Graph->Schema = UCkGoapDebugGraphSchema::StaticClass();

	// Create graph editor
	SGraphEditor::FGraphEditorEvents GraphEvents;
	GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateRaw(
		this, &SCkGoapDebuggerWindow::OnGraphSelectionChanged);

	_GraphEditor = SNew(SGraphEditor)
		.GraphToEdit(_Graph)
		.IsEditable(true)
		.GraphEvents(GraphEvents);

	// Layout: Mockup D
	// toolbar → plan chain → SSplitter(H): [left-col(graph+timeline) | right-col(worldstate+details)]
	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]

		// Main content: SSplitter horizontal
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// Left column: graph + timeline
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				// Graph editor
				+ SSplitter::Slot()
				.Value(0.7f)
				[
					_GraphEditor.ToSharedRef()
				]

				// Timeline + History
				+ SSplitter::Slot()
				.Value(0.3f)
				[
					BuildTimelineAndHistory()
				]
			]

			// Right column: world state + details
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				// World State
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SAssignNew(_WorldStatePanel, SCkGoapDebugger_WorldStatePanel)
					.ViewModel(_ViewModel)
				]

				// Action Details
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SAssignNew(_ActionDetailPanel, SCkGoapDebugger_StatsPanel)
					.ViewModel(_ViewModel)
				]
			]
		]
	];
}

// ====================================================================================================================

SCkGoapDebuggerWindow::~SCkGoapDebuggerWindow()
{
	if (_Graph != nullptr)
	{
		_Graph->RemoveFromRoot();
		_Graph = nullptr;
	}
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
	-> void
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

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

	// Update status badge
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
			_StatusBadge->SetColorAndOpacity(CkGoapDebuggerStyle::TextMuted);
		}
	}

	// Update plan history list
	if (_HistoryListBox.IsValid())
	{
		const auto* History = _ViewModel->Get_PlanHistory(_ViewModel->Get_SelectedEntityHandle());
		const auto HistoryCount = History ? History->Num() : 0;

		if (HistoryCount != _LastHistoryCount)
		{
			_LastHistoryCount = HistoryCount;
			_HistoryListBox->ClearChildren();

			if (History != nullptr)
			{
				for (auto Idx = History->Num() - 1; Idx >= 0; --Idx)
				{
					const auto& Entry = (*History)[Idx];
					const auto StatusColor = CkGoapDebuggerStyle::GetStatusColor(Entry.FinalStatus);
					const auto StatusText = CkGoapDebuggerStyle::GetStatusString(Entry.FinalStatus);

					_HistoryListBox->AddSlot()
					.AutoHeight()
					.Padding(8.0f, 2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("GenericWhiteBox"))
							.ColorAndOpacity(StatusColor)
							.DesiredSizeOverride(FVector2D(8.0f, 8.0f))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%s — %d actions, cost %.0f"),
								*StatusText, Entry.PlanLength, Entry.PlanCost)))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("F#%lld"), Entry.FrameNumber)))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
						]
					];
				}
			}

			if (HistoryCount == 0)
			{
				_HistoryListBox->AddSlot()
				.AutoHeight()
				.Padding(8.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("No plan history yet")))
					.ColorAndOpacity(CkGoapDebuggerStyle::TextMuted)
				];
			}
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

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	BuildToolbar()
	-> TSharedRef<SWidget>
{
	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.06f, 0.08f, 0.13f))
		.Padding(FMargin(8.0f, 6.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Entity:")))
				.ColorAndOpacity(CkGoapDebuggerStyle::TextSecondary)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
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

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
			[
				SAssignNew(_StatusBadge, STextBlock)
				.Text(FText::FromString(TEXT("Idle")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Name")))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f))
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
					if (NOT _Graph) { return FText::FromString(TEXT("1")); }
					auto D = _Graph->NameDepth;
					return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
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

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
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
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("H"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f)) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("-"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingX=FMath::Max(100,_Graph->SpacingX-50);_Graph->ForceRebuild();} return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(STextBlock).Text_Lambda([this](){ return FText::FromString(_Graph?FString::Printf(TEXT("%d"),_Graph->SpacingX):TEXT("300")); }).Font(FCoreStyle::GetDefaultFontStyle("Bold",9)).Justification(ETextJustify::Center).MinDesiredWidth(32.0f) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("+"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingX=FMath::Min(800,_Graph->SpacingX+50);_Graph->ForceRebuild();} return FReply::Handled(); }) ]

			// V Spacing
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("V"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.55f)) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("-"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingY=FMath::Max(40,_Graph->SpacingY-20);_Graph->ForceRebuild();} return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(STextBlock).Text_Lambda([this](){ return FText::FromString(_Graph?FString::Printf(TEXT("%d"),_Graph->SpacingY):TEXT("100")); }).Font(FCoreStyle::GetDefaultFontStyle("Bold",9)).Justification(ETextJustify::Center).MinDesiredWidth(32.0f) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[ SNew(SButton).Text(FText::FromString(TEXT("+"))).OnClicked_Lambda([this]() { if(_Graph){_Graph->SpacingY=FMath::Min(400,_Graph->SpacingY+20);_Graph->ForceRebuild();} return FReply::Handled(); }) ]

			+ SHorizontalBox::Slot().FillWidth(1.0f)

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Zoom to Fit")))
				.OnClicked_Lambda([this]()
				{
					if (_GraphEditor.IsValid()) { _GraphEditor->ZoomToFit(false); }
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
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
					_ViewModel->Set_Paused(NOT _ViewModel->Get_Paused());
					return FReply::Handled();
				})
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
	BuildTimelineAndHistory()
	-> TSharedRef<SWidget>
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(CkGoapDebuggerStyle::PanelPadding, 6.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Plan History")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			.ColorAndOpacity(CkGoapDebuggerStyle::SectionHeader)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(_HistoryListBox, SVerticalBox)
			]
		];
}

// ====================================================================================================================
