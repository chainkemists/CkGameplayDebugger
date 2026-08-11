#include "CkGoapDebugger/Graph/CkGoapRuntimeGraphModel.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

namespace ck_goap_runtime_graph
{
    constexpr float NodeWidth = 180.0f, NodeHeight = 110.0f, HorzGap = 70.0f, VertGap = 14.0f,
                    NodeMinX = 40.0f, NodeMinY = 40.0f, GoalGap = 90.0f;
    constexpr float NodeMaxWidth = 420.0f;

    auto TagLeaf(const FGameplayTag& InTag) -> FString
    {
        const FString Full = InTag.ToString();
        int32 Dot = INDEX_NONE;
        return Full.FindLastChar(TEXT('.'), Dot) ? Full.Mid(Dot + 1) : Full;
    }

    auto Flatten(const FCkGoapDebugger_PlannerInfo& Planner,
                 TArray<const FCkGoapDebugger_ActionInfo*>& Out,
                 TSet<FCk_Handle_Goap_Action>& Seen) -> void
    {
        for (const auto& Action : Planner.ChildActions)
        {
            if (NOT ck::IsValid(Action.Handle) || Seen.Contains(Action.Handle))
            {
                continue;
            }
            Seen.Add(Action.Handle);
            Out.Add(&Action);
        }
        for (const auto& Child : Planner.ChildPlanners)
        {
            Flatten(Child, Out, Seen);
        }
    }

    auto Find(const TArray<const FCkGoapDebugger_ActionInfo*>& Catalog,
              const FCk_Handle_Goap_Action& Handle) -> const FCkGoapDebugger_ActionInfo*
    {
        for (const auto* Action : Catalog)
        {
            if (Action != nullptr && Action->Handle == Handle)
            {
                return Action;
            }
        }
        return nullptr;
    }

