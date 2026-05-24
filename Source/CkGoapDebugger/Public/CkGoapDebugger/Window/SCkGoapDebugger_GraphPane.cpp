#include "SCkGoapDebugger_GraphPane.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraph.h"
#include "CkGoapDebugger/Graph/CkGoapDebugGraphSchema.h"
#include "CkGoapDebugger/Graph/CkGoapDebugNode_Action.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "GraphEditor.h"
#include "GraphEditorActions.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "UObject/Package.h"

// ====================================================================================================================

SCkGoapDebugger_GraphPane::SCkGoapDebugger_GraphPane() = default;

SCkGoapDebugger_GraphPane::~SCkGoapDebugger_GraphPane()
{
    if (_OnChangedHandle.IsValid() && _ViewModel.IsValid())
    {
        _ViewModel->OnChanged.Remove(_OnChangedHandle);
        _OnChangedHandle.Reset();
    }

    if (_Graph && UObjectInitialized())
    {
        _Graph->ForceClear();
        _Graph->RemoveFromRoot();
        _Graph = nullptr;
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_GraphPane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    // Transient, GC-rooted graph — mirrors CkSmDebugger.
    _Graph = NewObject<UCkGoapDebugGraph>(GetTransientPackage());
    _Graph->AddToRoot();
    _Graph->Schema = UCkGoapDebugGraphSchema::StaticClass();

    SGraphEditor::FGraphEditorEvents Events;
    Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SCkGoapDebugger_GraphPane::OnGraphSelectionChanged);

    _GraphEditor = SNew(SGraphEditor)
        .GraphToEdit(_Graph)
        .IsEditable(false)
        .ShowGraphStateOverlay(false)
        .GraphEvents(Events);

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            BuildHeader()
                        ]

                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(_GraphHost, SBox)
                            [
                                _GraphEditor.ToSharedRef()
                            ]
                        ]
            ]
    ];

    if (_ViewModel.IsValid())
    {
        // Seed the graph from the ViewModel's shared name-depth so both stay
        // in sync on first paint.
        _Graph->NameDepth = _ViewModel->Get_NameDepth();
        _OnChangedHandle = _ViewModel->OnChanged.AddSP(this, &SCkGoapDebugger_GraphPane::RefreshFromViewModel);
    }

    // Initial population.
    RefreshFromViewModel();
}

// ====================================================================================================================

