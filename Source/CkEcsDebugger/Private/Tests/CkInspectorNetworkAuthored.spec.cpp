#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Network.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorNetworkAuthored,
    "Ck.UiAuthoring.EcsDebugger.NetworkInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorNetworkAuthored::RunTest(const FString&) -> bool
{
    auto World = ck::FEcsWorld{};
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    TestTrue(TEXT("fixture creates distinct network entities"), ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB);
    if (NOT ck::IsValid(EntityA) || NOT ck::IsValid(EntityB)) { return false; }
    UCk_Utils_Net_UE::Add(EntityA, FCk_Net_ConnectionSettings{
        ECk_Replication::Replicates, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});
    UCk_Utils_Net_UE::Add(EntityB, FCk_Net_ConnectionSettings{
        ECk_Replication::Replicates, ECk_Net_NetModeType::Client, ECk_Net_EntityNetRole::Proxy});

    auto Inspector = FCkInspector_Network{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first build returns authored Network widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_NetworkAuthored")})
        || NOT TestEqual(TEXT("second build returns authored Network widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_NetworkAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }

    const TSharedRef<SCkInspector_NetworkAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_NetworkAuthored>(RenderedA);
    const TSharedRef<SCkInspector_NetworkAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_NetworkAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted authored view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    {
        AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false;
    }
    TestTrue(TEXT("Network values are live and retain their status-pill tone ownership"),
        AuthoredA->Get_NetModeText().Contains(TEXT("Host")) && AuthoredA->Get_NetRoleText().Contains(TEXT("Authority"))
            && AuthoredA->Get_NetModeForeground() != AuthoredB->Get_NetModeForeground()
            && AuthoredA->Get_NetRoleForeground() != AuthoredB->Get_NetRoleForeground());

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture remains the authority for both Network rows"),
        RowsA.FindRef(TEXT("NetMode:")) != RowsB.FindRef(TEXT("NetMode:"))
            && RowsA.FindRef(TEXT("NetRole:")) != RowsB.FindRef(TEXT("NetRole:"))
            && Differing.Contains(TEXT("NetMode:")) && Differing.Contains(TEXT("NetRole:")));

    TSharedPtr<SCkInspector_NetworkAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_NetworkAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("both authored Network labels receive exact diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_NetModeDiffMarked() && DiffAuthored->Is_NetRoleDiffMarked());

    const TSharedRef<SCkInspector_NetworkAuthored> Invalid = SNew(SCkInspector_NetworkAuthored).Entity(FCk_Handle{});
    TestTrue(TEXT("invalid entity stays mounted with safe Network placeholders"), Invalid->Is_Mounted()
        && Invalid->Get_NetModeText() == TEXT("--") && Invalid->Get_NetRoleText() == TEXT("--"));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Network resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorNetwork.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorNetwork.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns exact stable labels, status pills, and tone bindings"),
        Markup.Contains(TEXT(">NetMode:</text>")) && Markup.Contains(TEXT(">NetRole:</text>"))
            && Markup.Contains(TEXT("<debug-status")) && Markup.Contains(TEXT("network-net-mode-foreground"))
            && Markup.Contains(TEXT("network-net-mode-background")) && Markup.Contains(TEXT("network-net-role-foreground"))
            && Markup.Contains(TEXT("network-net-role-background")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Network A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing Network status binding is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><debug-status id=\"status\" label-bind=\"network-net-mode\" foreground-bind=\"network-net-mode-foreground\" background-bind=\"missing-background\" /></region></ui>"),
        TEXT(""), TEXT("Network B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes all retained Network views inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && AuthoredA->Get_NetModeText().IsEmpty() && AuthoredA->Get_NetRoleText().IsEmpty());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build Network view"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
