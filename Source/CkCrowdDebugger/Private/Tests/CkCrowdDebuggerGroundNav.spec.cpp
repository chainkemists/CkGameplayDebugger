#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Two boundaries, both checked without a world behind them.
//
// The GroundNav scene role takes the debugger's own COPY of one bake and draws it: these build a
// snapshot by hand, copy it, destroy the source, and assert the overlay still renders the plate
// outlines and boundary runs the copy carries. The shadow-parity rows are decided by a copied
// diagnostics fragment alone, so those are read back with no shadow entity and no Slate tree.
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/Viewport/CkCrowdDebugger_3dSceneAdapter.h"
#include "CkCrowdDebugger/Window/SCkCrowdDebugger_ShadowParityPanel.h"

#include "CkDebugScene/CkDebugScene_Target.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "CkGroundNav/Backend/CkGroundNav_GeometryBackend_Stub.h"
#include "CkGroundNav/Bake/CkGroundNav_AgentProfile.h"
#include "CkGroundNav/Debug/CkGroundNav_DebugSnapshot.h"
#include "CkGroundNav/Field/CkGroundNav_Field.h"

#include "CkShapes/Capsule/CkShapeCapsule_Fragment_Data.h"

#include "Engine/World.h"

namespace ck_crowd_debugger_groundnav_spec
{
constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
constexpr auto PlateCount = 3;
constexpr auto BoundaryCount = 2;

struct FScopedTarget
{
    // The budget is a parameter because a REAL bake has a real field's worth of plates and boundary
    // runs in it, where the hand-authored capture below has five items: a target too small refuses the
    // upsert and the reconcile aborts, which would read as a copy failure rather than as a full target.
    explicit FScopedTarget(int32 InMaxItems = 128, int32 InMaxInstances = 20000)
    {
        constexpr auto InformEngineOfWorld = false;
        _World = UWorld::CreateWorld(EWorldType::Game, InformEngineOfWorld,
                                    FName{TEXT("CkCrowdDebuggerGroundNav")});
        _Target = MakeShared<FCk_DebugScene_Target>(
            FCk_DebugScene_TargetConfig{}.Set_World(_World).Set_MaxItems(InMaxItems)
                .Set_MaxInstances(InMaxInstances));
    }

    ~FScopedTarget()
    {
        _Target.Reset();
        if (IsValid(_World))
        {
            constexpr auto InformEngineOfWorld = false;
            _World->DestroyWorld(InformEngineOfWorld);
            _World = nullptr;
        }
    }

