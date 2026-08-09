#include "CkIntentDebugger/Window/SCkIntentDebugger_LayerStackPanel.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_layerstack
{
    constexpr auto IndentPixels = 14.0f;
    constexpr auto PadS = 4.0f;

    auto
        Build_Nodes(
            const FCkIntentDebugger_SourceSnapshot& InSource)
        -> TArray<FCkIntentDebugger_StackNode>
    {
        auto Nodes = TArray<FCkIntentDebugger_StackNode>{};

        for (const auto& Layer : InSource.Layers)
        {
            auto LayerNode = FCkIntentDebugger_StackNode{};
            LayerNode.Kind = ECkIntentDebugger_StackNodeKind::Layer;
            LayerNode.LayerPriority = Layer.Priority;
            LayerNode.Label = Layer.Get_StackLabel();
            LayerNode.Detail = ck::Format_UE(TEXT("{} capture(s)"), Layer.Captures.Num());
            LayerNode.Tint = Layer.HasMatcher ? CkStyle::Accent() : CkStyle::Text();

            Nodes.Add(MoveTemp(LayerNode));

            if (Layer.HasMatcher)
            {
                auto MatcherNode = FCkIntentDebugger_StackNode{};
                MatcherNode.Kind = ECkIntentDebugger_StackNodeKind::MatcherSummary;
                MatcherNode.LayerPriority = Layer.Priority;
                MatcherNode.Indent = 1;
                MatcherNode.Label = TEXT("matcher");
                MatcherNode.Detail = Layer.HasActiveSet
                    ? ck::Format_UE(TEXT("{} intent(s) · chord {}f · latch decay {}f · {}"),
                        Layer.ActiveIntentCount,
                        Layer.ChordWindowFrames,
                        Layer.LatchDecayFrames,
                        ck::intent_debugger::Get_Label(Layer.MatcherCaptureBehavior))
                    : TEXT("no active set — captures nothing, answers Idle for everything");
                MatcherNode.Tint = Layer.HasActiveSet ? CkStyle::Ok() : CkStyle::TextMute();

                Nodes.Add(MoveTemp(MatcherNode));

                for (auto Index = 0; Index < Layer.RegisteredCaptureKeys.Num(); ++Index)
                {
                    auto KeyNode = FCkIntentDebugger_StackNode{};
                    KeyNode.Kind = ECkIntentDebugger_StackNodeKind::RegisteredKey;
                    KeyNode.LayerPriority = Layer.Priority;
                    KeyNode.ChildIndex = Index;
                    KeyNode.Indent = 2;
                    KeyNode.Label = TEXT("terminal key");
                    KeyNode.Detail = Layer.RegisteredCaptureKeys[Index].ToString();
                    KeyNode.Tint = CkStyle::TextDim();

                    Nodes.Add(MoveTemp(KeyNode));
                }
            }

            for (auto Index = 0; Index < Layer.Captures.Num(); ++Index)
            {
                const auto& Capture = Layer.Captures[Index];

                auto CaptureNode = FCkIntentDebugger_StackNode{};
                CaptureNode.Kind = ECkIntentDebugger_StackNodeKind::Capture;
                CaptureNode.LayerPriority = Layer.Priority;
                CaptureNode.ChildIndex = Index;
                CaptureNode.Indent = 1;
                CaptureNode.Label = Capture.MatchMode == ECk_InputLayer_CaptureMatch::CatchAll
                    ? FString{TEXT("catch-all")}
                    : Capture.Key.ToString();
                CaptureNode.Detail = ck::intent_debugger::Get_Label(Capture.Behavior);
                CaptureNode.Tint = Capture.Behavior == ECk_InputLayer_CaptureBehavior::Consume
                    ? CkStyle::Warn()
                    : CkStyle::TextDim();

                Nodes.Add(MoveTemp(CaptureNode));
            }
        }

        return Nodes;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_StackNode::
    Get_Key() const
    -> FString
{
    return ck::Format_UE(TEXT("{}|{}|{}"), static_cast<int32>(Kind), LayerPriority, ChildIndex);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_LayerStackPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SAssignNew(_ListView, SListView<TSharedPtr<FCkIntentDebugger_StackNode>>)
            .ListItemsSource(&_Nodes)
            .SelectionMode(ESelectionMode::Single)
            .OnGenerateRow(this, &SCkIntentDebugger_LayerStackPanel::OnGenerateRow)
            .OnSelectionChanged(this, &SCkIntentDebugger_LayerStackPanel::OnSelectionChanged)
            .OnContextMenuOpening_Lambda([this]() -> TSharedPtr<SWidget>
            {
                auto Lines = TArray<FString>{};
                for (const auto& Selected : _ListView->GetSelectedItems())
                {
                    if (NOT Selected.IsValid())
                    { continue; }

                    Lines.Add(ck::Format_UE(TEXT("{}\t{}"), Selected->Label, Selected->Detail));
                }

                if (Lines.IsEmpty())
                { return nullptr; }

                auto MenuBuilder = FMenuBuilder{true, nullptr};
                ck::DebugCopyMenu::AddCopyEntry(
                    MenuBuilder,
                    FText::FromString(TEXT("Copy Row(s)")),
                    FText::FromString(TEXT("Copy the selected stack rows")),
                    FString::Join(Lines, TEXT("\n")));

                return MenuBuilder.MakeWidget();
            })
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_LayerStackPanel::
    Reset_ForWorldChange()
    -> void
{
    _Nodes.Reset();

    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_LayerStackPanel::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _ListView.IsValid())
    { return; }

    const auto* Source = _ViewModel->TryGet_SelectedSource();

    const auto Rebuilt = Source != nullptr
        ? ck_intent_debugger_layerstack::Build_Nodes(*Source)
        : TArray<FCkIntentDebugger_StackNode>{};

    // Selection is tracked by pointer identity, so a row that still describes the same thing must keep the same
    // TSharedPtr across refreshes or the user's click evaporates on the next tick.
    auto Existing = TMap<FString, TSharedPtr<FCkIntentDebugger_StackNode>>{};
    for (const auto& Node : _Nodes)
    {
        if (Node.IsValid())
        { Existing.Add(Node->Get_Key(), Node); }
    }

    auto NewNodes = TArray<TSharedPtr<FCkIntentDebugger_StackNode>>{};
    NewNodes.Reserve(Rebuilt.Num());

    auto SetChanged = false;

    for (const auto& Row : Rebuilt)
    {
        const auto Key = Row.Get_Key();

        if (auto* Found = Existing.Find(Key))
        {
            **Found = Row;
            NewNodes.Add(*Found);
            Existing.Remove(Key);
            continue;
        }

        NewNodes.Add(MakeShared<FCkIntentDebugger_StackNode>(Row));
        SetChanged = true;
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _Nodes = MoveTemp(NewNodes);

    if (SetChanged)
    { _ListView->RequestListRefresh(); }

    const auto SelectedPriority = _ViewModel->Get_SelectedLayerPriority();

    auto Wanted = TSharedPtr<FCkIntentDebugger_StackNode>{};
    for (const auto& Node : _Nodes)
    {
        if (Node.IsValid()
            && Node->Kind == ECkIntentDebugger_StackNodeKind::Layer
            && Node->LayerPriority == SelectedPriority)
        {
            Wanted = Node;
            break;
        }
    }

    const auto Current = _ListView->GetSelectedItems();
    const auto AlreadySelected = Current.Num() == 1 && Current[0] == Wanted;

    if (Wanted.IsValid() && NOT AlreadySelected)
    { _ListView->SetItemSelection(Wanted, true, ESelectInfo::Direct); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_LayerStackPanel::
    OnGenerateRow(
        TSharedPtr<FCkIntentDebugger_StackNode> InNode,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    const auto WeakNode = TWeakPtr<FCkIntentDebugger_StackNode>(InNode);

    // Row content is plain visual widgets only — anything that returns Handled on left-mouse-down traps the click
    // before STableRow sees it and the row silently stops being selectable.
    return SNew(STableRow<TSharedPtr<FCkIntentDebugger_StackNode>>, InOwnerTable)
        .Padding(FMargin{0.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox)
                    .WidthOverride_Lambda([WeakNode]()
                    {
                        const auto Node = WeakNode.Pin();
                        return Node.IsValid()
                            ? static_cast<float>(Node->Indent) * ck_intent_debugger_layerstack::IndentPixels
                            : 0.0f;
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.45f).Padding(ck_intent_debugger_layerstack::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .Text_Lambda([WeakNode]()
                    {
                        const auto Node = WeakNode.Pin();
                        return Node.IsValid() ? FText::FromString(Node->Label) : FText::GetEmpty();
                    })
                    .ColorAndOpacity_Lambda([WeakNode]()
                    {
                        const auto Node = WeakNode.Pin();
                        return FSlateColor{Node.IsValid() ? Node->Tint : CkStyle::Text()};
                    })
            ]

            + SHorizontalBox::Slot().FillWidth(0.55f).Padding(ck_intent_debugger_layerstack::PadS, 0.0f)
            [
                SNew(STextBlock)
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(CkStyle::TextDim())
                    .Text_Lambda([WeakNode]()
                    {
                        const auto Node = WeakNode.Pin();
                        return Node.IsValid() ? FText::FromString(Node->Detail) : FText::GetEmpty();
                    })
            ]
        ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_LayerStackPanel::
    OnSelectionChanged(
        TSharedPtr<FCkIntentDebugger_StackNode> InNode,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // A programmatic restore arrives as Direct and must not echo back into the ViewModel.
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    if (NOT InNode.IsValid() || NOT _ViewModel.IsValid())
    { return; }

    _ViewModel->Set_SelectedLayerPriority(InNode->LayerPriority);
}

// --------------------------------------------------------------------------------------------------------------------
