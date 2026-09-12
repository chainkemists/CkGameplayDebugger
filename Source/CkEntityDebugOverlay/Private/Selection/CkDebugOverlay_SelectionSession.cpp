#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_SelectionSession.h"

#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"

#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "Math/UnrealMathUtility.h"

namespace ck_debugoverlay::selection_session
{
    namespace
    {
        auto BuildNodeMap(const TArray<FNode>& InNodes) -> TMap<uint32, const FNode*>
        {
            auto Result = TMap<uint32, const FNode*>{};
            for (const auto& Node : InNodes)
            {
                if (Node.Id != InvalidEntityId && NOT Result.Contains(Node.Id))
                { Result.Add(Node.Id, &Node); }
            }
            return Result;
        }

        auto IsEligibleNode(const FNode& InNode) -> bool
        {
            return InNode.Id != InvalidEntityId && InNode.Selectable && NOT InNode.TransparentInfrastructure;
        }

        auto FindSpatialAnchor(
            const TMap<uint32, const FNode*>&          InById,
            const FNode&                               InNode,
            const FCk_DebugOverlay_SelectionConfig&     InConfig,
            const bool                                  InAllowOccluded) -> const FNode*
        {
            const auto* Current = &InNode;
            auto Seen = TSet<uint32>{};
            while (Current != nullptr && NOT Seen.Contains(Current->Id))
            {
                Seen.Add(Current->Id);
                if (Current->HasPosition)
                {
                    return (InAllowOccluded || InConfig.IncludeOccluded || NOT Current->Occluded)
                        ? Current
                        : nullptr;
                }
                Current = Current->OwnerId == InvalidEntityId ? nullptr : InById.FindRef(Current->OwnerId);
            }
            return nullptr;
        }

        auto IsDescendantOf(
            const TMap<uint32, const FNode*>& InById,
            const uint32                      InId,
            const uint32                      InRootId) -> bool
        {
            auto Current = InId;
            auto Seen = TSet<uint32>{};
            while (Current != InvalidEntityId && NOT Seen.Contains(Current))
            {
                Seen.Add(Current);
                if (Current == InRootId)
                { return true; }
                const auto* Node = InById.FindRef(Current);
                if (Node == nullptr)
                { return false; }
                Current = Node->OwnerId;
            }
            return false;
        }

        auto HasNestedBoundaryBeforeRoot(
            const TMap<uint32, const FNode*>& InById,
            const uint32                      InId,
            const uint32                      InRootId) -> bool
        {
            auto Current = InId;
            auto Seen = TSet<uint32>{};
            while (Current != InvalidEntityId && Current != InRootId && NOT Seen.Contains(Current))
            {
                Seen.Add(Current);
                const auto* Node = InById.FindRef(Current);
                if (Node == nullptr)
                { return true; }
                if (Node->MeaningfulBoundary)
                { return true; }
                Current = Node->OwnerId;
            }
            return Current != InRootId;
        }