    UWorld* _World = nullptr;
    TSharedPtr<FCk_DebugScene_Target> _Target;
};

// A bake with three plates on two layers and two boundary runs, one of them a tile rim. Plate bounds
// are kept apart so no two collapse onto one identity, which is what the reorder assertion measures.
auto
MakeFieldSnapshot() -> ck::groundnav::FCk_GroundNav_DebugSnapshot
{
    auto Field = ck::groundnav::FCk_GroundNav_DebugSnapshot{};
    Field._Status = ck::groundnav::EDebugSnapshotStatus::Current;
    Field._CellSizeUu = 25.0f;
    Field._Region = FBox{FVector{-500.0, -500.0, 0.0}, FVector{500.0, 500.0, 300.0}};

    for (auto Index = 0; Index < PlateCount; ++Index)
    {
        auto Plate = ck::groundnav::FCk_GroundNav_DebugPlate{};
        const auto MinX = -400.0 + Index * 300.0;
        Plate._Bounds = FBox{FVector{MinX, -200.0, 0.0}, FVector{MinX + 200.0, 200.0, 0.0}};
        Plate._LayerIndex = Index % 2;
        Field._Plates.Add(Plate);
    }

    auto Run = ck::groundnav::FCk_GroundNav_DebugBoundary{};
    Run._Start = FVector{-400.0, -200.0, 0.0};
    Run._End = FVector{-200.0, -200.0, 0.0};
    Run._InwardNormalXY = FVector2D{0.0, 1.0};
    Field._Boundary.Add(Run);

    Run._Start = FVector{200.0, 200.0, 0.0};
    Run._End = FVector{400.0, 200.0, 0.0};
    Run._InwardNormalXY = FVector2D{0.0, -1.0};
    Run._IsTileRim = true;
    Field._Boundary.Add(Run);

    return Field;
}

// A real 2x2 field over ground that reaches past the lattice on every side - the same shape
// CkGroundNav's own snapshot pins bake, driven through the same stub geometry backend, so what the
// copy is asserted against is a capture the production path produced rather than one written by hand.
auto
Bake_FixtureField() -> TSharedPtr<ck::groundnav::FCk_GroundNav_Field>
{
    const auto Backend = ck::groundnav::FCk_GroundNav_GeometryBackend_Stub{
        TArray<FBox>{FBox{FVector{-400.0, -400.0, -10.0}, FVector{1200.0, 1200.0, 0.0}}}};

    auto Config = FCk_GroundNav_BakeConfig{25.0f, 10.0f};
    Config.Set_TileSizeUu(400.0f);

    auto Profile = FCk_GroundNav_AgentProfile{
        FCk_AnyShape{FCk_ShapeCapsule_Dimensions{70.0f, 20.0f}}};
    Profile.Set_LedgeSensitivity(0.0f);

    auto Params = ck::groundnav::FCk_GroundNav_FieldParams{};
    Params._OriginXY = FVector2D::ZeroVector;
    Params._Divisions = FIntPoint{2, 2};
    Params._MinZUu = -50.0f;
    Params._MaxZUu = 300.0f;
    Params._Config = Config;
    Params._Profile = Profile;
    Params._MaxClearanceUu = 100.0f;

    auto Field = TSharedPtr<ck::groundnav::FCk_GroundNav_Field>{
        MakeShared<ck::groundnav::FCk_GroundNav_Field>()};

    if (NOT ck::groundnav::DoBake_Field(
            Backend, Params, ck::groundnav::FCk_GroundNav_Epoch{4}, *Field).Get_IsCompleted())
    { return {}; }

    return Field;
}

// The cap the collector applies. Plates and boundary runs are what the overlay draws, so the per-cell
// list is capped and the counts stay exact either way.
constexpr auto SnapshotMaxCells = 4096;

auto
MakeKey() -> ck::groundnav::FCk_GroundNav_DebugSnapshotCacheKey
{
    auto Key = ck::groundnav::FCk_GroundNav_DebugSnapshotCacheKey{};
    Key._WorldName = FName{TEXT("CkCrowdDebuggerGroundNav")};
    Key._VolumeEntityNumber = 12;
    Key._VolumeEntityVersion = 1;
    Key._NewestTileEpoch = 4;
    Key._SurfaceRevision = 9;
    return Key;
}

auto
MakeSceneSnapshot(uint64 InRevision) -> FCkCrowdDebugger_3dSceneSnapshot
{
    auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
    Snapshot._WorldEpoch = 23;
    Snapshot._GroundNav._Revision = InRevision;
    Snapshot._GroundNav._Field._Source = ECkCrowdDebugger_SnapshotSource::LivePie;
    Snapshot._GroundNav._Field._Key = MakeKey();
    Snapshot._GroundNav._Field._Snapshot =
        MakeShared<const ck::groundnav::FCk_GroundNav_DebugSnapshot>(MakeFieldSnapshot());
    return Snapshot;
}

auto
MakeComparison(FName InQueryId, bool InAgrees) -> ck::groundnav::FCk_GroundNav_ShadowComparison
{
    auto Comparison = ck::groundnav::FCk_GroundNav_ShadowComparison{};
    Comparison._QueryId = InQueryId;
    Comparison._RecastStatus = ECk_Nav_PathStatus::Ready;
    Comparison._RecastWaypointCount = 4;
    Comparison._RecastLengthUu = 1000.0;
    Comparison._RecastEndpoint = FVector{500.0, 0.0, 0.0};
    // Unbuilt is a failure on the GroundNav side, so an agreeing pair is two successes and a
    // diverging pair is one success against ground nobody has baked.
    Comparison._GroundNavStatus = InAgrees
        ? ECk_GroundNav_PathStatus::Ready
        : ECk_GroundNav_PathStatus::Unbuilt;
    Comparison._GroundNavWaypointCount = 4;
    Comparison._GroundNavLengthUu = 1010.0;
    Comparison._GroundNavEndpoint = FVector{500.0, 0.0, 0.0};
    return Comparison;
}
} // namespace ck_crowd_debugger_groundnav_spec

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerGroundNav_FieldCopyOutlivesProducer,
                                 "Ck.CrowdDebugger.Viewport3d.GroundNavFieldCopyOutlivesProducer",
                                 ck_crowd_debugger_groundnav_spec::TestFlags)