auto
    SCkGoapDebugger_GraphPane::
    Reset_ForWorldChange()
    -> void
{
    if (_Graph)
    {
        _Graph->ForceClear();
        _Graph->NotifyGraphChanged();
    }

    // Clear the cached topology so the next RefreshFromViewModel hits the
    // full-rebuild path (the in-place fast-path would mismatch handles
    // captured before the world swap).
    _LastTopologyHash = 0;
    _LastSelectedAction = FCk_Handle_Goap_Action{};

    if (_HeaderText.IsValid())
    {
        _HeaderText->SetText(FText::FromString(TEXT("Action graph - (no selection)")));
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_GraphPane::
    RefreshFromViewModel()
    -> void
{
    if (NOT _Graph) { return; }

    // Keep the graph's NameDepth in sync with the ViewModel's shared value so
    // the action card widgets see the active depth at construction time.
    if (_ViewModel.IsValid() && _Graph->NameDepth != _ViewModel->Get_NameDepth())
    {
        _Graph->NameDepth = _ViewModel->Get_NameDepth();
        _LastTopologyHash = 0;  // force a rebuild so existing nodes re-render
    }

    auto SelectedActionName = FString(TEXT("(none)"));
    auto ActionCount = 0;
    auto EdgeCount   = 0;

    const auto* PlannerInfo = _ViewModel.IsValid() ? _ViewModel->GetSelectedPlannerInfo() : nullptr;
    if (PlannerInfo == nullptr)
    {
        // No Planner selected — drop everything once, then short-circuit.
        // Clearing the cached topology hash forces a rebuild when a
        // Planner eventually arrives.
        if (_LastTopologyHash != 0)
        {
            _Graph->ForceClear();
            _Graph->NotifyGraphChanged();
            _LastTopologyHash = 0;
            _LastSelectedAction = FCk_Handle_Goap_Action{};
        }

        if (_HeaderText.IsValid())
        {
            _HeaderText->SetText(FText::FromString(TEXT("Action graph - (no Planner selected)")));
        }
        return;
    }

    const auto SelectedActionHandle = _ViewModel.IsValid()
        ? _ViewModel->GetSelectedAction()
        : FCk_Handle_Goap_Action{};

    // ----------------------------------------------------------------------
    // Topology gate: rebuild from scratch only when the catalog identity /
    // pin shape / goal owner actually changed. Otherwise update mutable
    // render hints (plan membership / step index / selection / failure) on
    // existing nodes in place. This is the central fix for the
    // action-graph flicker — every ViewModel broadcast (every plan attempt)
    // was previously tearing down all UEdGraphNodes and forcing the
    // SGraphEditor to recreate every Slate widget, which read as flicker
    // and empty viewport during fast replans.
    // ----------------------------------------------------------------------
    const auto NewTopologyHash = UCkGoapDebugGraph::ComputeTopologyHash(*PlannerInfo);
    const auto TopologyChanged = NewTopologyHash != _LastTopologyHash;
    const auto SelectionChanged = SelectedActionHandle != _LastSelectedAction;

    if (TopologyChanged)
    {
        // Full rebuild — RebuildFromSnapshot un-suppresses notifications
        // and calls NotifyGraphChanged at the end, so we don't need to
        // notify again here.
        _Graph->RebuildFromSnapshot(*PlannerInfo, SelectedActionHandle);
        _LastTopologyHash = NewTopologyHash;
        _LastSelectedAction = SelectedActionHandle;
    }
    else
    {
        // Mutable per-tick state may have shifted — refresh node hints in
        // place on the existing nodes. SGraphNode_GoapAction binds its
        // visuals (border color, plan-step badge, composite bar) via
        // TAttribute lambdas that read live from the node flags, so the
        // next paint pass will reflect the new state WITHOUT a
        // NotifyGraphChanged. Emitting NotifyGraphChanged here would
        // trigger SGraphPanel::OnGraphChanged → node-widget recreation,
        // which is the very flicker this fix exists to remove. We still
        // mirror selection into the SGraphEditor below so the framework's
        // selection box updates without rebuilding nodes.
        (void)_Graph->UpdateRuntimeState(*PlannerInfo, SelectedActionHandle);
        _LastSelectedAction = SelectedActionHandle;
    }

    ActionCount = _Graph->Get_ActionCount();
    EdgeCount   = _Graph->Get_EdgeCount();

    if (const auto* ActionInfo = _ViewModel.IsValid() ? _ViewModel->GetSelectedActionInfo() : nullptr)
    { SelectedActionName = ActionInfo->ClassName; }

    if (_HeaderText.IsValid())
    {
        const auto Header = FString::Printf(
            TEXT("Action graph - selected: %s - %d actions - %d edges"),
            *SelectedActionName, ActionCount, EdgeCount);
        _HeaderText->SetText(FText::FromString(Header));
    }

    // Mirror the ViewModel selection into the SGraphEditor only when it
    // actually drifted (avoids a per-broadcast Clear+SetSelection cycle
    // that would otherwise echo back through OnGraphSelectionChanged).
    if (_GraphEditor.IsValid() && (TopologyChanged || SelectionChanged))
    {
        _SuppressSelectionEcho = true;
        _GraphEditor->ClearSelectionSet();
        if (ck::IsValid(SelectedActionHandle))
        {
            if (auto* Node = _Graph->FindActionNode(SelectedActionHandle))
            { _GraphEditor->SetNodeSelection(Node, true); }
        }
        _SuppressSelectionEcho = false;
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_GraphPane::
    OnGraphSelectionChanged(
        const TSet<UObject*>& InSelection)
    -> void
{
    if (_SuppressSelectionEcho) { return; }
    if (NOT _ViewModel.IsValid()) { return; }

    for (auto* Obj : InSelection)
    {
        if (auto* ActionNode = Cast<UCkGoapDebugNode_Action>(Obj))
        {
            _ViewModel->SetSelectedAction(ActionNode->Get_ActionHandle());
            return;
        }
    }

    // Empty selection — clear the ViewModel's Action selection.
    _ViewModel->SetSelectedAction(FCk_Handle_Goap_Action{});
}

// ====================================================================================================================

auto
    SCkGoapDebugger_GraphPane::
    BuildHeader()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Black")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(_HeaderText, STextBlock)
                            .Text(FText::FromString(TEXT("Action graph - (no selection)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    ]

                // Right side action buttons — fit / 1:1 / hide-dimmed (stubs for D5).
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x2295 Fit")))
                            .ToolTipText(FText::FromString(TEXT("Fit graph to view (D5 stub)")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_GraphEditor.IsValid())
                                { _GraphEditor->ZoomToFit(/*bOnlySelection=*/ false); }
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("1:1")))
                            .ToolTipText(FText::FromString(TEXT("Reset zoom (D5 stub)")))
                            .OnClicked_Lambda([]() -> FReply
                            {
                                return FReply::Handled();
                            })
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x229F Hide dimmed")))
                            .ToolTipText(FText::FromString(TEXT("Hide off-plan nodes (D5 stub)")))
                            .OnClicked_Lambda([]() -> FReply
                            {
                                return FReply::Handled();
                            })
                    ]

                // ---- Name depth cycler -----------------------------------
                // Cycle: Full(0) → 1 → 2 → ... → MaxDepth → Full(0). Mirrors
                // the SM debugger's name-depth control. Setting depth flips
                // the topology hash via _LastTopologyHash reset so every
                // action card + the goal label re-render through the rebuild
                // path. Every other pane that renders class names reads from
                // the same Get_NameDepth source of truth.
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Name")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x25C0")))  // ◀
                            .ToolTipText(FText::FromString(TEXT("Shorter display name (fewer segments)")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_Graph != nullptr)
                                {
                                    auto& Depth = _Graph->NameDepth;
                                    const auto MaxDepth = _Graph->Get_MaxNameDepth();
                                    Depth = (Depth == 0) ? MaxDepth : (Depth - 1);
                                    if (_ViewModel.IsValid()) { _ViewModel->Set_NameDepth(Depth); }
                                    _LastTopologyHash = 0;
                                    RefreshFromViewModel();
                                    if (_ViewModel.IsValid()) { _ViewModel->Broadcast_Changed(); }
                                }
                                return FReply::Handled();
                            })
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]() -> FText
                            {
                                if (_Graph == nullptr) { return FText::FromString(TEXT("1")); }
                                const auto D = _Graph->NameDepth;
                                return FText::FromString(D == 0 ? TEXT("Full") : FString::Printf(TEXT("%d"), D));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .Justification(ETextJustify::Center)
                            .MinDesiredWidth(28.0f)
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Primary))
                    ]
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Text(FText::FromString(TEXT("\x25B6")))  // ▶
                            .ToolTipText(FText::FromString(TEXT("Longer display name (more segments)")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (_Graph != nullptr)
                                {
                                    auto& Depth = _Graph->NameDepth;
                                    const auto MaxDepth = _Graph->Get_MaxNameDepth();
                                    Depth = (Depth >= MaxDepth) ? 0 : (Depth + 1);
                                    if (_ViewModel.IsValid()) { _ViewModel->Set_NameDepth(Depth); }
                                    _LastTopologyHash = 0;
                                    RefreshFromViewModel();
                                    if (_ViewModel.IsValid()) { _ViewModel->Broadcast_Changed(); }
                                }
                                return FReply::Handled();
                            })
                    ]
        ];
}

// ====================================================================================================================