        auto MakeCandidate(const FNode& InRoot, const FNode& InAnchor, const FViewpoint& InViewpoint,
            const FCk_DebugOverlay_SelectionConfig& InConfig) -> FCandidateSelection
        {
            const auto ToAnchor = InAnchor.Position - InViewpoint.Location;
            const auto Distance = ToAnchor.Size();
            const auto Direction = Distance > KINDA_SMALL_NUMBER ? ToAnchor / Distance : InViewpoint.Forward.GetSafeNormal();
            const auto Forward = InViewpoint.Forward.GetSafeNormal();
            const auto Alignment = FMath::Clamp((FVector::DotProduct(Direction, Forward) + 1.0f) * 0.5f, 0.0f, 1.0f);
            const auto Proximity = InConfig.SearchRadius > KINDA_SMALL_NUMBER
                ? FMath::Clamp(1.0f - Distance / InConfig.SearchRadius, 0.0f, 1.0f) : 0.0f;

            auto Result = FCandidateSelection{};
            Result.Id = InRoot.Id;
            Result.RootId = InRoot.Id;
            Result.AnchorId = InAnchor.Id;
            Result.WorldLocation = InAnchor.Position;
            Result.ScreenPos = InAnchor.ScreenPos;
            Result.Distance = Distance;
            Result.AngleRadians = FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Forward), -1.0f, 1.0f));
            Result.IsOnScreen = InAnchor.IsOnScreen;
            Result.Score = InConfig.ViewBias * Alignment + (1.0f - InConfig.ViewBias) * Proximity;
            return Result;
        }

        /** An AnchorId of InvalidEntityId directs presentation to omit a world-space diamond. */
        auto MakeAnchorlessFamilyCandidate(const FNode& InRoot, const FNode& InNode) -> FCandidateSelection
        {
            auto Result = FCandidateSelection{};
            Result.Id = InNode.Id;
            Result.RootId = InRoot.Id;
            Result.AnchorId = InvalidEntityId;
            Result.IsOnScreen = false;
            return Result;
        }

        auto IsWithinTargetingBounds(
            const FCandidateSelection&                 InCandidate,
            const FCk_DebugOverlay_SelectionConfig& InConfig) -> bool
        {
            if (InCandidate.Distance > InConfig.SearchRadius)
            { return false; }
            return InConfig.Targeting != ECk_DebugOverlay_SelectionTargeting::Cone ||
                InCandidate.AngleRadians <= FMath::DegreesToRadians(InConfig.ConeHalfAngle);
        }

        auto BuildScopedCandidates(const TArray<FNode>& InNodes, const FViewpoint& InViewpoint,
            const FCk_DebugOverlay_SelectionConfig& InConfig, const uint32 InFamilyRootId) -> TArray<FCandidateSelection>
        {
            const auto ById = BuildNodeMap(InNodes);
            const auto* Root = ById.FindRef(InFamilyRootId);
            if (Root == nullptr)
            { return {}; }

            auto Result = TArray<FCandidateSelection>{};
            for (const auto& Node : InNodes)
            {
                if (NOT IsEligibleNode(Node) || NOT IsDescendantOf(ById, Node.Id, InFamilyRootId))
                { continue; }
                if (InConfig.Hierarchy != ECk_DebugOverlay_SelectionHierarchy::LiteralRoots &&
                    HasNestedBoundaryBeforeRoot(ById, Node.Id, InFamilyRootId))
                { continue; }
                // Family inspection retains the root's direct members independently of world filters.
                // Nested meaningful roots deliberately own separate families.
                const auto* Anchor = FindSpatialAnchor(ById, Node, InConfig, true);
                if (Anchor == nullptr)
                {
                    Result.Add(MakeAnchorlessFamilyCandidate(*Root, Node));
                    continue;
                }
                auto Candidate = MakeCandidate(*Root, *Anchor, InViewpoint, InConfig);
                Candidate.Id = Node.Id;
                Candidate.AnchorId = Anchor->Id;
                Result.Add(Candidate);
            }
            return Result;
        }

        auto SortCandidates(TArray<FCandidateSelection>& InOutCandidates, const FCk_DebugOverlay_SelectionConfig& InConfig) -> void
        {
            InOutCandidates.Sort([&InConfig](const FCandidateSelection& InA, const FCandidateSelection& InB)
            {
                if (InConfig.Order == ECk_DebugOverlay_SelectionOrder::Screen)
                {
                    const auto X_A = FMath::IsFinite(InA.ScreenPos.X) ? InA.ScreenPos.X : MAX_dbl;
                    const auto X_B = FMath::IsFinite(InB.ScreenPos.X) ? InB.ScreenPos.X : MAX_dbl;
                    if (X_A != X_B)
                    { return X_A < X_B; }
                }
                else
                {
                    const auto ScoreA = FMath::IsFinite(InA.Score) ? InA.Score : -MAX_flt;
                    const auto ScoreB = FMath::IsFinite(InB.Score) ? InB.Score : -MAX_flt;
                    if (ScoreA != ScoreB)
                    { return ScoreA > ScoreB; }
                }
                return InA.Id < InB.Id;
            });
        }

        auto ResolveRootById(
            const TMap<uint32, const FNode*>&          InById,
            const uint32                                InId,
            const FCk_DebugOverlay_SelectionConfig& InConfig) -> uint32
        {
            const auto* Start = InById.FindRef(InId);
            if (Start == nullptr || Start->TransparentInfrastructure)
            { return InvalidEntityId; }
            if (InConfig.Hierarchy == ECk_DebugOverlay_SelectionHierarchy::AllEntities)
            { return InId; }

            auto Current = Start;
            auto LastNonTransparent = Current;
            auto Seen = TSet<uint32>{};
            auto ReachedLineageEnd = false;
            while (Current != nullptr && NOT Seen.Contains(Current->Id))
            {
                Seen.Add(Current->Id);
                if (InConfig.Hierarchy == ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots &&
                    Current->MeaningfulBoundary)
                { return Current->Id; }
                if (NOT Current->TransparentInfrastructure)
                { LastNonTransparent = Current; }
                if (Current->OwnerId == InvalidEntityId)
                {
                    ReachedLineageEnd = true;
                    break;
                }
                Current = InById.FindRef(Current->OwnerId);
            }
            return ReachedLineageEnd ? LastNonTransparent->Id : InvalidEntityId;
        }
    }

    auto ResolveRoot(const TArray<FNode>& InNodes, const uint32 InId,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> uint32
    {
        const auto ById = BuildNodeMap(InNodes);
        return ResolveRootById(ById, InId, InConfig);
    }

    auto BuildCandidates(const TArray<FNode>& InNodes, const FViewpoint& InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>
    {
        const auto ById = BuildNodeMap(InNodes);
        auto AnchorsByRoot = TMap<uint32, TArray<const FNode*>>{};
        for (const auto& Node : InNodes)
        {
            if (NOT IsEligibleNode(Node))
            { continue; }
            const auto RootId = ResolveRootById(ById, Node.Id, InConfig);
            const auto* Root = ById.FindRef(RootId);
            if (Root == nullptr)
            { continue; }
            const auto* Anchor = FindSpatialAnchor(ById, Node, InConfig, false);
            if (Anchor == nullptr)
            { continue; }
            if (InConfig.RootAnchor == ECk_DebugOverlay_SelectionRootAnchor::Root && Node.Id != RootId)
            { continue; }
            if (NOT IsWithinTargetingBounds(MakeCandidate(*Root, *Anchor, InViewpoint, InConfig), InConfig))
            { continue; }
            AnchorsByRoot.FindOrAdd(RootId).Add(Anchor);
        }

        auto Result = TArray<FCandidateSelection>{};
        for (const auto& Pair : AnchorsByRoot)
        {
            const auto* Root = ById.FindRef(Pair.Key);
            if (Root == nullptr)
            { continue; }
            const auto* BestAnchor = Pair.Value[0];
            for (const auto* CandidateAnchor : Pair.Value)
            {
                const auto Best = MakeCandidate(*Root, *BestAnchor, InViewpoint, InConfig);
                const auto Candidate = MakeCandidate(*Root, *CandidateAnchor, InViewpoint, InConfig);
                const auto CandidateScoreIsFinite = FMath::IsFinite(Candidate.Score);
                const auto BestScoreIsFinite = FMath::IsFinite(Best.Score);
                const auto CandidateScoreIsHigher = CandidateScoreIsFinite &&
                    (NOT BestScoreIsFinite || Candidate.Score > Best.Score);
                const auto ScoresAreExactlyEqual = CandidateScoreIsFinite && BestScoreIsFinite &&
                    Candidate.Score == Best.Score;
                const auto PreferCandidate = Candidate.IsOnScreen != Best.IsOnScreen
                    ? Candidate.IsOnScreen
                    : CandidateScoreIsHigher ||
                        (ScoresAreExactlyEqual && Candidate.AnchorId < Best.AnchorId);
                if (PreferCandidate)
                { BestAnchor = CandidateAnchor; }
            }
            const auto Candidate = MakeCandidate(*Root, *BestAnchor, InViewpoint, InConfig);
            Result.Add(Candidate);
        }

        if (InConfig.Scope == ECk_DebugOverlay_SelectionScope::InView ||
            (InConfig.Scope == ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback &&
             Result.ContainsByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.IsOnScreen; })))
        {
            Result = Result.FilterByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.IsOnScreen; });
        }
        SortCandidates(Result, InConfig);
        return Result;
    }

    auto BuildDiscoveryCandidates(const TArray<FNode>& InNodes, const FViewpoint& InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>
    {
        auto DiscoveryConfig = InConfig;
        DiscoveryConfig.Scope = ECk_DebugOverlay_SelectionScope::Nearby;
        DiscoveryConfig.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
        return BuildCandidates(InNodes, InViewpoint, DiscoveryConfig);
    }

    auto PickAimCandidate(const TArray<FCandidateSelection>& InCandidates,
        const FCk_DebugOverlay_SelectionConfig& InConfig, const bool InFamily) -> uint32
    {
        const auto* Best = static_cast<const FCandidateSelection*>(nullptr);
        for (const auto& Candidate : InCandidates)
        {
            if (Candidate.Id == InvalidEntityId || NOT FMath::IsFinite(Candidate.Score))
            { continue; }
            if (InFamily)
            {
                if (NOT Candidate.IsOnScreen)
                { continue; }
                if (InConfig.Targeting == ECk_DebugOverlay_SelectionTargeting::Cone &&
                    (NOT FMath::IsFinite(Candidate.AngleRadians) || Candidate.AngleRadians < 0.0f ||
                     Candidate.AngleRadians > FMath::DegreesToRadians(InConfig.ConeHalfAngle)))
                { continue; }
            }
            if (Best == nullptr || Candidate.Score > Best->Score ||
                (Candidate.Score == Best->Score && Candidate.Id < Best->Id))
            { Best = &Candidate; }
        }
        return Best != nullptr ? Best->Id : InvalidEntityId;
    }

    auto BuildFamily(const TArray<FNode>& InNodes, const uint32 InRootId, const FViewpoint& InViewpoint,
        const FCk_DebugOverlay_SelectionConfig& InConfig) -> TArray<FCandidateSelection>
    {
        auto Result = BuildScopedCandidates(InNodes, InViewpoint, InConfig, InRootId);
        SortCandidates(Result, InConfig);
        return Result;
    }

    auto Cycle(const TArray<uint32>& InOrderedIds, const TSet<uint32>& InValidIds, const uint32 InSelectedId,
        const uint32 InSelectedRootId, const int32 InDirection) -> uint32
    {
        auto ValidOrder = TArray<uint32>{};
        for (const auto Id : InOrderedIds)
        {
            if (Id != InvalidEntityId && InValidIds.Contains(Id))
            { ValidOrder.Add(Id); }
        }
        if (ValidOrder.IsEmpty())
        { return InvalidEntityId; }

        auto CurrentIndex = ValidOrder.IndexOfByKey(InSelectedId);
        if (CurrentIndex == INDEX_NONE)
        { CurrentIndex = ValidOrder.IndexOfByKey(InSelectedRootId); }
        if (CurrentIndex == INDEX_NONE)
        { return InDirection < 0 ? ValidOrder.Last() : ValidOrder[0]; }
        const auto Step = InDirection < 0 ? -1 : 1;
        const auto TargetIndex = CurrentIndex + Step;
        return ValidOrder.IsValidIndex(TargetIndex) ? ValidOrder[TargetIndex] : InvalidEntityId;
    }

    auto ReconcileOrder(const TArray<uint32>& InPreviousIds, const TArray<uint32>& InCurrentIds,
        const bool InRerank) -> TArray<uint32>
    {
        auto Current = TArray<uint32>{};
        auto CurrentSet = TSet<uint32>{};
        for (const auto Id : InCurrentIds)
        {
            if (Id != InvalidEntityId && NOT CurrentSet.Contains(Id))
            {
                Current.Add(Id);
                CurrentSet.Add(Id);
            }
        }
        if (InRerank)
        { return Current; }

        auto Result = TArray<uint32>{};
        auto Added = TSet<uint32>{};
        for (const auto Id : InPreviousIds)
        {
            if (CurrentSet.Contains(Id) && NOT Added.Contains(Id))
            {
                Result.Add(Id);
                Added.Add(Id);
            }
        }
        for (const auto Id : Current)
        {
            if (NOT Added.Contains(Id))
            {
                Result.Add(Id);
                Added.Add(Id);
            }
        }
        return Result;
    }

    auto BuildSpatialOrder(const TArray<FCandidateSelection>& InCandidates, const uint32 InSelectedId,
        const uint32 InSelectedRootId) -> TArray<uint32>
    {
        const auto IsFiniteScreenPosition = [](const FCandidateSelection& InCandidate) -> bool
        {
            return FMath::IsFinite(InCandidate.ScreenPos.X) && FMath::IsFinite(InCandidate.ScreenPos.Y);
        };
        auto Candidates = TArray<FCandidateSelection>{};
        auto Seen = TSet<uint32>{};
        for (const auto& Candidate : InCandidates)
        {
            if (Candidate.Id != InvalidEntityId && Candidate.IsOnScreen &&
                IsFiniteScreenPosition(Candidate) && NOT Seen.Contains(Candidate.Id))
            {
                Candidates.Add(Candidate);
                Seen.Add(Candidate.Id);
            }
        }

        const auto FindCandidate = [&Candidates](const uint32 InId) -> const FCandidateSelection*
        {
            return Candidates.FindByPredicate([InId](const auto& InCandidate)
                { return InCandidate.Id == InId; });
        };
        const auto* Anchor = FindCandidate(InSelectedId);
        if (Anchor == nullptr)
        { Anchor = FindCandidate(InSelectedRootId); }

        const auto ScreenPositionLess = [](
            const FCandidateSelection& InLeft, const FCandidateSelection& InRight) -> bool
        {
            if (InLeft.ScreenPos.X != InRight.ScreenPos.X)
            { return InLeft.ScreenPos.X < InRight.ScreenPos.X; }
            if (InLeft.ScreenPos.Y != InRight.ScreenPos.Y)
            { return InLeft.ScreenPos.Y < InRight.ScreenPos.Y; }
            return InLeft.Id < InRight.Id;
        };

        if (Anchor == nullptr)
        {
            Candidates.Sort(ScreenPositionLess);
            auto Result = TArray<uint32>{};
            for (const auto& Candidate : Candidates)
            { Result.Add(Candidate.Id); }
            return Result;
        }

        const auto AnchorValue = *Anchor;
        auto Before = TArray<FCandidateSelection>{};
        auto After = TArray<FCandidateSelection>{};
        for (const auto& Candidate : Candidates)
        {
            if (Candidate.Id == AnchorValue.Id)
            { continue; }
            if (ScreenPositionLess(Candidate, AnchorValue))
            { Before.Add(Candidate); }
            else
            { After.Add(Candidate); }
        }

        const auto DistanceSquared = [&AnchorValue](const FCandidateSelection& InCandidate) -> double
        { return FVector2D::DistSquared(InCandidate.ScreenPos, AnchorValue.ScreenPos); };
        Before.Sort([&DistanceSquared](const auto& InLeft, const auto& InRight)
        {
            const auto LeftDistance = DistanceSquared(InLeft);
            const auto RightDistance = DistanceSquared(InRight);
            return LeftDistance != RightDistance ? LeftDistance > RightDistance : InLeft.Id < InRight.Id;
        });
        After.Sort([&DistanceSquared](const auto& InLeft, const auto& InRight)
        {
            const auto LeftDistance = DistanceSquared(InLeft);
            const auto RightDistance = DistanceSquared(InRight);
            return LeftDistance != RightDistance ? LeftDistance < RightDistance : InLeft.Id < InRight.Id;
        });
        auto Result = TArray<uint32>{};
        Result.Reserve(Candidates.Num());
        for (const auto& Candidate : Before)
        { Result.Add(Candidate.Id); }
        Result.Add(AnchorValue.Id);
        for (const auto& Candidate : After)
        { Result.Add(Candidate.Id); }
        return Result;
    }

    auto BuildRelativeLabels(const TArray<uint32>& InOrderedIds, const uint32 InSelectedId,
        const uint32 InSelectedRootId) -> TMap<uint32, FString>
    {
        const auto Order = ReconcileOrder({}, InOrderedIds, true);
        auto Result = TMap<uint32, FString>{};
        auto Anchor = Order.IndexOfByKey(InSelectedId);
        if (Anchor == INDEX_NONE)
        { Anchor = Order.IndexOfByKey(InSelectedRootId); }
        for (auto Index = 0; Index < Order.Num(); ++Index)
        {
            auto Label = FString{};
            if (Anchor == INDEX_NONE)
            { Label = FString::Printf(TEXT("+%d"), Index + 1); }
            else if (Index == Anchor)
            { Label = TEXT("0"); }
            else if (Index < Anchor)
            { Label = FString::Printf(TEXT("-%d"), Anchor - Index); }
            else
            { Label = FString::Printf(TEXT("+%d"), Index - Anchor); }
            Result.Add(Order[Index], MoveTemp(Label));
        }
        return Result;
    }

    auto ResolveAimFocus(const uint32 InAimId, const uint32 InPreviousAimId, const uint32 InManualId,
        const TSet<uint32>& InValidIds, const uint32 InValidatedLockId) -> uint32
    {
        if (InValidatedLockId != InvalidEntityId)
        { return InValidatedLockId; }
        if (InAimId == InPreviousAimId && InManualId != InvalidEntityId && InValidIds.Contains(InManualId))
        { return InManualId; }
        return InAimId != InvalidEntityId && InValidIds.Contains(InAimId) ? InAimId : InvalidEntityId;
    }
}