auto
    FCkCrowdDebuggerGroundNav_FieldCopyOutlivesProducer::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_groundnav_spec;
    // A field's worth of outlines and runs, so the target is sized for one.
    auto Fixture = FScopedTarget{4096, 200000};

    auto Field = Bake_FixtureField();

    if (NOT Field.IsValid())
    {
        AddError(TEXT("the fixture field did not bake, so there is no producer to outlive"));
        return false;
    }

    auto Capture = ck::groundnav::Make_DebugSnapshotFromField(*Field, SnapshotMaxCells);

    TestEqual(TEXT("the bake ran to the end"),
              static_cast<int32>(Capture._Status),
              static_cast<int32>(ck::groundnav::EDebugSnapshotStatus::Current));

    const auto CapturedPlateCount = Capture.Get_PlateCount();
    const auto CapturedBoundaryCount = Capture.Get_BoundaryCount();

    TestTrue(TEXT("the bake found plates to outline"), CapturedPlateCount > 0);
    TestTrue(TEXT("the bake found boundary runs to draw"), CapturedBoundaryCount > 0);

    auto Snapshot = FCkCrowdDebugger_3dSceneSnapshot{};
    Snapshot._WorldEpoch = 23;
    Snapshot._GroundNav._Revision = 1;
    Snapshot._GroundNav._Field._Key = MakeKey();
    Snapshot._GroundNav._Field._Snapshot =
        MakeShared<const ck::groundnav::FCk_GroundNav_DebugSnapshot>(MoveTemp(Capture));

    // The PRODUCER goes away here: the last reference to the baked field is dropped before anything is
    // drawn, so what reconciles below is the copy and nothing else. A view rather than a copy would
    // read freed tiles from this point on.
    Field.Reset();

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    TestTrue(TEXT("a copied field reconciles after the field it was baked from is gone"),
             Adapter.Reconcile(Snapshot, *Fixture._Target));

    const auto DrawnPlateCount = Adapter.Get_GroundNavPlateCount();
    const auto DrawnBoundaryCount = Adapter.Get_GroundNavBoundaryCount();

    TestTrue(TEXT("the plates the copy carries are still outlined"), DrawnPlateCount > 0);
    TestTrue(TEXT("no plate is invented that the capture does not carry"),
             DrawnPlateCount <= CapturedPlateCount);
    TestTrue(TEXT("the boundary runs the copy carries are still drawn"), DrawnBoundaryCount > 0);
    TestTrue(TEXT("no boundary run is invented that the capture does not carry"),
             DrawnBoundaryCount <= CapturedBoundaryCount);
    TestEqual(TEXT("plate outlines and boundary runs share the GroundNav role"),
              Adapter.Get_ItemCount(ECkCrowdDebugger_3dSceneRole::GroundNavField),
              DrawnPlateCount + DrawnBoundaryCount);

    // Failure is a status, not an empty field: a bake that is not drawable submits no geometry, and
    // the copy's own _Status is what says so. Drawing its partial arrays would report ground the bake
    // never vouched for.
    auto FailedCapture = ck::groundnav::FCk_GroundNav_DebugSnapshot{};
    FailedCapture._Status = ck::groundnav::EDebugSnapshotStatus::Failed;

    auto Failed = Snapshot;
    Failed._GroundNav._Revision = 2;
    Failed._GroundNav._Field._Snapshot =
        MakeShared<const ck::groundnav::FCk_GroundNav_DebugSnapshot>(MoveTemp(FailedCapture));

    TestTrue(TEXT("a failed bake still reconciles"), Adapter.Reconcile(Failed, *Fixture._Target));
    TestFalse(TEXT("a bake that is not drawable leaves no GroundNav geometry behind"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::GroundNavField));
    TestEqual(TEXT("the status the copy carries is the one the panel reads"),
              static_cast<int32>(Failed._GroundNav._Field._Snapshot->_Status),
              static_cast<int32>(ck::groundnav::EDebugSnapshotStatus::Failed));

    // No snapshot at all is a third answer, and the one a provider that is not GroundNav produces: no
    // capture was taken, so there is nothing to draw and nothing to report a status about.
    auto Absent = Snapshot;
    Absent._GroundNav._Revision = 3;
    Absent._GroundNav._Field._Snapshot = nullptr;

    TestTrue(TEXT("a copy holding no capture at all still reconciles"),
             Adapter.Reconcile(Absent, *Fixture._Target));
    TestFalse(TEXT("and leaves no GroundNav geometry behind"),
              Adapter.Has_Role(ECkCrowdDebugger_3dSceneRole::GroundNavField));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerGroundNav_FieldStableIdentitySurvivesReorder,
                                 "Ck.CrowdDebugger.Viewport3d.GroundNavFieldStableIdentitySurvivesReorder",
                                 ck_crowd_debugger_groundnav_spec::TestFlags)

