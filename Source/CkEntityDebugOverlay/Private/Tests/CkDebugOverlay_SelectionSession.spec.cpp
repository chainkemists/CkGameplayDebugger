#include "Misc/AutomationTest.h"

#include <limits>

#include "CkEntityDebugOverlay/Selection/CkDebugOverlay_SelectionSession.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_SelectionSettings.h"

namespace ck_debugoverlay::selection_session_spec
{
    using namespace ck_debugoverlay::selection_session;

    auto MakeNode(const uint32 InId, const uint32 InOwner = InvalidEntityId) -> FNode
    {
        auto Result = FNode{};
        Result.Id = InId;
        Result.OwnerId = InOwner;
        Result.HasPosition = true;
        Result.IsOnScreen = true;
        Result.Position = FVector{ static_cast<float>(InId + 1) * 100.0f, 0.0f, 0.0f };
        return Result;
    }

    auto MakeConfig() -> FCk_DebugOverlay_SelectionConfig
    {
        auto Result = FCk_DebugOverlay_SelectionConfig{};
        Result.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;
        Result.RootAnchor = ECk_DebugOverlay_SelectionRootAnchor::Member;
        Result.Scope = ECk_DebugOverlay_SelectionScope::Nearby;
        Result.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
        Result.SearchRadius = 10000.0f;
        Result.ViewBias = 0.7f;
        Result.ConeHalfAngle = 15.0f;
        Result.Order = ECk_DebugOverlay_SelectionOrder::Score;
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_SelectionSession_Test,
    "Ck.DebugOverlay.Selection.Session",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionSession_Test::RunTest(const FString&)
{
    using namespace ck_debugoverlay::selection_session;
    using namespace ck_debugoverlay::selection_session_spec;

    const auto Viewpoint = FViewpoint{};
    auto Config = MakeConfig();
    TestTrue(TEXT("empty input yields no candidates"), BuildCandidates({}, Viewpoint, Config).IsEmpty());
    TestEqual(TEXT("invalid root ID fails closed"), ResolveRoot({}, 5, Config), InvalidEntityId);

    auto EntityZero = MakeNode(0);
    EntityZero.MeaningfulBoundary = true;
    auto Child = MakeNode(1, 0);
    Child.Position = FVector{100.0f, 0.0f, 0.0f};
    auto Nodes = TArray<FNode>{EntityZero, Child};
    TestEqual(TEXT("entity zero is valid root"), ResolveRoot(Nodes, 1, Config), uint32{0});

    auto Transparent = MakeNode(2, 0);
    Transparent.TransparentInfrastructure = true;
    auto Grandchild = MakeNode(3, 2);
    Nodes.Append({Transparent, Grandchild});
    TestEqual(TEXT("transparent owner is walked through"), ResolveRoot(Nodes, 3, Config), uint32{0});

    auto CycleA = MakeNode(40, 41);
    auto CycleB = MakeNode(41, 40);
    TestEqual(TEXT("owner cycle fails closed"), ResolveRoot({CycleA, CycleB}, 40, Config), InvalidEntityId);

    auto Nested = MakeNode(4, 0);
    Nested.MeaningfulBoundary = true;
    auto NestedChild = MakeNode(5, 4);
    Nodes.Append({Nested, NestedChild});
    TestEqual(TEXT("nearest nested boundary is independent"), ResolveRoot(Nodes, 5, Config), uint32{4});

    auto OffscreenParent = MakeNode(6);
    OffscreenParent.MeaningfulBoundary = true;
    OffscreenParent.HasPosition = false;
    auto VisibleChild = MakeNode(7, 6);
    VisibleChild.Position = FVector{100.0f, 0.0f, 0.0f};
    Nodes.Append({OffscreenParent, VisibleChild});
    auto Candidates = BuildCandidates(Nodes, Viewpoint, Config);
    const auto* Promoted = Candidates.FindByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.Id == 6; });
    TestNotNull(TEXT("visible child promotes transformless meaningful parent"), Promoted);
    if (Promoted != nullptr)
    { TestEqual(TEXT("child supplies parent spatial anchor"), Promoted->AnchorId, uint32{7}); }

