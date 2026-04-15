#include "SCkGoapDebuggerWindow.h"

#include "SCkGoapDebugger_PlanView.h"
#include "SCkGoapDebugger_WorldStatePanel.h"
#include "SCkGoapDebugger_GoalPanel.h"
#include "SCkGoapDebugger_StatsPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/STextComboBox.h"

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

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			BuildToolbar()
		]

		// Main content
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// Left panel: Plan + World State
			+ SSplitter::Slot()
			.Value(0.55f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(0.5f)
				[
					SAssignNew(_PlanView, SCkGoapDebugger_PlanView)
					.ViewModel(_ViewModel)
				]

				+ SVerticalBox::Slot()
				.FillHeight(0.5f)
				[
					SAssignNew(_WorldStatePanel, SCkGoapDebugger_WorldStatePanel)
					.ViewModel(_ViewModel)
				]
			]

			// Right panel: Goals + Stats
			+ SSplitter::Slot()
			.Value(0.45f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(0.5f)
				[
					SAssignNew(_GoalPanel, SCkGoapDebugger_GoalPanel)
					.ViewModel(_ViewModel)
				]

				+ SVerticalBox::Slot()
				.FillHeight(0.5f)
				[
					SAssignNew(_StatsPanel, SCkGoapDebugger_StatsPanel)
					.ViewModel(_ViewModel)
				]
			]
		]
	];
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

	// Update status badge
	if (_StatusBadge.IsValid())
	{
		const auto* Info = _ViewModel->Get_CurrentGoapInfo();
		if (Info != nullptr)
		{
			const auto StatusColor = CkGoapDebuggerStyle::GetStatusColor(Info->PlanStatus);
			const auto StatusText = CkGoapDebuggerStyle::GetStatusString(Info->PlanStatus);
			_StatusBadge->SetText(FText::FromString(StatusText));
			_StatusBadge->SetColorAndOpacity(StatusColor);
		}
		else
		{
			_StatusBadge->SetText(FText::FromString(TEXT("No Entity")));
			_StatusBadge->SetColorAndOpacity(CkGoapDebuggerStyle::TextMuted);
		}
	}
}

// ====================================================================================================================

auto
	SCkGoapDebuggerWindow::
	BuildToolbar()
	-> TSharedRef<SWidget>
{
	return SNew(SHorizontalBox)

		// Entity selector
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f)
		[
			SAssignNew(_EntitySelector, STextComboBox)
			.OptionsSource(&_EntitySelectorItems)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InSelected, ESelectInfo::Type)
			{
				if (InSelected.IsValid())
				{
					const auto Index = _EntitySelectorItems.IndexOfByKey(InSelected);
					if (_EntitySelectorHandles.IsValidIndex(Index))
					{
						_ViewModel->Set_SelectedEntityHandle(_EntitySelectorHandles[Index]);
					}
				}
			})
		]

		// Status badge
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f)
		[
			SAssignNew(_StatusBadge, STextBlock)
			.Text(FText::FromString(TEXT("Idle")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		// Spacer
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)

		// Pause button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f)
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