auto
    FCkCrowdDebuggerGroundNav_FieldStableIdentitySurvivesReorder::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_groundnav_spec;
    auto Fixture = FScopedTarget{};

    const auto First = MakeSceneSnapshot(1);
    auto Reordered = First;
    {
        // The held capture is immutable, exactly as the cache publishes it, so the reorder is a NEW
        // capture rather than an edit of the one already on screen - which is what a rebake is.
        auto Shuffled = *First._GroundNav._Field._Snapshot;
        Swap(Shuffled._Plates[0], Shuffled._Plates[2]);
        Swap(Shuffled._Boundary[0], Shuffled._Boundary[1]);
        Reordered._GroundNav._Field._Snapshot =
            MakeShared<const ck::groundnav::FCk_GroundNav_DebugSnapshot>(MoveTemp(Shuffled));
    }
    // A rebake stamps a new revision even when only the ORDER moved, so the reorder is checked
    // through the rebuild path. The revision-reuse path would pass this without proving anything.
    Reordered._GroundNav._Revision = 2;

    auto Adapter = FCkCrowdDebugger_3dSceneAdapter{};
    Adapter.Reconcile(First, *Fixture._Target);
    Fixture._Target->Reset_FrameStats();
    Adapter.Reconcile(Reordered, *Fixture._Target);

    const auto& Stats = Fixture._Target->Get_Stats();
    TestEqual(TEXT("reordering the plate and boundary arrays adds no target instances"),
              Stats.Get_InstancesAdded(), 0);
    TestEqual(TEXT("reordering the plate and boundary arrays removes no target instances"),
              Stats.Get_InstancesRemoved(), 0);
    TestEqual(TEXT("the same plates are still on screen after the reorder"),
              Adapter.Get_GroundNavPlateCount(), PlateCount);
    TestEqual(TEXT("the same boundary runs are still on screen after the reorder"),
              Adapter.Get_GroundNavBoundaryCount(), BoundaryCount);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerShadowParity_RowsReadTheCopiedDiagnostics,
                                 "Ck.CrowdDebugger.ShadowParity.RowsReadTheCopiedDiagnostics",
                                 ck_crowd_debugger_groundnav_spec::TestFlags)