    auto TransformlessChild = MakeNode(8, 0);
    TransformlessChild.HasPosition = false;
    Nodes.Add(TransformlessChild);
    auto FamilyWithTransformlessChild = BuildFamily(Nodes, 0, Viewpoint, Config);
    const auto* AnchoredChild = FamilyWithTransformlessChild.FindByPredicate(
        [](const FCandidateSelection& InCandidate) { return InCandidate.Id == 8; });
    TestNotNull(TEXT("transformless child is retained with owner spatial anchor"), AnchoredChild);
    if (AnchoredChild != nullptr)
    { TestEqual(TEXT("transformless child uses nearest spatial ancestor"), AnchoredChild->AnchorId, uint32{0}); }

    auto Family = BuildFamily(Nodes, 0, Viewpoint, Config);
    TestFalse(TEXT("family does not expand nested independent root"), Family.ContainsByPredicate(
        [](const FCandidateSelection& InCandidate) { return InCandidate.Id == 4 || InCandidate.Id == 5; }));

    Config.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::LiteralRoots;
    const auto LiteralFamily = BuildFamily(Nodes, 0, Viewpoint, Config);
    TestTrue(TEXT("literal hierarchy expands boundaries which are not independent in that policy"),
        LiteralFamily.ContainsByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.Id == 4; }));
    TestTrue(TEXT("literal hierarchy retains nested boundary descendants"),
        LiteralFamily.ContainsByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.Id == 5; }));
    Config.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;

    auto FamilyRoot = MakeNode(50);
    FamilyRoot.MeaningfulBoundary = true;
    FamilyRoot.HasPosition = false;
    auto DistantOccludedChild = MakeNode(51, 50);
    DistantOccludedChild.Position = FVector{-100000.0f, 0.0f, 0.0f};
    DistantOccludedChild.IsOnScreen = false;
    DistantOccludedChild.Occluded = true;
    auto AnchorlessFamilyChild = MakeNode(52, 50);
    AnchorlessFamilyChild.HasPosition = false;
    Config.Scope = ECk_DebugOverlay_SelectionScope::InView;
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ConeHalfAngle = 1.0f;
    Config.SearchRadius = 1.0f;
    const auto UnfilteredFamily = BuildFamily(
        {FamilyRoot, DistantOccludedChild, AnchorlessFamilyChild}, 50, Viewpoint, Config);
    TestTrue(TEXT("family retains distant occluded off-screen member despite target filters"),
        UnfilteredFamily.ContainsByPredicate([](const FCandidateSelection& InCandidate) { return InCandidate.Id == 51; }));
    const auto* AnchorlessMember = UnfilteredFamily.FindByPredicate(
        [](const FCandidateSelection& InCandidate) { return InCandidate.Id == 52; });
    TestNotNull(TEXT("family retains transformless member without an ancestor anchor"), AnchorlessMember);
    if (AnchorlessMember != nullptr)
    {
        TestEqual(TEXT("anchorless family member has no world-space anchor"), AnchorlessMember->AnchorId, InvalidEntityId);
        TestFalse(TEXT("anchorless family member is not presented on screen"), AnchorlessMember->IsOnScreen);
    }

    auto OccludedChild = MakeNode(61, 60);
    OccludedChild.Occluded = true;
    auto VisibleOwner = MakeNode(60);
    VisibleOwner.MeaningfulBoundary = true;
    Config = MakeConfig();
    const auto OcclusionCandidates = BuildCandidates({VisibleOwner, OccludedChild}, Viewpoint, Config);
    TestTrue(TEXT("occluded nearest spatial anchor does not relocate child selection to visible owner"),
        NOT OcclusionCandidates.ContainsByPredicate([](const FCandidateSelection& InCandidate)
        { return InCandidate.Id == 60 && InCandidate.AnchorId == 61; }));
    TestTrue(TEXT("visible owner remains independently selectable by its own anchor"),
        OcclusionCandidates.ContainsByPredicate([](const FCandidateSelection& InCandidate)
        { return InCandidate.Id == 60 && InCandidate.AnchorId == 60; }));

    auto TiedA = MakeNode(20);
    auto TiedB = MakeNode(10);
    TiedA.Position = TiedB.Position = FVector{100.0f, 0.0f, 0.0f};
    Config.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::AllEntities;
    const auto TieCandidates = BuildCandidates({TiedA, TiedB}, Viewpoint, Config);
    TestTrue(TEXT("equal-score candidates exist"), TieCandidates.Num() == 2);
    if (TieCandidates.Num() == 2)
    { TestEqual(TEXT("equal scores use ID deterministic order"), TieCandidates[0].Id, uint32{10}); }

    Config.Order = ECk_DebugOverlay_SelectionOrder::Screen;
    auto NearlyLeft = MakeNode(81);
    auto NearlyRight = MakeNode(80);
    NearlyLeft.ScreenPos.X = 0.0f;
    NearlyRight.ScreenPos.X = 0.00001f;
    const auto ScreenCandidates = BuildCandidates({NearlyRight, NearlyLeft}, Viewpoint, Config);
    TestTrue(TEXT("near screen positions produce candidates"), ScreenCandidates.Num() == 2);
    if (ScreenCandidates.Num() == 2)
    { TestEqual(TEXT("screen order uses exact position instead of non-transitive near equality"), ScreenCandidates[0].Id, uint32{81}); }
    Config.Order = ECk_DebugOverlay_SelectionOrder::Score;

    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ConeHalfAngle = 5.0f;
    auto OutsideCone = MakeNode(30);
    OutsideCone.Position = FVector{100.0f, 100.0f, 0.0f};
    TestTrue(TEXT("cone excludes off-axis entity"), BuildCandidates({TiedA, OutsideCone}, Viewpoint, Config).Num() == 1);

    auto RootWithEligibleFarMember = MakeNode(70);
    RootWithEligibleFarMember.MeaningfulBoundary = true;
    RootWithEligibleFarMember.Position = FVector{5000.0f, 5000.0f, 0.0f};
    auto EligibleNearMember = MakeNode(71, 70);
    EligibleNearMember.Position = FVector{100.0f, 0.0f, 0.0f};
    EligibleNearMember.IsOnScreen = false;
    Config.Hierarchy = ECk_DebugOverlay_SelectionHierarchy::MeaningfulRoots;
    Config.RootAnchor = ECk_DebugOverlay_SelectionRootAnchor::Member;
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ConeHalfAngle = 15.0f;
    Config.SearchRadius = 500.0f;
    const auto RootCandidates = BuildCandidates({RootWithEligibleFarMember, EligibleNearMember}, Viewpoint, Config);
    TestTrue(TEXT("radius and cone eligible member is considered before an ineligible visible root anchor"),
        RootCandidates.ContainsByPredicate([](const FCandidateSelection& InCandidate)
        { return InCandidate.Id == 70 && InCandidate.AnchorId == 71; }));

    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
    Config.Scope = ECk_DebugOverlay_SelectionScope::InView;
    TiedA.IsOnScreen = false;
    TestTrue(TEXT("in-view scope rejects off-screen candidate"), BuildCandidates({TiedA}, Viewpoint, Config).IsEmpty());
    Config.Scope = ECk_DebugOverlay_SelectionScope::ViewWithNearbyFallback;
    TestTrue(TEXT("nearby fallback retains candidate when view is empty"), BuildCandidates({TiedA}, Viewpoint, Config).Num() == 1);

    Config = MakeConfig();
    Config.SearchRadius = 500.0f;
    Config.Scope = ECk_DebugOverlay_SelectionScope::InView;
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ConeHalfAngle = 5.0f;
    TestTrue(TEXT("discovery starts empty before any candidate spawns"),
        BuildDiscoveryCandidates({}, Viewpoint, Config).IsEmpty());
    auto LateSpawn = MakeNode(90);
    LateSpawn.Position = FVector{600.0f, 0.0f, 0.0f};
    TestTrue(TEXT("discovery starts empty before a candidate enters range"),
        BuildDiscoveryCandidates({LateSpawn}, Viewpoint, Config).IsEmpty());
    LateSpawn.Position = FVector{100.0f, 100.0f, 0.0f};
    auto OutsideConeSecond = MakeNode(93);
    OutsideConeSecond.Position = FVector{100.0f, -100.0f, 0.0f};
    const auto DiscoveryAfterSpawn = BuildDiscoveryCandidates({LateSpawn, OutsideConeSecond}, Viewpoint, Config);
    TestEqual(TEXT("discovery admits every late range entry despite the aim cone"), DiscoveryAfterSpawn.Num(), 2);
    TestTrue(TEXT("aim policy still excludes every outside-cone candidate"),
        BuildCandidates({LateSpawn, OutsideConeSecond}, Viewpoint, Config).IsEmpty());
    LateSpawn.Position = FVector{600.0f, 0.0f, 0.0f};
    TestTrue(TEXT("discovery removes a candidate that exits range"),
        BuildDiscoveryCandidates({LateSpawn}, Viewpoint, Config).IsEmpty());
    LateSpawn.Position = FVector{100.0f, 100.0f, 0.0f};
    TestEqual(TEXT("discovery readmits a re-entered candidate"),
        BuildDiscoveryCandidates({LateSpawn}, Viewpoint, Config).Num(), 1);

    auto DiscoveryRoot = MakeNode(91);
    DiscoveryRoot.MeaningfulBoundary = true;
    DiscoveryRoot.HasPosition = false;
    auto DiscoveryChild = MakeNode(92, 91);
    DiscoveryChild.Position = FVector{100.0f, 0.0f, 0.0f};
    const auto HierarchicalDiscovery = BuildDiscoveryCandidates({DiscoveryRoot, DiscoveryChild}, Viewpoint, Config);
    TestEqual(TEXT("discovery preserves meaningful hierarchy roots"), HierarchicalDiscovery.Num(), 1);
    if (HierarchicalDiscovery.Num() == 1)
    {
        TestEqual(TEXT("discovery promotes the meaningful root"), HierarchicalDiscovery[0].Id, uint32{91});
        TestEqual(TEXT("discovery preserves the child spatial anchor"), HierarchicalDiscovery[0].AnchorId, uint32{92});
    }

    const auto StableOrder = ReconcileOrder({4, 2, 2, 1}, {3, 2, 5, 4, 5}, false);
    const auto ExpectedStableOrder = TArray<uint32>{4, 2, 3, 5};
    TestTrue(TEXT("stable order retains surviving IDs then appends discoveries"), StableOrder == ExpectedStableOrder);
    const auto RerankedOrder = ReconcileOrder({4, 2, 2, 1}, {3, 2, 5, 4, 5}, true);
    const auto ExpectedRerankedOrder = TArray<uint32>{3, 2, 5, 4};
    TestTrue(TEXT("live order follows current ranking without duplicates"), RerankedOrder == ExpectedRerankedOrder);
    const auto PrunedOrder = ReconcileOrder({InvalidEntityId, 4, 1}, {InvalidEntityId, 4}, false);
    const auto ExpectedPrunedOrder = TArray<uint32>{4};
    TestTrue(TEXT("order reconciliation removes vanished and invalid IDs"), PrunedOrder == ExpectedPrunedOrder);

    const auto AimValid = TSet<uint32>{10, 20};
    TestEqual(TEXT("manual cycle persists while aim winner stays stationary"),
        ResolveAimFocus(10, 10, 20, AimValid), uint32{20});
    TestEqual(TEXT("changed aim winner replaces the manual cycle target"),
        ResolveAimFocus(20, 10, 20, AimValid), uint32{20});
    TestEqual(TEXT("changed aim winner replaces a different manual target"),
        ResolveAimFocus(20, 10, 10, AimValid), uint32{20});
    TestEqual(TEXT("missing manual target falls back to the stationary aim winner"),
        ResolveAimFocus(10, 10, 20, TSet<uint32>{10}), uint32{10});
    TestEqual(TEXT("invalid changed aim clears focus instead of retaining a lock"),
        ResolveAimFocus(InvalidEntityId, 10, 20, AimValid), InvalidEntityId);
    TestEqual(TEXT("explicit selection lock survives a changed aim winner"),
        ResolveAimFocus(20, 10, InvalidEntityId, AimValid, 10), uint32{10});
    TestEqual(TEXT("a lifetime-validated selection lock survives range exit"),
        ResolveAimFocus(20, 10, InvalidEntityId, TSet<uint32>{20}, 10), uint32{10});
    TestEqual(TEXT("releasing or invalidating the lock resumes current aim"),
        ResolveAimFocus(20, 10, InvalidEntityId, AimValid, InvalidEntityId), uint32{20});

    Config = MakeConfig();
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Cone;
    Config.ConeHalfAngle = 15.0f;
    Config.SearchRadius = 1.0f;
    auto FamilyOutsideCone = FCandidateSelection{};
    FamilyOutsideCone.Id = 101;
    FamilyOutsideCone.Score = 0.9f;
    FamilyOutsideCone.Distance = 100000.0f;
    FamilyOutsideCone.AngleRadians = FMath::DegreesToRadians(30.0f);
    FamilyOutsideCone.IsOnScreen = true;
    auto FamilyInsideCone = FCandidateSelection{};
    FamilyInsideCone.Id = 102;
    FamilyInsideCone.Score = 0.2f;
    FamilyInsideCone.Distance = 100000.0f;
    FamilyInsideCone.AngleRadians = FMath::DegreesToRadians(5.0f);
    FamilyInsideCone.IsOnScreen = true;
    TestEqual(TEXT("family aim rejects a visible outside-cone member"),
        PickAimCandidate({FamilyOutsideCone, FamilyInsideCone}, Config, true), uint32{102});
    auto MalformedAngle = FamilyInsideCone;
    MalformedAngle.AngleRadians = std::numeric_limits<float>::quiet_NaN();
    TestEqual(TEXT("family cone rejects an invalid angular measurement"),
        PickAimCandidate({MalformedAngle}, Config, true), InvalidEntityId);
    Config.Targeting = ECk_DebugOverlay_SelectionTargeting::Weighted;
    TestEqual(TEXT("weighted family aim keeps range-exempt visible members"),
        PickAimCandidate({FamilyOutsideCone, FamilyInsideCone}, Config, true), uint32{101});
    auto FamilyTieA = FamilyInsideCone;
    FamilyTieA.Id = 8;
    FamilyTieA.Score = 0.5f;
    auto FamilyTieB = FamilyInsideCone;
    FamilyTieB.Id = 9;
    FamilyTieB.Score = 0.5f;
    TestEqual(TEXT("aim ties use the smaller entity ID"),
        PickAimCandidate({FamilyTieB, FamilyTieA}, Config, true), uint32{8});
    TestEqual(TEXT("empty aim list fails closed"), PickAimCandidate({}, Config, true), InvalidEntityId);

    auto FarLeft = FCandidateSelection{};
    FarLeft.Id = 10;
    FarLeft.ScreenPos = FVector2D{0.0, 100.0};
    FarLeft.IsOnScreen = true;
    auto NearLeft = FCandidateSelection{};
    NearLeft.Id = 20;
    NearLeft.ScreenPos = FVector2D{90.0, 100.0};
    NearLeft.IsOnScreen = true;
    auto Selected = FCandidateSelection{};
    Selected.Id = 30;
    Selected.ScreenPos = FVector2D{100.0, 100.0};
    Selected.IsOnScreen = true;
    auto NearRight = FCandidateSelection{};
    NearRight.Id = 40;
    NearRight.ScreenPos = FVector2D{110.0, 100.0};
    NearRight.IsOnScreen = true;
    auto FarRight = FCandidateSelection{};
    FarRight.Id = 50;
    FarRight.ScreenPos = FVector2D{200.0, 100.0};
    FarRight.IsOnScreen = true;
    auto HiddenNearLeft = FCandidateSelection{};
    HiddenNearLeft.Id = 15;
    HiddenNearLeft.ScreenPos = FVector2D{95.0, 100.0};
    const auto RelativeOrder = BuildSpatialOrder(
        {FarRight, NearLeft, HiddenNearLeft, Selected, FarLeft, NearRight}, 30, InvalidEntityId);
    TestTrue(TEXT("spatial order places nearest screen neighbors beside selection"),
        RelativeOrder == TArray<uint32>{10, 20, 30, 40, 50});
    TestFalse(TEXT("off-screen candidates do not distort visible badge steps"), RelativeOrder.Contains(15));
    auto CoLocatedBefore = Selected;
    CoLocatedBefore.Id = 29;
    auto CoLocatedAfter = Selected;
    CoLocatedAfter.Id = 31;
    TestTrue(TEXT("co-located candidates use deterministic IDs around selection"),
        BuildSpatialOrder({CoLocatedAfter, Selected, CoLocatedBefore}, 30, InvalidEntityId) ==
            TArray<uint32>{29, 30, 31});
    auto MalformedScreenPosition = Selected;
    MalformedScreenPosition.Id = 60;
    MalformedScreenPosition.ScreenPos.X = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("nonfinite projected candidates fail closed"),
        BuildSpatialOrder({Selected, MalformedScreenPosition}, 30, InvalidEntityId).Contains(60));
    const auto Relative = BuildRelativeLabels(RelativeOrder, 30, InvalidEntityId);
    TestEqual(TEXT("relative selected entity is zero"), Relative.FindRef(30), FString{TEXT("0")});
    TestEqual(TEXT("relative next is plus one"), Relative.FindRef(40), FString{TEXT("+1")});
    TestEqual(TEXT("relative previous is minus one"), Relative.FindRef(20), FString{TEXT("-1")});
    TestEqual(TEXT("relative second next is plus two"), Relative.FindRef(50), FString{TEXT("+2")});
    TestEqual(TEXT("relative second previous is minus two"), Relative.FindRef(10), FString{TEXT("-2")});
    const auto ShiftedRelative = BuildRelativeLabels(RelativeOrder, 50, InvalidEntityId);
    TestEqual(TEXT("moving selection updates relative zero"), ShiftedRelative.FindRef(50), FString{TEXT("0")});
    TestEqual(TEXT("right-edge selection keeps every left candidate negative"),
        ShiftedRelative.FindRef(10), FString{TEXT("-4")});
    TestEqual(TEXT("left-edge selection keeps every right candidate positive"),
        BuildRelativeLabels(RelativeOrder, 10, InvalidEntityId).FindRef(50), FString{TEXT("+4")});
    TestEqual(TEXT("hidden selected child is relative to visible root"),
        BuildRelativeLabels(RelativeOrder, 99, 30).FindRef(30), FString{TEXT("0")});
    const auto UnselectedRelative = BuildRelativeLabels(RelativeOrder, InvalidEntityId, InvalidEntityId);
    TestEqual(TEXT("without focus first candidate matches Next"), UnselectedRelative.FindRef(10), FString{TEXT("+1")});
    TestEqual(TEXT("without focus labels follow deterministic screen order"), UnselectedRelative.FindRef(50), FString{TEXT("+5")});
    TestTrue(TEXT("empty relative order is safe"), BuildRelativeLabels({}, 10, 10).IsEmpty());
    TestEqual(TEXT("relative labels discard invalid and duplicate entries"),
        BuildRelativeLabels({10, 10, InvalidEntityId}, 10, 10).Num(), 1);

    const auto Ordered = TArray<uint32>{0, 1, 2, 3};
    const auto Valid = TSet<uint32>{0, 2, 3};
    TestEqual(TEXT("cycle skips removed frozen gap"), Cycle(Ordered, Valid, 0, InvalidEntityId, 1), uint32{2});
    TestEqual(TEXT("cycle re-enters from hidden selected child root"), Cycle(Ordered, Valid, 99, 2, 1), uint32{3});
    TestEqual(TEXT("next stops at the right edge"), Cycle(Ordered, Valid, 3, InvalidEntityId, 1), InvalidEntityId);
    TestEqual(TEXT("previous stops at the left edge"), Cycle(Ordered, Valid, 0, InvalidEntityId, -1), InvalidEntityId);
    TestEqual(TEXT("cycle empty valid set fails closed"), Cycle(Ordered, {}, 0, 0, 1), InvalidEntityId);
    return true;
}
