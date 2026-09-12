#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_PathNetworkFollower.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkPathNetwork/Network/CkPathNetwork_Fragment.h"
#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"

namespace ck_inspector_pathnetworkfollower_authored_test
{
    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        {
            return StaticCastSharedRef<SButton>(InRoot);
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SButton> Found =
                FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid())
            {
                return Found;
            }
        }
        return nullptr;
    }
} // namespace ck_inspector_pathnetworkfollower_authored_test

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorPathNetworkFollowerAuthored,
                                 "Ck.UiAuthoring.EcsDebugger.PathNetworkFollowerInspector.AuthoredComposition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorPathNetworkFollowerAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_pathnetworkfollower_authored_test;
    auto World = ck::FEcsWorld{};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityC = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture creates three distinct follower entities"),
                     ck::IsValid(EntityA) && ck::IsValid(EntityB) && ck::IsValid(EntityC) && EntityA != EntityB &&
                         EntityA != EntityC && EntityB != EntityC))
    {
        return false;
    }
    const auto AuthoritySettings = FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority};
    UCk_Utils_Net_UE::Add(EntityA, AuthoritySettings);
    UCk_Utils_Net_UE::Add(EntityB, AuthoritySettings);
    UCk_Utils_Net_UE::Add(EntityC, AuthoritySettings);
    auto FollowerA = UCk_Utils_PathNetworkFollower_UE::Add(EntityA, FCk_Fragment_PathNetworkFollower_ParamsData{});
    auto FollowerB = UCk_Utils_PathNetworkFollower_UE::Add(EntityB, FCk_Fragment_PathNetworkFollower_ParamsData{});
    auto FollowerC = UCk_Utils_PathNetworkFollower_UE::Add(EntityC, FCk_Fragment_PathNetworkFollower_ParamsData{});
    if (NOT TestTrue(TEXT("public Add creates three inspectable followers"),
                     ck::IsValid(FollowerA) && ck::IsValid(FollowerB) && ck::IsValid(FollowerC) &&
                         UCk_Utils_PathNetworkFollower_UE::Has(EntityA) &&
                         UCk_Utils_PathNetworkFollower_UE::Has(EntityB) &&
                         UCk_Utils_PathNetworkFollower_UE::Has(EntityC)))
    {
        return false;
    }
    UCk_Utils_PathNetworkFollower_UE::Request_FindRoute(
        FollowerB, FCk_Request_PathNetworkFollower_FindRoute{FVector{120.0f, 240.0f, 360.0f}}, {});
    if (NOT TestTrue(TEXT("public authority route request leaves B Pending without a pump"),
                     UCk_Utils_PathNetworkFollower_UE::Get_RouteStatus(FollowerB) ==
                         ECk_PathNetwork_RouteStatus::Pending))
    {
        return false;
    }
    auto Inspector = FCkInspector_PathNetworkFollower{};
    const auto RenderedA = Inspector.Build_Inspector(EntityA);
    const auto RenderedB = Inspector.Build_Inspector(EntityB);
    const auto RenderedC = Inspector.Build_Inspector(EntityC);
    if (NOT TestEqual(TEXT("first build mounts authored follower view"), RenderedA->GetTypeAsString(),
                      FString{TEXT("SCkInspector_PathNetworkFollowerAuthored")}) ||
        NOT TestEqual(TEXT("second build mounts authored follower view"), RenderedB->GetTypeAsString(),
                      FString{TEXT("SCkInspector_PathNetworkFollowerAuthored")}) ||
        NOT TestEqual(TEXT("third build mounts authored follower view"), RenderedC->GetTypeAsString(),
                      FString{TEXT("SCkInspector_PathNetworkFollowerAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const auto AuthoredA = StaticCastSharedRef<SCkInspector_PathNetworkFollowerAuthored>(RenderedA);
    const auto AuthoredB = StaticCastSharedRef<SCkInspector_PathNetworkFollowerAuthored>(RenderedB);
    const auto AuthoredC = StaticCastSharedRef<SCkInspector_PathNetworkFollowerAuthored>(RenderedC);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiView> ViewC = AuthoredC->Get_View();
    TestTrue(TEXT("each build retains an independent accepted view"),
             AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && AuthoredC->Is_Mounted() && ViewA.IsValid() &&
                 ViewB.IsValid() && ViewC.IsValid() && ViewA != ViewB && ViewA != ViewC && ViewB != ViewC &&
                 ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded &&
                 ViewC->GetLastResult().Succeeded);
    TestTrue(TEXT("A remains None while B projects public Pending state and nonzero goal"),
             AuthoredA->Get_RouteStatusText().Contains(TEXT("None")) &&
                 AuthoredB->Get_RouteStatusText().Contains(TEXT("Pending")) &&
                 AuthoredB->Get_GoalXText() == TEXT("120.000") && AuthoredB->Get_GoalYText() == TEXT("240.000") &&
                 AuthoredB->Get_GoalZText() == TEXT("360.000") &&
                 AuthoredA->Get_RouteStatusForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral) &&
                 AuthoredB->Get_RouteStatusForeground() == CkStyle::GetToneColor(ECk_Tone::Info) &&
                 AuthoredA->Get_FailReasonForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral) &&
                 AuthoredB->Get_FailReasonForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral));
    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Inspector.Build_Inspector(EntityA);
        RowsA = Capture.Get_Rows();
    }
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Inspector.Build_Inspector(EntityB);
        RowsB = Capture.Get_Rows();
    }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native fallback retains all seven rows and exact A/B differences"),
             RowsA.Num() == 7 && RowsB.Num() == 7 &&
                 RowsA.FindRef(TEXT("Route Status:")) != RowsB.FindRef(TEXT("Route Status:")) &&
                 RowsA.FindRef(TEXT("Fail Reason:")) == RowsB.FindRef(TEXT("Fail Reason:")) &&
                 RowsA.FindRef(TEXT("Goal:")) != RowsB.FindRef(TEXT("Goal:")) &&
                 RowsA.FindRef(TEXT("Legs:")) == RowsB.FindRef(TEXT("Legs:")) &&
                 RowsA.FindRef(TEXT("Waypoints:")) == RowsB.FindRef(TEXT("Waypoints:")) &&
                 RowsA.FindRef(TEXT("Total Cost:")) == RowsB.FindRef(TEXT("Total Cost:")) &&
                 RowsA.FindRef(TEXT("Feature:")) == RowsB.FindRef(TEXT("Feature:")) && Differing.Num() == 2 &&
                 Differing.Contains(TEXT("Route Status:")) && Differing.Contains(TEXT("Goal:")));
    TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored =
            StaticCastSharedRef<SCkInspector_PathNetworkFollowerAuthored>(Inspector.Build_Inspector(EntityB));
    }
    TestTrue(TEXT("authored labels receive the exact native diff set"),
             DiffAuthored.IsValid() && DiffAuthored->Is_RouteStatusDiffMarked() && DiffAuthored->Is_GoalDiffMarked() &&
                 NOT DiffAuthored->Is_FailReasonDiffMarked() && NOT DiffAuthored->Is_LegsDiffMarked() &&
                 NOT DiffAuthored->Is_WaypointsDiffMarked() && NOT DiffAuthored->Is_TotalCostDiffMarked() &&
                 NOT DiffAuthored->Is_FeatureDiffMarked());
    const TSharedRef<SCkInspector_PathNetworkFollowerAuthored> Invalid =
        SNew(SCkInspector_PathNetworkFollowerAuthored).Entity(FCk_Handle{});
    TestTrue(TEXT("invalid mount remains safe and inert dispatch is disabled"),
             Invalid->Is_Mounted() && Invalid->Get_RouteStatusText() == TEXT("--") && NOT Invalid->Get_IsAvailable());
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Css;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed follower resources are readable"),
                     Plugin.IsValid() &&
                         FFileHelper::LoadFileToString(
                             Markup, *FPaths::Combine(Root, TEXT("EcsInspectorPathNetworkFollower.ui.html"))) &&
                         FFileHelper::LoadFileToString(
                             Css, *FPaths::Combine(Root, TEXT("EcsInspectorPathNetworkFollower.ui.css")))))
    {
        return false;
    }
    TestTrue(TEXT("authored markup owns all route fields and direct remove action"),
             Markup.Contains(TEXT("Route Status:")) && Markup.Contains(TEXT("Fail Reason:")) &&
                 Markup.Contains(TEXT("Goal:")) && Markup.Contains(TEXT("Legs:")) &&
                 Markup.Contains(TEXT("Waypoints:")) && Markup.Contains(TEXT("Total Cost:")) &&
                 Markup.Contains(TEXT("<debug-inspector-action")) &&
                 Markup.Contains(TEXT("action=\"path-network-follower-remove\"")));
    const TSharedPtr<SButton> HeldRemoveB = FindButton(RenderedB, TEXT("path-network-follower-remove"));
    if (NOT TestTrue(TEXT("authored B mounts a physical retained Remove action"),
                     HeldRemoveB.IsValid() && HeldRemoveB->IsEnabled()))
    {
        return false;
    }
    const TSharedPtr<SButton> HeldRemoveA = FindButton(RenderedA, TEXT("path-network-follower-remove"));
    const int64 Revision = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted"),
             ViewB->TryReload(Markup, Css, TEXT("follower compatible candidate")).Succeeded);
    TestTrue(TEXT("accepted reload preserves retained button identity"),
             AuthoredB->Get_View() == ViewB && ViewB->GetRevision() > Revision &&
                 FindButton(RenderedB, TEXT("path-network-follower-remove")) == HeldRemoveB);
    const TSharedRef<SWidget> Before = ViewB->GetRegion(TEXT("main"));
    const int64 BeforeRevision = ViewB->GetRevision();
    const FString MissingActionMarkup =
        Markup.Replace(TEXT("action=\"path-network-follower-remove\""), TEXT("action=\"missing-action\""));
    TestFalse(TEXT("missing Remove action binding is rejected atomically"),
              ViewB->TryReload(MissingActionMarkup, Css, TEXT("follower missing action candidate")).Succeeded);
    TestTrue(TEXT("rejected action reload retains B tree and revision"),
             &ViewB->GetRegion(TEXT("main")).Get() == &Before.Get() && ViewB->GetRevision() == BeforeRevision);
    const FString MissingStatusMarkup =
        Markup.Replace(TEXT("background-bind=\"path-network-follower-route-status-background\""),
                       TEXT("background-bind=\"missing-route-status-background\""));
    TestFalse(TEXT("missing Route Status binding is rejected atomically"),
              ViewB->TryReload(MissingStatusMarkup, Css, TEXT("follower missing status candidate")).Succeeded);
    TestTrue(TEXT("rejected status reload retains B tree and revision"),
             &ViewB->GetRegion(TEXT("main")).Get() == &Before.Get() && ViewB->GetRevision() == BeforeRevision);
    HeldRemoveB->SimulateClick();
    TestTrue(TEXT("physical Remove strips only B feature while its entity survives"),
             ck::IsValid(EntityB) && NOT UCk_Utils_PathNetworkFollower_UE::Has(EntityB) &&
                 NOT Inspector.CanInspect(EntityB) && AuthoredB->Get_RouteStatusText() == TEXT("--"));
    HeldRemoveB->SimulateClick();
    TestFalse(TEXT("held repeated Remove is inert after feature removal"),
              UCk_Utils_PathNetworkFollower_UE::Has(EntityB));

    auto ParamsOnly = EntityA;
    ParamsOnly.Try_Remove<ck::FFragment_PathNetworkFollower_Corridor>();
    TestTrue(TEXT("Params-only teardown fails all authored reads and action dispatch closed"),
             UCk_Utils_PathNetworkFollower_UE::Has(ParamsOnly) && NOT Inspector.CanInspect(ParamsOnly) &&
                 AuthoredA->Get_RouteStatusText() == TEXT("--") && AuthoredA->Get_FailReasonText() == TEXT("--") &&
                 AuthoredA->Get_GoalXText() == TEXT("--") && AuthoredA->Get_LegsText() == TEXT("--") &&
                 AuthoredA->Get_WaypointsText() == TEXT("--") && AuthoredA->Get_TotalCostText() == TEXT("--") &&
                 AuthoredA->Get_RouteStatusForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral) &&
                 AuthoredA->Get_FailReasonForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral) &&
                 HeldRemoveA.IsValid());
    HeldRemoveA->SimulateClick();
    TestTrue(TEXT("held Remove cannot mutate the surviving Params-only composition"),
             UCk_Utils_PathNetworkFollower_UE::Has(EntityA));
    auto StaleRows = TMap<FString, FString>{};
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Inspector.Build_Inspector(EntityA);
        StaleRows = Capture.Get_Rows();
    }
    TestTrue(TEXT("native capture also fails Params-only reads closed"),
             StaleRows.FindRef(TEXT("Route Status:")) == TEXT("--") &&
                 StaleRows.FindRef(TEXT("Fail Reason:")) == TEXT("--") &&
                 StaleRows.FindRef(TEXT("Legs:")) == TEXT("0") && StaleRows.FindRef(TEXT("Waypoints:")) == TEXT("0") &&
                 StaleRows.FindRef(TEXT("Total Cost:")) == TEXT("--"));

    auto CorridorOnly = EntityC;
    CorridorOnly.Try_Remove<ck::FFragment_PathNetworkFollower_Params>();
    TestTrue(TEXT("Corridor-only teardown also fails retained state closed"),
             NOT UCk_Utils_PathNetworkFollower_UE::Has(CorridorOnly) && NOT Inspector.CanInspect(CorridorOnly) &&
                 AuthoredC->Get_RouteStatusText() == TEXT("--") && AuthoredC->Get_FailReasonText() == TEXT("--") &&
                 AuthoredC->Get_GoalXText() == TEXT("--") && NOT AuthoredC->Get_IsAvailable());

    TSharedPtr<SCkInspector_PathNetworkFollowerAuthored> DestructorAuthored;
    TWeakPtr<FCkUiView> DestructorView;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_PathNetworkFollower>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_PathNetworkFollowerAuthored>(
            DestructorInspector->Build_Inspector(FCk_Handle{}));
        DestructorView = DestructorAuthored->Get_View();
    }
    TestTrue(TEXT("inspector destruction releases its authored view"),
             DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorView.IsValid());

    const TWeakPtr<FCkUiView> ReleasedA = ViewA, ReleasedB = ViewB, ReleasedC = ViewC;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained view inert"),
             AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && AuthoredC->Is_Inert() && DiffAuthored->Is_Inert());
    ViewA.Reset();
    ViewB.Reset();
    ViewC.Reset();
    TestFalse(TEXT("deactivation releases every per-build view"),
              ReleasedA.IsValid() || ReleasedB.IsValid() || ReleasedC.IsValid());
    return true;
}
#endif
