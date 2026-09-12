#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Relationships.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkRelationship/Team/CkTeam_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"

namespace ck_inspector_relationships_authored_test
{
    auto CountType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> int32
    {
        int32 Count = InRoot->GetTypeAsString() == InType ? 1 : 0;
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { Count += CountType(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorRelationshipsAuthored,
    "Ck.UiAuthoring.EcsDebugger.RelationshipsInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorRelationshipsAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_relationships_authored_test;
    auto World = ck::FEcsWorld{};
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerA);
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerB);
    TestTrue(TEXT("fixture creates distinct relationship entities"), ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB);
    if (NOT ck::IsValid(EntityA) || NOT ck::IsValid(EntityB)) { return false; }

    UCk_Utils_ContextOwner_UE::Request_Override(EntityA, OwnerA, {});
    UCk_Utils_ContextOwner_UE::Request_Override(EntityB, OwnerB, {});
    auto TeamA = UCk_Utils_Team_UE::Add(EntityA, ECk_Team_ID::Two, ECk_Replication::DoesNotReplicate);
    auto TeamB = UCk_Utils_Team_UE::Add(EntityB, ECk_Team_ID::Four, ECk_Replication::DoesNotReplicate);
    TestTrue(TEXT("fixture assigns distinct teams"), ck::IsValid(TeamA) && ck::IsValid(TeamB));
    if (NOT ck::IsValid(TeamA) || NOT ck::IsValid(TeamB)) { return false; }

    auto Inspector = FCkInspector_Relationships{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first build returns the authored Relationships widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_RelationshipsAuthored")})
        || NOT TestEqual(TEXT("second build returns the authored Relationships widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_RelationshipsAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_RelationshipsAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_RelationshipsAuthored>(RenderedA);
    const TSharedRef<SCkInspector_RelationshipsAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_RelationshipsAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build receives an independent authored view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    {
        AddError(AuthoredA->Get_LoadError()); AddError(AuthoredB->Get_LoadError()); return false;
    }
    TestTrue(TEXT("each authored instance mounts two native entity reference ports"),
        CountType(RenderedA, TEXT("SCkDebug_EntityRef")) == 2 && CountType(RenderedB, TEXT("SCkDebug_EntityRef")) == 2);

    const FString TeamBefore = AuthoredA->Get_TeamText();
    UCk_Utils_Team_UE::Assign(TeamA, ECk_Team_ID::Six);
    TestTrue(TEXT("Team text stays live after Build_Inspector"),
        AuthoredA->Get_TeamText() != TeamBefore && AuthoredA->Get_TeamText().Contains(TEXT("Starts from ZERO")));
    TestEqual(TEXT("Team color is live while a Team fragment remains present"), AuthoredA->Get_TeamColor(), CkStyle::Relationship());
    UCk_Utils_Team_UE::Unassign(TeamA);
    TestTrue(TEXT("Team text and color update when the Team is unassigned"),
        AuthoredA->Get_TeamText().Contains(TEXT("Unassigned")) && AuthoredA->Get_TeamColor() == CkStyle::Relationship());

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native capture provides distinct owner values for multi-select diff"),
        RowsA.FindRef(TEXT("Context Owner:")) != RowsB.FindRef(TEXT("Context Owner:"))
            && RowsA.FindRef(TEXT("Lifetime Owner:")) != RowsB.FindRef(TEXT("Lifetime Owner:"))
            && Differing.Contains(TEXT("Context Owner:")) && Differing.Contains(TEXT("Lifetime Owner:")));

    TSharedPtr<SCkInspector_RelationshipsAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_RelationshipsAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored rows receive their matching diff marks"),
        DiffAuthored.IsValid() && DiffAuthored->Is_ContextOwnerDiffMarked() && DiffAuthored->Is_LifetimeOwnerDiffMarked());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed relationships resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorRelationships.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorRelationships.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns exact stable labels, ports, Team color, and diff bindings"),
        Markup.Contains(TEXT("relationships-team-label")) && Markup.Contains(TEXT(">Team:</text>"))
            && Markup.Contains(TEXT("relationships-context-owner-label")) && Markup.Contains(TEXT(">Context Owner:</text>"))
            && Markup.Contains(TEXT("relationships-lifetime-owner-label")) && Markup.Contains(TEXT(">Lifetime Owner:</text>"))
            && Markup.Contains(TEXT("relationships-context-owner-port")) && Markup.Contains(TEXT("relationships-lifetime-owner-port"))
            && Markup.Contains(TEXT("color-bind=\"relationships-team-color\"")) && Markup.Contains(TEXT("relationships-context-owner-diff-color")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by A"), ViewA->TryReload(Markup, Stylesheet, TEXT("Relationships A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing port is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"only-port\" bind=\"relationships-context-owner-port\" /></region></ui>"),
        TEXT(""), TEXT("Relationships B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes all retained build results inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build view"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
