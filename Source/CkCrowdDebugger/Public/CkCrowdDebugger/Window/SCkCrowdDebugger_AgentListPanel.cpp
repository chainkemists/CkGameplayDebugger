#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebuggerStyle.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	auto AgentRowText(const FCkCrowdDebugger_AgentSnapshot& InSnapshot) -> FString
	{
		return FString::Printf(TEXT("#%05u  %s"),
			GetTypeHash(InSnapshot.Handle),
			*InSnapshot.PrimaryTag);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentListPanel::Construct(const FArguments& InArgs) -> void
{
	_ViewModel = InArgs._ViewModel;

	if (_ViewModel.IsValid())
	{
		_OnListChangedHandle = _ViewModel->OnAgentListChanged.AddSP(
			SharedThis(this), &SCkCrowdDebugger_AgentListPanel::OnAgentListChanged);
	}

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(0))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("AGENT LIST")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(_ListView, SListView<ItemPtr>)
				.ListItemsSource(&_ItemSource)
				.OnGenerateRow(this, &SCkCrowdDebugger_AgentListPanel::OnGenerateRow)
				.OnSelectionChanged(this, &SCkCrowdDebugger_AgentListPanel::OnSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
		]
	];
}

// --------------------------------------------------------------------------------------------------------------------

SCkCrowdDebugger_AgentListPanel::~SCkCrowdDebugger_AgentListPanel()
{
	if (_ViewModel.IsValid() && _OnListChangedHandle.IsValid())
	{
		_ViewModel->OnAgentListChanged.Remove(_OnListChangedHandle);
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentListPanel::OnAgentListChanged(
	const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents)
	-> void
{
	_ItemSource.Reset(InAgents.Num());
	for (const auto& Agent : InAgents)
	{
		_ItemSource.Add(MakeShared<FCkCrowdDebugger_AgentSnapshot>(Agent));
	}

	if (_ListView.IsValid())
	{
		_ListView->RequestListRefresh();
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentListPanel::OnGenerateRow(
	ItemPtr InItem,
	const TSharedRef<STableViewBase>& InTable)
	-> TSharedRef<ITableRow>
{
	if (NOT InItem.IsValid())
	{
		return SNew(STableRow<ItemPtr>, InTable)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("(invalid)")))
		];
	}

	return SNew(STableRow<ItemPtr>, InTable)
		.Padding(FMargin(8, 3))
		[
			SNew(STextBlock).Text(FText::FromString(AgentRowText(*InItem)))
		];
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_AgentListPanel::OnSelectionChanged(
	ItemPtr InItem,
	ESelectInfo::Type InSelectInfo)
	-> void
{
	if (NOT _ViewModel.IsValid())
	{ return; }

	if (NOT InItem.IsValid())
	{
		_ViewModel->Set_SelectedHandle(FCk_Handle{});
		return;
	}

	_ViewModel->Set_SelectedHandle(InItem->Handle);
}

// --------------------------------------------------------------------------------------------------------------------
