#include "CkSchedulerDebuggerPage_TreeView.h"

#include "CkCore/Format/CkFormat.h"
#include "CkDebuggerCommon/Graph/SCkDebug_GraphCanvas.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkSchedulerDebugger/Graph/SCkSchedulerProcessorCard.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebuggerStyle.h"
#include "CkSchedulerDebugger/Styles/CkSchedulerDebugger_Axes.h"
#include "CkSchedulerDebugger/ViewModel/CkSchedulerDebugger_ViewModel.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h"
#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_ProcessorTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

FCkSchedulerDebuggerPage_TreeView::~FCkSchedulerDebuggerPage_TreeView()
{
    if (_DetailGraphCanvas.IsValid())
    {
        _DetailGraphCanvas->Clear_InteractionDelegates();
    }
    if (_ViewModel.IsValid() && _SelectionChangedHandle.IsValid())
    {
        _ViewModel->OnSelectionChanged.Remove(_SelectionChangedHandle);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Tree View"));
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::Build_Content(
    TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget>
{
    _ViewModel = InViewModel;
    _DetailGraphContainer = SNew(SBox);

    if (_ViewModel.IsValid())
    {
        _SelectionChangedHandle = _ViewModel->OnSelectionChanged.AddRaw(
            this, &FCkSchedulerDebuggerPage_TreeView::DoOnSelectionChanged);
    }

    DoBuildDetailGraph();

    return SNew(SSplitter).Orientation(Orient_Horizontal)

           +
           SSplitter::Slot().Value(0.25f).MinSize(
                240.0f)[SNew(SCkDebug_PaneHost)[SAssignNew(_ProcessorTree, SCkSchedulerDebugger_ProcessorTree)
                                                     .ViewModel(InViewModel)]]

           + SSplitter::Slot().Value(0.50f).MinSize(400.0f)
                  [SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[SNew(SVerticalBox)

                           + SVerticalBox::Slot().AutoHeight().Padding(
                                 FCkSchedulerDebuggerStyle::Padding_Small,
                                 0.0f)[SNew(SHorizontalBox)

                                       + SHorizontalBox::Slot()
                                             .AutoWidth()
                                             .VAlign(VAlign_Center)
                                             .Padding(FCkSchedulerDebuggerStyle::Padding_Small)
                                                 [SNew(SButton)
                                                      .Text(FText::FromString(TEXT("Relayout")))
                                                      .OnClicked_Lambda(
                                                          [this]() -> FReply
                                                          {
                                                              DoRebuildDetailGraph();
                                                              return FReply::Handled();
                                                          })]

                                       + SHorizontalBox::Slot()
                                             .AutoWidth()
                                             .VAlign(VAlign_Center)
                                             .Padding(FCkSchedulerDebuggerStyle::Padding_Small)
                                                 [SNew(STextBlock)
                                                      .Text(FText::FromString(TEXT("X:")))
                                                      .Font_Static(&ck::scheduler_debugger_axes::
                                                                       Get_Font_Regular_Micro)
                                                      .ColorAndOpacity(CkStyle::TextDim())]

                                       + SHorizontalBox::Slot().AutoWidth().VAlign(
                                             VAlign_Center)[SNew(SBox).WidthOverride(
                                             60.0f)[SNew(SSpinBox<int32>)
                                                        .MinValue(100)
                                                        .MaxValue(600)
                                                        .Value_Lambda(
                                                            [this]()
                                                            {
                                                                return _LayoutParams.SpacingX;
                                                            })
                                                        .OnValueChanged_Lambda(
                                                            [this](int32 InValue)
                                                            {
                                                                _LayoutParams.SpacingX = InValue;
                                                            })]]

                                       + SHorizontalBox::Slot()
                                             .AutoWidth()
                                             .VAlign(VAlign_Center)
                                             .Padding(FCkSchedulerDebuggerStyle::Padding_Small)
                                                 [SNew(STextBlock)
                                                      .Text(FText::FromString(TEXT("Y:")))
                                                      .Font_Static(&ck::scheduler_debugger_axes::
                                                                       Get_Font_Regular_Micro)
                                                      .ColorAndOpacity(CkStyle::TextDim())]

                                       + SHorizontalBox::Slot().AutoWidth().VAlign(
                                             VAlign_Center)[SNew(SBox).WidthOverride(
                                             60.0f)[SNew(SSpinBox<int32>)
                                                        .MinValue(50)
                                                        .MaxValue(250)
                                                        .Value_Lambda(
                                                            [this]()
                                                            {
                                                                return _LayoutParams.SpacingY;
                                                            })
                                                        .OnValueChanged_Lambda(
                                                            [this](int32 InValue)
                                                            {
                                                                _LayoutParams.SpacingY = InValue;
                                                            })]]

                                       + SHorizontalBox::Slot()
                                             .AutoWidth()
                                             .VAlign(VAlign_Center)
                                             .Padding(FCkSchedulerDebuggerStyle::Padding_Small)
                                                 [SNew(STextBlock)
                                                      .Text(FText::FromString(TEXT("Passes:")))
                                                      .Font_Static(&ck::scheduler_debugger_axes::
                                                                       Get_Font_Regular_Micro)
                                                      .ColorAndOpacity(CkStyle::TextDim())]

                                       + SHorizontalBox::Slot().AutoWidth().VAlign(
                                             VAlign_Center)[SNew(SBox).WidthOverride(
                                             50.0f)[SNew(SSpinBox<int32>)
                                                        .MinValue(0)
                                                        .MaxValue(8)
                                                        .Value_Lambda(
                                                            [this]()
                                                            {
                                                                return _LayoutParams
                                                                    .CrossingReductionPasses;
                                                            })
                                                        .OnValueChanged_Lambda(
                                                            [this](int32 InValue)
                                                            {
                                                                _LayoutParams
                                                                    .CrossingReductionPasses =
                                                                    InValue;
                                                            })]]]

                           + SVerticalBox::Slot().FillHeight(
                                  1.0f)[_DetailGraphContainer.ToSharedRef()]]]

           + SSplitter::Slot().Value(0.25f).MinSize(
                  260.0f)[SNew(SCkDebug_PaneHost)[SAssignNew(_Inspector, SCkSchedulerDebugger_Inspector)
                                                     .ViewModel(InViewModel)]];
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::Tick(float InDeltaTime) -> void
{
    (void)InDeltaTime;
    if (NOT _ViewModel.IsValid() || NOT _DetailGraphCanvas.IsValid())
    {
        return;
    }

    const auto& Collector = _ViewModel->Get_DataCollector();
    const auto& Processors = Collector.Get_Processors();
    if (_RuntimeGraphModel.Update_LiveState(Processors))
    {
        _DetailGraphCanvas->Invalidate(EInvalidateWidgetReason::Paint);
    }

}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::OnSelectionChanged(int32 InProcessorIndex) -> void
{
    DoOnSelectionChanged(InProcessorIndex);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::OnStyleRevisionChanged() -> void
{
    if (_ProcessorTree.IsValid())
    {
        _ProcessorTree->Rebuild_ForStyleChange();
    }

    if (_Inspector.IsValid())
    {
        _Inspector->Rebuild_ForStyleChange();
    }

    // Static style choices are baked into the cards; live timing and dirty state remain
    // attribute-bound.
    DoRebuildDetailGraph(true);
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoOnSelectionChanged(int32 InProcessorIndex) -> void
{
    (void)InProcessorIndex;
    DoRebuildDetailGraph();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoRebuildDetailGraph(bool InForceCards)
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _DetailGraphContainer.IsValid())
    {
        return;
    }

    const auto SelectedArrayIndex = _ViewModel->Get_SelectedProcessorIndex();
    const auto& AllProcessors = _ViewModel->Get_DataCollector().Get_Processors();

    if (NOT AllProcessors.IsValidIndex(SelectedArrayIndex))
    {
        _RuntimeGraphModel.Reset();
        _ProcessorCards.Reset();
        _DetailGraphCanvas.Reset();
        DoShowEmptyState();
        return;
    }

    const auto SelectedProcessorId = AllProcessors[SelectedArrayIndex].NodeIndex;
    const auto TopologyChanged =
        _RuntimeGraphModel.Rebuild(AllProcessors, SelectedProcessorId, _LayoutParams);
    if (NOT TopologyChanged && NOT InForceCards && _DetailGraphCanvas.IsValid())
    {
        return;
    }

    _ProcessorCards.Reset();
    auto Scene = FCkDebug_GraphCanvasScene{};
    for (const auto& RuntimeNode : _RuntimeGraphModel.Get_Nodes())
    {
        if (NOT RuntimeNode.IsValid())
        {
            continue;
        }

        auto Card = SNew(SCkSchedulerProcessorCard).Node(RuntimeNode);
        Card->SlatePrepass();
        const auto DesiredSize = Card->GetDesiredSize();
        _ProcessorCards.Add(RuntimeNode->StableId, Card);

        auto CanvasNode = FCkDebug_GraphCanvasNode{};
        CanvasNode.Id = static_cast<uint64>(static_cast<uint32>(RuntimeNode->StableId)) + 1;
        CanvasNode.Position = FVector2D{static_cast<double>(RuntimeNode->Position.X),
                                        static_cast<double>(RuntimeNode->Position.Y)};
        CanvasNode.Size = DesiredSize;
        CanvasNode.Widget = Card;
        Scene.Nodes.Add(MoveTemp(CanvasNode));
    }

    for (const auto& RuntimeEdge : _RuntimeGraphModel.Get_Edges())
    {
        auto CanvasEdge = FCkDebug_GraphCanvasEdge{};
        CanvasEdge.SourceId = static_cast<uint64>(static_cast<uint32>(RuntimeEdge.SourceId)) + 1;
        CanvasEdge.TargetId = static_cast<uint64>(static_cast<uint32>(RuntimeEdge.TargetId)) + 1;
        CanvasEdge.Color = CkStyle::Graph_Edge();
        CanvasEdge.Thickness = 1.5f;
        CanvasEdge.LineSeparation = 4.5f;
        CanvasEdge.DeemphasizeWhenUnrelatedHovered = false;
        Scene.Edges.Add(MoveTemp(CanvasEdge));
    }

    if (NOT _DetailGraphCanvas.IsValid())
    {
        _DetailGraphCanvas =
            SNew(SCkDebug_GraphCanvas)
                .OnSelectionChanged_Raw(
                    this, &FCkSchedulerDebuggerPage_TreeView::DoOnCanvasSelectionChanged)
                .OnNodeContextMenu_Raw(
                    this, &FCkSchedulerDebuggerPage_TreeView::DoOnCanvasNodeContextMenu);
        _DetailGraphContainer->SetContent(_DetailGraphCanvas.ToSharedRef());
    }

    _DetailGraphCanvas->Set_Scene(MoveTemp(Scene));
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoBuildDetailGraph() -> TSharedRef<SWidget>
{
    DoShowEmptyState();
    return _DetailGraphContainer.ToSharedRef();
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoOnCanvasSelectionChanged(
    const TSet<uint64>& InSelectedNodeIds) -> void
{
    for (const auto& Card : _ProcessorCards)
    {
        const auto CanvasId = static_cast<uint64>(static_cast<uint32>(Card.Key)) + 1;
        if (Card.Value.IsValid())
        {
            Card.Value->Set_Selected(InSelectedNodeIds.Contains(CanvasId));
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoOnCanvasNodeContextMenu(uint64 InNodeId,
                                                                  const FPointerEvent& InMouseEvent)
    -> void
{
    if (InNodeId == 0)
    {
        return;
    }

    const auto ProcessorId = static_cast<int32>(static_cast<uint32>(InNodeId - 1));
    const auto Node = _RuntimeGraphModel.Get_NodeById(ProcessorId);
    if (NOT Node.IsValid())
    {
        return;
    }

    const auto& Info = Node->Processor;
    const auto DisplayName = Info.DisplayName;
    const auto ClassName = Info.ProcessorName.ToString();
    const auto GroupName = Info.GroupName.ToString();
    const auto ExecOrder = ck::Format_UE(TEXT("#{}"), Info.ExecutionOrder);
    const auto Timing = ck::Format_UE(TEXT("{:.3f} ms"), Info.MainPassTimeMs);

    auto Flags = TArray<FString>{};
    if (Info.IsGhost)
    {
        Flags.Add(TEXT("Ghost"));
    }
    if (Info.IsParallel)
    {
        Flags.Add(TEXT("Parallel"));
    }
    if (Info.HasDirtyMarker)
    {
        Flags.Add(TEXT("Dirty"));
    }
    if (Info.IsGroupStart)
    {
        Flags.Add(TEXT("GroupStart"));
    }
    if (Info.IsGroupEnd)
    {
        Flags.Add(TEXT("GroupEnd"));
    }
    const auto FlagsText = FString::Join(Flags, TEXT(" "));
    auto Summary =
        ck::Format_UE(TEXT("{}\nClass: {}\nGroup: {}\nExec Order: {}\nTiming: {}\nPump Count: {}"),
                      DisplayName,
                      ClassName,
                      GroupName,
                      ExecOrder,
                      Timing,
                      Info.PumpCountThisFrame);
    if (NOT FlagsText.IsEmpty())
    {
        Summary += ck::Format_UE(TEXT("\nFlags: {}"), FlagsText);
    }

    auto Menu = FMenuBuilder(true, nullptr);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy Display Name")),
                                    FText::FromString(TEXT("Copy the visible node title")),
                                    DisplayName);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy Processor Class Name")),
                                    FText::FromString(
                                        TEXT("Copy the underlying processor class name")),
                                    ClassName);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy Group Name")),
                                    FText::FromString(
                                        TEXT("Copy the group this processor belongs to")),
                                    GroupName);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy Exec Order")),
                                    FText::FromString(
                                        TEXT("Copy this processor's execution order")),
                                    ExecOrder);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy Timing")),
                                    FText::FromString(
                                        TEXT("Copy last frame's timing for this processor")),
                                    Timing);
    ck::DebugCopyMenu::AddCopyEntry(Menu,
                                    FText::FromString(TEXT("Copy All")),
                                    FText::FromString(
                                        TEXT("Copy a multi-line summary of this node")),
                                    Summary);
    FSlateApplication::Get().PushMenu(_DetailGraphCanvas.ToSharedRef(),
                                      FWidgetPath{},
                                      Menu.MakeWidget(),
                                      InMouseEvent.GetScreenSpacePosition(),
                                      FPopupTransitionEffect{FPopupTransitionEffect::ContextMenu});
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkSchedulerDebuggerPage_TreeView::DoShowEmptyState() -> void
{
    if (NOT _DetailGraphContainer.IsValid())
    {
        return;
    }

    _DetailGraphContainer->SetContent(
        SNew(SBox)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
                [SNew(STextBlock)
                     .Text(FText::FromString(TEXT("Select a processor to view its dependencies")))
                     .Font_Static(&ck::scheduler_debugger_axes::Get_Font_Italic_EmptyState)
                     .ColorAndOpacity(CkStyle::TextMute())]);
}

// --------------------------------------------------------------------------------------------------------------------
