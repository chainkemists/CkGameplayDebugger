#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_PathNetwork.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkPathNetwork/Network/CkPathNetwork_Processor.h"
#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ck_inspector_pathnetwork_authored_test
{
    auto MakeRibbon(const FVector& InStart, const FVector& InEnd) -> FCk_PathNetwork_Ribbon
    {
        return FCk_PathNetwork_Ribbon{{
            FCk_PathNetwork_RibbonPoint{InStart, 100.0f},
            FCk_PathNetwork_RibbonPoint{InEnd, 100.0f}}};
    }

    auto MakeParams(const TArray<FCk_PathNetwork_Ribbon>& InRibbons) -> FCk_Fragment_PathNetwork_ParamsData
    {
        return FCk_Fragment_PathNetwork_ParamsData{InRibbons};
    }

    auto BuildNetwork(ck::FProcessor_PathNetwork_Setup& InSetup, const FCk_Handle_PathNetwork& InNetwork) -> bool
    {
        auto MutableNetwork = InNetwork;
        if (ck::Is_NOT_Valid(MutableNetwork)
            || NOT MutableNetwork.Has<ck::FFragment_PathNetwork_Params>()
            || NOT MutableNetwork.Has<ck::FFragment_PathNetwork_Graph>())
        { return false; }
        InSetup.ForEachEntity(
            FCk_Time::ZeroSecond(), MutableNetwork,
            MutableNetwork.Get<ck::FFragment_PathNetwork_Params>(),
            MutableNetwork.Get<ck::FFragment_PathNetwork_Graph>());
        return UCk_Utils_PathNetwork_UE::Get_IsBuilt(MutableNetwork);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorPathNetworkAuthored,
    "Ck.UiAuthoring.EcsDebugger.PathNetworkInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorPathNetworkAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_pathnetwork_authored_test;

    auto World = ck::FEcsWorld{};
    auto Setup = ck::FProcessor_PathNetwork_Setup{World.Get_Registry()};
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture creates distinct Path Network owners"), ck::IsValid(OwnerA) && ck::IsValid(OwnerB) && OwnerA != OwnerB))
    { return false; }

    auto NetworkA = UCk_Utils_PathNetwork_UE::Add(OwnerA, MakeParams({MakeRibbon(FVector::ZeroVector, FVector{500.0f, 0.0f, 0.0f})}));
    auto NetworkB = UCk_Utils_PathNetwork_UE::Add(OwnerB, MakeParams({
        MakeRibbon(FVector::ZeroVector, FVector{500.0f, 0.0f, 0.0f}),
        MakeRibbon(FVector{500.0f, 0.0f, 0.0f}, FVector{1000.0f, 0.0f, 0.0f})}));
    if (NOT TestTrue(TEXT("public Path Network Add creates distinct inspectable networks"),
        ck::IsValid(NetworkA) && ck::IsValid(NetworkB) && NetworkA != NetworkB
            && UCk_Utils_PathNetwork_UE::Has(NetworkA) && UCk_Utils_PathNetwork_UE::Has(NetworkB)))
    { return false; }
    if (NOT TestTrue(TEXT("production setup builds the authored path while its peer remains pending"),
        BuildNetwork(Setup, NetworkA) && NOT UCk_Utils_PathNetwork_UE::Get_IsBuilt(NetworkB)))
    { return false; }

    auto Inspector = FCkInspector_PathNetwork{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(NetworkA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(NetworkB);
    if (NOT TestEqual(TEXT("first build returns authored Path Network widget"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_PathNetworkAuthored")})
        || NOT TestEqual(TEXT("second build returns authored Path Network widget"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_PathNetworkAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_PathNetworkAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_PathNetworkAuthored>(RenderedA);
    const TSharedRef<SCkInspector_PathNetworkAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_PathNetworkAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted Path Network view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false; }
    TestTrue(TEXT("authored values retain live built/count/epoch semantics and tones"),
        AuthoredA->Get_BuiltText() == TEXT("Yes") && AuthoredA->Get_NodesText() != TEXT("0")
            && AuthoredA->Get_EdgesText() != TEXT("0") && AuthoredA->Get_BuildEpochText() == TEXT("1")
            && AuthoredA->Get_BuiltForeground() != AuthoredB->Get_BuiltForeground()
            && AuthoredA->Get_BuildEpochForeground() != AuthoredB->Get_BuildEpochForeground());

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(NetworkA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(NetworkB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native builder remains multi-selection authority for all four Path Network rows"),
        RowsA.FindRef(TEXT("Built:")) != RowsB.FindRef(TEXT("Built:"))
            && RowsA.FindRef(TEXT("Nodes:")) != RowsB.FindRef(TEXT("Nodes:"))
            && RowsA.FindRef(TEXT("Edges:")) != RowsB.FindRef(TEXT("Edges:"))
            && RowsA.FindRef(TEXT("Build Epoch:")) != RowsB.FindRef(TEXT("Build Epoch:"))
            && Differing.Num() == 4 && Differing.Contains(TEXT("Built:")) && Differing.Contains(TEXT("Nodes:"))
            && Differing.Contains(TEXT("Edges:")) && Differing.Contains(TEXT("Build Epoch:")));

    TSharedPtr<SCkInspector_PathNetworkAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_PathNetworkAuthored>(Inspector.Build_Inspector(NetworkA));
    }
    TestTrue(TEXT("all four authored Path Network labels receive exact diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_BuiltDiffMarked() && DiffAuthored->Is_NodesDiffMarked()
        && DiffAuthored->Is_EdgesDiffMarked() && DiffAuthored->Is_BuildEpochDiffMarked());

    const TSharedRef<SCkInspector_PathNetworkAuthored> Invalid = SNew(SCkInspector_PathNetworkAuthored).Entity(FCk_Handle{});
    TestTrue(TEXT("invalid Path Network entity stays mounted with safe placeholders"), Invalid->Is_Mounted()
        && Invalid->Get_BuiltText() == TEXT("--") && Invalid->Get_NodesText() == TEXT("--")
        && Invalid->Get_EdgesText() == TEXT("--") && Invalid->Get_BuildEpochText() == TEXT("--"));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Path Network resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetwork.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPathNetwork.ui.css"))))) { return false; }
    TestTrue(TEXT("resource preserves native status/count/numeric presentation and semantic tone bindings"),
        Markup.Contains(TEXT(">Built:</text>")) && Markup.Contains(TEXT(">Nodes:</text>"))
            && Markup.Contains(TEXT(">Edges:</text>")) && Markup.Contains(TEXT(">Build Epoch:</text>"))
            && Markup.Contains(TEXT("show-dot=\"true\"")) && Markup.Contains(TEXT("path-network-nodes-value\" label-bind=\"path-network-nodes\" foreground-bind=\"path-network-nodes-foreground\" background-bind=\"path-network-nodes-background\" show-dot=\"false\""))
            && Markup.Contains(TEXT("path-network-edges-value\" label-bind=\"path-network-edges\" foreground-bind=\"path-network-edges-foreground\" background-bind=\"path-network-edges-background\" show-dot=\"false\""))
            && Markup.Contains(TEXT("<text id=\"path-network-build-epoch-value\" bind=\"path-network-build-epoch\" color-bind=\"path-network-build-epoch-foreground\""))
            && NOT Markup.Contains(TEXT("path-network-build-epoch-background")) && Markup.Contains(TEXT("path-network-built-foreground"))
            && Markup.Contains(TEXT("path-network-nodes-background")) && Markup.Contains(TEXT("path-network-edges-foreground"))
            && Markup.Contains(TEXT("path-network-build-epoch-foreground")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by Path Network A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Path Network A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing Path Network status binding is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><debug-status id=\"status\" label-bind=\"path-network-built\" foreground-bind=\"path-network-built-foreground\" background-bind=\"missing-background\" /></region></ui>"),
        TEXT(""), TEXT("Path Network B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    auto StaleNetworkA = NetworkA;
    StaleNetworkA.Try_Remove<ck::FFragment_PathNetwork_Graph>();
    TestTrue(TEXT("mounted authored reads fail closed after the Path Network graph is removed"),
        ck::IsValid(StaleNetworkA) && AuthoredA->Get_BuiltText() == TEXT("--")
            && AuthoredA->Get_NodesText() == TEXT("--") && AuthoredA->Get_EdgesText() == TEXT("--")
            && AuthoredA->Get_BuildEpochText() == TEXT("--")
            && AuthoredA->Get_BuiltForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral));
    auto StaleRows = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(NetworkA); StaleRows = Capture.Get_Rows(); }
    TestTrue(TEXT("native captured reads fail closed after graph removal without utility getter access"),
        StaleRows.FindRef(TEXT("Built:")) == TEXT("--") && StaleRows.FindRef(TEXT("Nodes:")) == TEXT("0")
            && StaleRows.FindRef(TEXT("Edges:")) == TEXT("0") && StaleRows.FindRef(TEXT("Build Epoch:")) == TEXT("--"));

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained Path Network view inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && AuthoredA->Get_BuiltText().IsEmpty() && AuthoredA->Get_NodesText().IsEmpty());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build Path Network view"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