auto
    FCkCrowdDebuggerShadowParity_RowsReadTheCopiedDiagnostics::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_crowd_debugger_groundnav_spec;
    using FPanel = SCkCrowdDebugger_ShadowParityPanel;

    const auto FixtureKey = FName{TEXT("SpecFixture")};
    auto Producer = ck::FFragment_GroundNav_ShadowDiagnostics{};
    ck::groundnav::shadow::Accumulate(Producer, MakeComparison(FName{TEXT("Agreed")}, true), FixtureKey);
    ck::groundnav::shadow::Accumulate(Producer, MakeComparison(FName{TEXT("Diverged")}, false), FixtureKey);

    auto Parity = FCkCrowdDebugger_ShadowParity{};
    Parity._Sampled = true;
    Parity._Diagnostics = Producer;
    Producer = {};

    TestEqual(TEXT("comparisons landing with no fixture open report none open, not none recorded"),
              FPanel::Format_ActiveFixtureText(Parity).ToString(), FString{TEXT("(none open)")});
    TestEqual(TEXT("the fallback key still opened a row"),
              FPanel::Format_FixtureCountText(Parity).ToString(), FString{TEXT("1")});
    TestEqual(TEXT("one of two comparisons agreed"),
              FPanel::Format_AgreementText(Parity).ToString(), FString{TEXT("1 / 2")});
    TestTrue(TEXT("a run with a disagreement is not reported as clean"),
             FPanel::Resolve_AgreementColor(Parity) != CkStyle::Ok());
    TestTrue(TEXT("the diverging query is named, not just counted"),
             FPanel::Format_DivergingIdsText(Parity).ToString().Contains(TEXT("Diverged")));
    TestFalse(TEXT("the agreeing query is not named as diverging"),
              FPanel::Format_DivergingIdsText(Parity).ToString().Contains(TEXT("Agreed")));

    const auto FixtureRows = FPanel::Format_FixtureRowsText(Parity).ToString();
    TestTrue(TEXT("the per-fixture breakdown names the fixture the comparisons bucketed under"),
             FixtureRows.Contains(FixtureKey.ToString()));
    TestTrue(TEXT("the per-fixture breakdown carries that fixture's own agreement count"),
             FixtureRows.Contains(TEXT("1/2 agree")));
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkCrowdDebuggerShadowParity_UnsampledRowsSayNoDiagnostics,
                                 "Ck.CrowdDebugger.ShadowParity.UnsampledRowsSayNoDiagnostics",
                                 ck_crowd_debugger_groundnav_spec::TestFlags)

auto
    FCkCrowdDebuggerShadowParity_UnsampledRowsSayNoDiagnostics::
    RunTest(const FString&)
    -> bool
{
    using FPanel = SCkCrowdDebugger_ShadowParityPanel;

    // A world that was never read and a world holding an empty fragment are different answers, and
    // the unsampled value is the one that must not read as a clean run.
    const auto Unsampled = FCkCrowdDebugger_ShadowParity{};
    TestEqual(TEXT("an unread world names no fixture"),
              FPanel::Format_ActiveFixtureText(Unsampled).ToString(), FString{TEXT("(no shadow diagnostics)")});
    TestEqual(TEXT("an unread world reports no agreement"),
              FPanel::Format_AgreementText(Unsampled).ToString(), FString{TEXT("(no shadow diagnostics)")});
    TestTrue(TEXT("an unread world does not read as a clean run"),
             FPanel::Resolve_AgreementColor(Unsampled) == CkStyle::TextMute());
    TestTrue(TEXT("an unread world does not claim zero diverging ids"),
             FPanel::Resolve_DivergingIdsColor(Unsampled) == CkStyle::TextMute());
    TestTrue(TEXT("an unread world has no per-fixture breakdown to show"),
             FPanel::Format_FixtureRowsText(Unsampled).IsEmpty());

    auto Sampled = FCkCrowdDebugger_ShadowParity{};
    Sampled._Sampled = true;
    TestEqual(TEXT("a world read with nothing compared says so rather than reporting 0/0"),
              FPanel::Format_AgreementText(Sampled).ToString(), FString{TEXT("(nothing compared yet)")});
    TestEqual(TEXT("a world read with no divergences reports none"),
              FPanel::Format_DivergingIdsText(Sampled).ToString(), FString{TEXT("None")});
    return true;
}

#endif