    auto MeasureNode(const FCkGoapDebugger_ActionInfo& Action, int32 NameDepth) -> FVector2D
    {
        if (NOT FSlateApplication::IsInitialized())
        {
            return FVector2D{NodeWidth, NodeHeight};
        }
        const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const auto HeaderFont = ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH3());
        const auto CostFont = ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody());
        const auto CompositeFont = ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro());
        const auto RowFont = ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro());
        const auto MeasureWidth = [&FontMeasure](const FString& Text, const FSlateFontInfo& Font)
        {
            return static_cast<float>(FontMeasure->Measure(Text, Font).X);
        };
        const auto MeasureHeight = [&FontMeasure](const FString& Text, const FSlateFontInfo& Font)
        {
            return static_cast<float>(FontMeasure->Measure(Text, Font).Y);
        };

        constexpr float DotWidth = 7.0f + 4.0f;
        constexpr float PreRowPrefix = 32.0f;
        constexpr float ColumnGap = 12.0f;
        constexpr float RowVertPad = 2.0f;
        const float CardChrome =
            2.0f * ck::debug_axes::Get_NodeBorderThickness() +
            2.0f * CkStyle::SpaceM * ck_goap_debugger_axes::Get_NodePaddingScale() + 6.0f;

        const FString HeaderName = SCkDebug_NameLabel::Get_ShortName(Action.ClassName, NameDepth);
        const FString CostText = FString::Printf(TEXT("$%.0f"), Action.Cost);
        const float HeaderWidth = MeasureWidth(HeaderName, HeaderFont) + 4.0f +
                                  MeasureWidth(CostText, CostFont);
        const float HeaderHeight = MeasureHeight(HeaderName, HeaderFont);

        float CompositeWidth = 0.0f;
        float CompositeHeight = 0.0f;
        if (Action.ChildActionHandles.Num() > 0)
        {
            const FString CompositeText =
                FString::Printf(TEXT("\x203A %s"), *TagLeaf(Action.ActionTag));
            CompositeWidth = MeasureWidth(CompositeText, CompositeFont) + 2.0f * 4.0f;
            CompositeHeight = MeasureHeight(CompositeText, CompositeFont) + 2.0f * 1.0f + 2.0f;
        }

        float PreWidth = 0.0f;
        float EffectWidth = 0.0f;
        float RowHeight = 0.0f;
        for (const auto& Pre : Action.Preconditions)
        {
            const FString Leaf = TagLeaf(Pre.Key);
            PreWidth = FMath::Max(PreWidth, PreRowPrefix + MeasureWidth(Leaf, RowFont));
            RowHeight = FMath::Max(RowHeight, MeasureHeight(Leaf, RowFont) + RowVertPad);
        }
        for (const auto& Effect : Action.Effects)
        {
            const FString Leaf = TagLeaf(Effect.Key);
            EffectWidth = FMath::Max(EffectWidth, MeasureWidth(Leaf, RowFont) + DotWidth);
            RowHeight = FMath::Max(RowHeight, MeasureHeight(Leaf, RowFont) + RowVertPad);
        }

        const float BodyWidth = PreWidth +
                                (PreWidth > 0.0f && EffectWidth > 0.0f ? ColumnGap : 0.0f) +
                                EffectWidth;
        const float Width = FMath::Clamp(
            FMath::Max3(HeaderWidth, CompositeWidth, BodyWidth) + CardChrome, NodeWidth, NodeMaxWidth);
        const float BodyRows =
            static_cast<float>(FMath::Max(Action.Preconditions.Num(), Action.Effects.Num()));
        const float Height = 4.0f + 2.0f * 4.0f + HeaderHeight + CompositeHeight +
                             (BodyRows > 0.0f ? 4.0f + BodyRows * RowHeight : 0.0f);
        return FVector2D{Width, FMath::Max(Height, 60.0f)};
    }

    auto MeasureGoalNode(const TArray<FCkGoapDebugger_Condition>& Conditions) -> FVector2D
    {
        if (NOT FSlateApplication::IsInitialized())
        {
            return FVector2D{200.0f, NodeHeight};
        }
        const auto FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        const auto HeaderFont = ck::debug_axes::ScaledFont("Bold", CkStyle::NodeTitleFontSize());
        const auto RowFont = ck::debug_axes::ScaledFont("Regular", CkStyle::NodeMetaFontSize());
        const auto MeasureWidth = [&FontMeasure](const FString& Text, const FSlateFontInfo& Font)
        {
            return static_cast<float>(FontMeasure->Measure(Text, Font).X);
        };
        const auto MeasureHeight = [&FontMeasure](const FString& Text, const FSlateFontInfo& Font)
        {
            return static_cast<float>(FontMeasure->Measure(Text, Font).Y);
        };

        const float Chrome = 2.0f * ck::debug_axes::Get_NodeBorderThickness() +
                             2.0f * CkStyle::SpaceM * ck_goap_debugger_axes::Get_NodePaddingScale();
        float ContentWidth = MeasureWidth(TEXT("◆ Goal"), HeaderFont);
        float RowsHeight = 0.0f;
        for (const auto& Condition : Conditions)
        {
            const FString Text = FString::Printf(TEXT("%s = %s"),
                                                  *TagLeaf(Condition.Key),
                                                  Condition.Value ? TEXT("true") : TEXT("false"));
            ContentWidth = FMath::Max(ContentWidth, MeasureWidth(Text, RowFont));
            RowsHeight += MeasureHeight(Text, RowFont) + 2.0f;
        }
        const float Height = 2.0f * ck::debug_axes::Get_NodeBorderThickness() +
                             2.0f * CkStyle::SpaceS * ck_goap_debugger_axes::Get_NodePaddingScale() +
                             MeasureHeight(TEXT("◆ Goal"), HeaderFont) +
                             (Conditions.IsEmpty() ? 0.0f : CkStyle::SpaceS + RowsHeight);
        return FVector2D{FMath::Clamp(ContentWidth + Chrome, NodeWidth, NodeMaxWidth),
                         FMath::Max(Height, 60.0f)};
    }

} // namespace ck_goap_runtime_graph

