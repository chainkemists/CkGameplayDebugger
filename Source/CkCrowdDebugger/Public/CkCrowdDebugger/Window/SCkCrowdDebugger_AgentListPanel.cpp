#include "CkCrowdDebugger/Window/SCkCrowdDebugger_AgentListPanel.h"

#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_CategoryDot.h"

#include "Styling/CoreStyle.h"
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
		// Gate 3 — `Nbrs` column. Right-padded so columns line up at small list widths;
		// formatted as "n=NN" rather than a bare number to keep tag strings unambiguous.
		return FString::Printf(TEXT("#%05u  n=%-2d  %s"),
			GetTypeHash(InSnapshot.Handle),
			InSnapshot.NeighborCount,
			*InSnapshot.PrimaryTag);
	}

	// Status colour — routed through the shared debugger palette so it matches the viewport dots
	// and the detail-panel verdict. Failed is red and prominent so a misbehaving agent is the first
	// thing a dev sees; Idle and the rest stay muted so they don't compete with anomalies.
	auto StatusColor(ECkCrowdDebugger_AgentStatus InStatus) -> FLinearColor
	{
		switch (InStatus)
		{
			case ECkCrowdDebugger_AgentStatus::Failed:     return CkDebugStyle::Err();
			case ECkCrowdDebugger_AgentStatus::Walking:    return CkDebugStyle::Info();
			case ECkCrowdDebugger_AgentStatus::Replanning: return CkDebugStyle::Warn();
			case ECkCrowdDebugger_AgentStatus::Asleep:     return CkDebugStyle::TextMute();
			case ECkCrowdDebugger_AgentStatus::Idle:       return CkDebugStyle::TextDim();
			default:                                       return CkDebugStyle::TextDim();
		}
	}

	auto StatusLabel(const FCkCrowdDebugger_AgentSnapshot& InSnapshot) -> FString
	{
		switch (InSnapshot.Status)
		{
			case ECkCrowdDebugger_AgentStatus::Failed:
				// Surface the fail reason inline — that's the actionable data. "FAIL: NoNavData"
				// instantly says "the navmesh isn't there" without needing to open Agent Detail.
				return FString::Printf(TEXT("FAIL: %s"),
					*StaticEnum<ECk_Nav_PathFailReason>()->GetNameStringByValue(static_cast<int64>(InSnapshot.PathFailReason)));
			case ECkCrowdDebugger_AgentStatus::Walking:    return TEXT("Walking");
			case ECkCrowdDebugger_AgentStatus::Replanning: return TEXT("Pending");
			case ECkCrowdDebugger_AgentStatus::Asleep:     return TEXT("Asleep");
			case ECkCrowdDebugger_AgentStatus::Idle:       return TEXT("Idle");
			default:                                       return TEXT("—");
		}
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
		.BorderImage(CkDebugStyle::GetFilledBrush())
		.BorderBackgroundColor(FSlateColor(CkDebugStyle::Bg1()))
		.Padding(FMargin(0))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("AGENT LIST")))
				.ColorAndOpacity(FSlateColor(CkDebugStyle::PaneHeadingColor()))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", CkDebugStyle::PaneHeadingFontSize()))
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
	// Gate 3 — broadcast fires every frame for live data (neighbor count, status). To keep
	// SListView selection alive, we mutate existing TSharedPtr<AgentSnapshot> contents in
	// place rather than rebuild _ItemSource. SListView keys selection by pointer identity,
	// so a wholesale rebuild every tick was eating user clicks before they resolved into
	// selection. We only re-allocate items when the agent set itself changes (count diff
	// or handle mismatch at any index).
	const auto NeedsRebuild = [&]() -> bool
	{
		if (_ItemSource.Num() != InAgents.Num())
		{ return true; }

		for (auto i = 0; i < InAgents.Num(); ++i)
		{
			if (NOT _ItemSource[i].IsValid() || _ItemSource[i]->Handle != InAgents[i].Handle)
			{ return true; }
		}
		return false;
	}();

	if (NeedsRebuild)
	{
		_ItemSource.Reset(InAgents.Num());
		for (const auto& Agent : InAgents)
		{
			_ItemSource.Add(MakeShared<FCkCrowdDebugger_AgentSnapshot>(Agent));
		}
	}
	else
	{
		// In-place content update — same TSharedPtr identities, fresh data inside.
		for (auto i = 0; i < InAgents.Num(); ++i)
		{
			*_ItemSource[i] = InAgents[i];
		}
	}

	if (_ListView.IsValid())
	{
		_ListView->RequestListRefresh();

		// Only re-stamp selection on rebuild — otherwise the user's in-flight click is
		// preserved by the unchanged TSharedPtr identity.
		if (NeedsRebuild && _ViewModel.IsValid())
		{
			const auto SelectedHandle = _ViewModel->Get_SelectedHandle();
			if (ck::IsValid(SelectedHandle))
			{
				for (const auto& Item : _ItemSource)
				{
					if (Item.IsValid() && Item->Handle == SelectedHandle)
					{
						_ListView->SetSelection(Item, ESelectInfo::Direct);
						break;
					}
				}
			}
		}
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

	// Bind text to a lambda so live mutations to the underlying snapshot (Gate 3+: neighbor
	// count, status, etc.) propagate per-frame without regenerating the row widget. Static
	// `Text(...)` would freeze the row at the values present at OnGenerateRow time.
	//
	// The leading SCkDebug_CategoryDot shows the agent's identity colour — same colour the
	// in-world capsule + breadcrumb + planned-path use, sourced from Get_DebugColor (which
	// falls back to a hash-derived stable colour for agents that never opted in).
	const auto WeakItem = TWeakPtr<FCkCrowdDebugger_AgentSnapshot>(InItem);
	auto AgentHandle = InItem->Handle;
	auto AgentColor = FLinearColor::White;
	if (auto CrowdAgent = UCk_Utils_CrowdAgent_UE::Cast(AgentHandle); ck::IsValid(CrowdAgent))
	{
		AgentColor = UCk_Utils_CrowdAgent_UE::Get_DebugColor(CrowdAgent);
	}

	return SNew(STableRow<ItemPtr>, InTable)
		.Padding(FMargin(8, 3))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SCkDebug_CategoryDot)
					.Color(AgentColor)
					.Diameter(10.0f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakItem]() -> FText
				{
					const auto Pinned = WeakItem.Pin();
					if (NOT Pinned.IsValid())
					{ return FText::GetEmpty(); }
					return FText::FromString(AgentRowText(*Pinned));
				})
			]
			// Status badge — colour bound to a lambda so live status changes (Idle → Walking →
			// Failed) repaint without us regenerating the row widget.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakItem]() -> FText
				{
					const auto Pinned = WeakItem.Pin();
					if (NOT Pinned.IsValid())
					{ return FText::GetEmpty(); }
					return FText::FromString(StatusLabel(*Pinned));
				})
				.ColorAndOpacity_Lambda([WeakItem]() -> FSlateColor
				{
					const auto Pinned = WeakItem.Pin();
					if (NOT Pinned.IsValid())
					{ return FSlateColor(FLinearColor::Gray); }
					return FSlateColor(StatusColor(Pinned->Status));
				})
			]
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