auto FCkGoapRuntimeGraphModel::ComputeTopologyHash(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> uint32
{
    using namespace ck_goap_runtime_graph;
    uint32 Hash = GetTypeHash(static_cast<FCk_Handle>(InPlanner.PlannerHandle));
    TArray<const FCkGoapDebugger_ActionInfo*> Catalog;
    TSet<FCk_Handle_Goap_Action> Seen;
    Flatten(InPlanner, Catalog, Seen);
    for (const auto* A : Catalog)
    {
        Hash = HashCombine(Hash, GetTypeHash(static_cast<FCk_Handle>(A->Handle)));
        Hash = HashCombine(Hash, GetTypeHash(A->ClassName));
        for (const auto& C : A->Preconditions)
        {
            Hash = HashCombine(Hash, HashCombine(GetTypeHash(C.Key), GetTypeHash(C.Value)));
        }
        for (const auto& C : A->Effects)
        {
            Hash = HashCombine(Hash, HashCombine(GetTypeHash(C.Key), GetTypeHash(C.Value)));
        }
        Hash = HashCombine(Hash, GetTypeHash(static_cast<FCk_Handle>(A->ParentActionHandle)));
        for (const auto& Child : A->ChildActionHandles)
        {
            Hash = HashCombine(Hash, GetTypeHash(static_cast<FCk_Handle>(Child)));
        }
    }
    for (const auto& Goal : InPlanner.GoalResolved)
    {
        Hash = HashCombine(Hash, HashCombine(GetTypeHash(Goal.Key), GetTypeHash(Goal.Value)));
    }
    return Hash;
}

auto FCkGoapRuntimeGraphModel::ComputeEffectiveGoalHash(
    const FCkGoapDebugger_PlannerInfo& InPlanner,
    const FCkGoapDebugger_ActionInfo* InSelectedAction) -> uint32
{
    const auto* Goal = &InPlanner.GoalResolved;
    auto Owner = InPlanner.DisplayName;
    if (InSelectedAction != nullptr && InSelectedAction->Goal.Num() > 0)
    {
        Goal = &InSelectedAction->Goal;
        Owner = InSelectedAction->ClassName;
    }

    auto Hash = GetTypeHash(Owner);
    for (const auto& Condition : *Goal)
    {
        Hash = HashCombine(Hash, GetTypeHash(Condition.Key));
        Hash = HashCombine(Hash, GetTypeHash(Condition.Value));
    }
    return Hash;
}

auto FCkGoapRuntimeGraphModel::ComputeMaxNameDepth(const FString& InClassName) -> int32
{
    auto Name = InClassName;
    if (Name.StartsWith(TEXT("Default__")))
    {
        Name = Name.RightChop(9);
    }
    if (Name.EndsWith(TEXT("_C")))
    {
        Name = Name.LeftChop(2);
    }

    auto Segments = TArray<FString>{};
    Name.ParseIntoArray(Segments, TEXT("_"), true);
    return FMath::Max(1, Segments.Num());
}

auto FCkGoapRuntimeGraphModel::Rebuild(const FCkGoapDebugger_PlannerInfo& InPlanner,
                                       const FCk_Handle_Goap_Action& InSelectedAction,
                                       int32 InNameDepth) -> void
{
    using namespace ck_goap_runtime_graph;
    Reset();
    _TopologyHash = ComputeTopologyHash(InPlanner);
    TArray<const FCkGoapDebugger_ActionInfo*> Catalog;
    TSet<FCk_Handle_Goap_Action> Seen;
    Flatten(InPlanner, Catalog, Seen);
    _ActionCount = Catalog.Num();
    TMap<FGameplayTag, TArray<int32>> Producers;
    for (int32 I = 0; I < Catalog.Num(); ++I)
    {
        for (const auto& Effect : Catalog[I]->Effects)
        {
            Producers.FindOrAdd(Effect.Key).Add(I);
        }
    }
    TArray<int32> Layers;
    Layers.Init(-1, Catalog.Num());
    bool HasCycle = false;
    auto Resolve = [&](auto&& Self, int32 Index) -> int32
    {
        if (Layers[Index] >= 0)
            return Layers[Index];
        if (Layers[Index] == -2)
        {
            HasCycle = true;
            return 0;
        }
        Layers[Index] = -2;
        int32 Layer = 0;
        for (const auto& Pre : Catalog[Index]->Preconditions)
            if (const auto* Ps = Producers.Find(Pre.Key))
                for (int32 P : *Ps)
                    if (P != Index)
                        Layer = FMath::Max(Layer, Self(Self, P) + 1);
        return Layers[Index] = Layer;
    };
    for (int32 I = 0; I < Catalog.Num(); ++I)
    {
        Resolve(Resolve, I);
    }
    if (HasCycle)
    {
        for (int32 I = 0; I < Layers.Num(); ++I)
        {
            Layers[I] = I;
        }
    }
    int32 MaxLayer = -1;
    for (int32 Layer : Layers)
    {
        MaxLayer = FMath::Max(MaxLayer, Layer);
    }
    TArray<FVector2D> NodeSizes;
    NodeSizes.Reserve(Catalog.Num());
    TArray<float> LayerWidths;
    LayerWidths.Init(NodeWidth, MaxLayer + 1);
    TArray<float> LayerHeights;
    LayerHeights.Init(0.0f, MaxLayer + 1);
    for (int32 I = 0; I < Catalog.Num(); ++I)
    {
        const FVector2D Size = MeasureNode(*Catalog[I], InNameDepth);
        NodeSizes.Add(Size);
        LayerWidths[Layers[I]] = FMath::Max(LayerWidths[Layers[I]], static_cast<float>(Size.X));
        LayerHeights[Layers[I]] += static_cast<float>(Size.Y) + VertGap;
    }
    for (float& Height : LayerHeights)
    {
        Height = FMath::Max(Height - VertGap, 0.0f);
    }
    TArray<float> LayerX;
    LayerX.Init(NodeMinX, MaxLayer + 2);
    for (int32 Layer = 1; Layer <= MaxLayer + 1; ++Layer)
    {
        LayerX[Layer] = LayerX[Layer - 1] + LayerWidths[FMath::Min(Layer - 1, MaxLayer)] + HorzGap;
    }
    TArray<float> LayerY;
    LayerY.SetNumZeroed(MaxLayer + 1);
    for (int32 I = 0; I < Catalog.Num(); ++I)
    {
        // Catalog order is parent-first and participates in the topology hash.
        // This deterministic ordinal is collision-free within the canvas scene;
        // the handle map remains the authoritative selection identity.
        const uint64 Id = static_cast<uint64>(I) + 1ull;
        auto Node = MakeShared<FCkGoapRuntimeGraphNode>();
        Node->Id = Id;
        Node->Action = *Catalog[I];
        Node->Size = NodeSizes[I];
        Node->Position = FVector2D{LayerX[Layers[I]],
                                   NodeMinY + LayerY[Layers[I]] - LayerHeights[Layers[I]] / 2.0f};
        LayerY[Layers[I]] += static_cast<float>(NodeSizes[I].Y) + VertGap;
        _Nodes.Add(Node);
        _ActionById.Add(Id, Node->Action.Handle);
        _MaxNameDepth = FMath::Max(_MaxNameDepth, ComputeMaxNameDepth(Node->Action.ClassName));
    }
    auto FindCatalogIndex = [&Catalog](const FCk_Handle_Goap_Action& InHandle) -> int32
    {
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
        {
            if (Catalog[Index] != nullptr && Catalog[Index]->Handle == InHandle)
            {
                return Index;
            }
        }
        return INDEX_NONE;
    };
    for (int32 Consumer = 0; Consumer < Catalog.Num(); ++Consumer)
        for (const auto& Pre : Catalog[Consumer]->Preconditions)
            if (const auto* Ps = Producers.Find(Pre.Key))
                for (int32 Producer : *Ps)
                    if (Producer != Consumer)
                        _Edges.Add({static_cast<uint64>(Producer) + 1ull,
                                    static_cast<uint64>(Consumer) + 1ull,
                                    ECkGoapRuntimeGraphEdgeKind::Dependency});
    for (int32 ParentIndex = 0; ParentIndex < Catalog.Num(); ++ParentIndex)
        for (const auto& Child : Catalog[ParentIndex]->ChildActionHandles)
            if (const int32 ChildIndex = FindCatalogIndex(Child); ChildIndex != INDEX_NONE)
                _Edges.Add({static_cast<uint64>(ParentIndex) + 1ull,
                            static_cast<uint64>(ChildIndex) + 1ull,
                            ECkGoapRuntimeGraphEdgeKind::Tree});
    const TArray<FCkGoapDebugger_Condition>* Goal = nullptr;
    FString Owner;
    if (const auto* Selected = Find(Catalog, InSelectedAction);
        Selected != nullptr && Selected->Goal.Num() > 0)
    {
        Goal = &Selected->Goal;
        Owner = Selected->ClassName;
    }
    if (Goal == nullptr && InPlanner.GoalResolved.Num() > 0)
    {
        Goal = &InPlanner.GoalResolved;
        Owner = InPlanner.DisplayName;
    }
    if (Goal != nullptr)
    {
        auto Node = MakeShared<FCkGoapRuntimeGraphNode>();
        Node->Id = MAX_uint64;
        Node->Kind = ECkGoapRuntimeGraphNodeKind::Goal;
        Node->GoalOwnerName = Owner;
        Node->GoalConditions = *Goal;
        Node->Position = FVector2D{LayerX[MaxLayer + 1] + GoalGap, NodeMinY};
        Node->Size = MeasureGoalNode(Node->GoalConditions);
        _Nodes.Add(Node);
        for (int32 I = 0; I < Catalog.Num(); ++I)
            for (const auto& E : Catalog[I]->Effects)
                for (const auto& G : *Goal)
                    if (E.Key == G.Key)
                        _Edges.Add({static_cast<uint64>(I) + 1ull,
                                    Node->Id,
                                    ECkGoapRuntimeGraphEdgeKind::Dependency});
    }
    UpdateRuntimeState(InPlanner, InSelectedAction);
}

auto FCkGoapRuntimeGraphModel::UpdateRuntimeState(const FCkGoapDebugger_PlannerInfo& InPlanner,
                                                   const FCk_Handle_Goap_Action& InSelectedAction)
    -> bool
{
    using namespace ck_goap_runtime_graph;
    TArray<const FCkGoapDebugger_ActionInfo*> Catalog;
    TSet<FCk_Handle_Goap_Action> Seen;
    Flatten(InPlanner, Catalog, Seen);
    const auto* Selected = Find(Catalog, InSelectedAction);
    const auto& PlanNames = Selected != nullptr && Selected->PlanClassNames.Num() > 0
                                ? Selected->PlanClassNames
                                : InPlanner.PlanClassNames;
    TMap<FString, int32> Steps;
    int32 Step = 1;
    for (const auto& Name : PlanNames)
    {
        Steps.Add(Name, Step++);
    }
    auto SceneStateChanged = false;
    for (const auto& Node : _Nodes)
    {
        Node->WorldState.Reset();
        for (const auto& Entry : InPlanner.WorldState)
        {
            Node->WorldState.Add(Entry.Key, Entry.Value);
        }
        if (Node->Kind != ECkGoapRuntimeGraphNodeKind::Action)
            continue;
        if (const auto* Current = Find(Catalog, Node->Action.Handle))
        {
            Node->Action = *Current;
        }
        const int32 NewPlanStep = Steps.FindRef(Node->Action.ClassName);
        const bool NewInPlan = NewPlanStep > 0;
        Node->IsSelected = ck::IsValid(InSelectedAction) && Node->Action.Handle == InSelectedAction;
        const bool NewFailureBlocked = Node->Action.PlanStatus == ECk_GoapPlanStatus::PlanFailed &&
                                       (Node->Action.Role == ECkGoapDebugger_ActionRole::Leaf ||
                                        Node->Action.Role == ECkGoapDebugger_ActionRole::Mid);
        SceneStateChanged |= Node->IsInPlan != NewInPlan ||
                             Node->IsFailureBlocked != NewFailureBlocked;
        Node->PlanStepIndex = NewPlanStep;
        Node->IsInPlan = NewInPlan;
        Node->IsFailureBlocked = NewFailureBlocked;
    }
    return SceneStateChanged;
}

auto FCkGoapRuntimeGraphModel::Relayout(int32 InNameDepth) -> void
{
    using namespace ck_goap_runtime_graph;
    auto Actions = TArray<TSharedPtr<FCkGoapRuntimeGraphNode>>{};
    TSharedPtr<FCkGoapRuntimeGraphNode> Goal;
    for (const auto& Node : _Nodes)
    {
        if (Node->Kind == ECkGoapRuntimeGraphNodeKind::Action)
        {
            Actions.Add(Node);
        }
        else
        {
            Goal = Node;
        }
    }

    auto Producers = TMap<FGameplayTag, TArray<int32>>{};
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        for (const auto& Effect : Actions[Index]->Action.Effects)
        {
            Producers.FindOrAdd(Effect.Key).Add(Index);
        }
    }
    auto Layers = TArray<int32>{};
    Layers.Init(-1, Actions.Num());
    bool HasCycle = false;
    auto Resolve = [&](auto&& Self, int32 Index) -> int32
    {
        if (Layers[Index] >= 0) return Layers[Index];
        if (Layers[Index] == -2)
        {
            HasCycle = true;
            return 0;
        }
        Layers[Index] = -2;
        int32 Layer = 0;
        for (const auto& Precondition : Actions[Index]->Action.Preconditions)
        {
            if (const auto* Found = Producers.Find(Precondition.Key))
            {
                for (const int32 Producer : *Found)
                {
                    if (Producer != Index)
                    {
                        Layer = FMath::Max(Layer, Self(Self, Producer) + 1);
                    }
                }
            }
        }
        return Layers[Index] = Layer;
    };
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        Resolve(Resolve, Index);
    }
    if (HasCycle)
    {
        for (int32 Index = 0; Index < Layers.Num(); ++Index)
        {
            Layers[Index] = Index;
        }
    }
    int32 MaxLayer = -1;
    for (const int32 Layer : Layers)
    {
        MaxLayer = FMath::Max(MaxLayer, Layer);
    }
    auto LayerWidths = TArray<float>{};
    LayerWidths.Init(NodeWidth, MaxLayer + 1);
    auto LayerHeights = TArray<float>{};
    LayerHeights.Init(0.0f, MaxLayer + 1);
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        Actions[Index]->Size = MeasureNode(Actions[Index]->Action, InNameDepth);
        const int32 Layer = Layers[Index];
        LayerWidths[Layer] = FMath::Max(LayerWidths[Layer], static_cast<float>(Actions[Index]->Size.X));
        LayerHeights[Layer] += static_cast<float>(Actions[Index]->Size.Y) + VertGap;
    }
    for (float& Height : LayerHeights)
    {
        Height = FMath::Max(Height - VertGap, 0.0f);
    }
    auto LayerX = TArray<float>{};
    LayerX.Init(NodeMinX, MaxLayer + 2);
    for (int32 Layer = 1; Layer <= MaxLayer + 1; ++Layer)
    {
        LayerX[Layer] = LayerX[Layer - 1] +
                        LayerWidths[FMath::Min(Layer - 1, MaxLayer)] + HorzGap;
    }
    auto LayerY = TArray<float>{};
    LayerY.SetNumZeroed(MaxLayer + 1);
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        const int32 Layer = Layers[Index];
        Actions[Index]->Position = FVector2D{LayerX[Layer],
                                             NodeMinY + LayerY[Layer] - LayerHeights[Layer] / 2.0f};
        LayerY[Layer] += static_cast<float>(Actions[Index]->Size.Y) + VertGap;
    }
    if (Goal.IsValid())
    {
        Goal->Size = MeasureGoalNode(Goal->GoalConditions);
        Goal->Position = FVector2D{LayerX[MaxLayer + 1] + GoalGap, NodeMinY};
    }
}

auto FCkGoapRuntimeGraphModel::Reset() -> void
{
    _Nodes.Reset();
    _Edges.Reset();
    _ActionById.Reset();
    _TopologyHash = 0;
    _ActionCount = 0;
    _MaxNameDepth = 1;
}
auto FCkGoapRuntimeGraphModel::FindActionId(const FCk_Handle_Goap_Action& InHandle) const -> uint64
{
    for (const auto& Pair : _ActionById)
        if (Pair.Value == InHandle)
            return Pair.Key;
    return 0;
}
auto FCkGoapRuntimeGraphModel::FindActionById(uint64 InId) const -> const FCk_Handle_Goap_Action*
{
    return _ActionById.Find(InId);
}
