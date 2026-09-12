#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Resolver.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"

// The Resolver fragment deliberately exposes read-only state. This focused presentation fixture owns
// synthetic state and must make two otherwise processor-free snapshots distinguishable.
#define private public
#include "ResolverDataBundle/CkResolverDataBundle_Fragment.h"
#undef private

#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ck_inspector_resolver_authored_test
{
    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        if (Children == nullptr) { return nullptr; }
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                    ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
                Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    struct FFixture final
    {
        FCk_Handle EntityA;
        FCk_Handle EntityB;
    };

    auto Make_Phases(const int32 InCount) -> TArray<FCk_Fragment_ResolverDataBundle_PhaseInfo>
    {
        auto Phases = TArray<FCk_Fragment_ResolverDataBundle_PhaseInfo>{};
        Phases.Reserve(InCount);
        for (int32 Index = 0; Index < InCount; ++Index)
        { Phases.Emplace(); }
        return Phases;
    }

    auto Create_Entity(
        ck::FEcsWorld& InWorld,
        const float InFinalValue,
        const int32 InPhaseIndex,
        const int32 InPhaseCount,
        const FGameplayTag InMetadataTag,
        const int32 InModifierOpCount,
        const int32 InMetadataOpCount)
        -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (ck::Is_NOT_Valid(Entity)) { return {}; }

        Entity.Add<ck::FFragment_ResolverDataBundle_Params>(
            FCk_Fragment_ResolverDataBundle_ParamsData{FGameplayTag{}, {}, {}, {}, Make_Phases(InPhaseCount)});
        auto& Current = Entity.Add<ck::FFragment_ResolverDataBundle_Current>();
        Current._FinalValue = InFinalValue;
        Current._CurrentPhaseIndex = InPhaseIndex;
        Current._MetadataTags.AddTag(InMetadataTag);

        if (InModifierOpCount > 0 || InMetadataOpCount > 0)
        {
            auto& Pending = Entity.Add<ck::FFragment_ResolverDataBundle_PendingOperations>();
            for (int32 Index = 0; Index < InModifierOpCount; ++Index)
            { Pending._PendingModifiersOperations.Emplace(); }
            for (int32 Index = 0; Index < InMetadataOpCount; ++Index)
            { Pending._PendingMetadataOperations.Emplace(); }
        }

        return Entity;
    }

    auto CreateFixture(
        ck::FEcsWorld& InWorld,
        const FGameplayTag InMetadataTagA,
        const FGameplayTag InMetadataTagB,
        FFixture& OutFixture)
        -> bool
    {
        OutFixture.EntityA = Create_Entity(InWorld, 12.5f, 0, 2, InMetadataTagA, 1, 2);
        OutFixture.EntityB = Create_Entity(InWorld, 83.25f, 2, 3, InMetadataTagB, 3, 1);
        return ck::IsValid(OutFixture.EntityA) && ck::IsValid(OutFixture.EntityB)
            && OutFixture.EntityA != OutFixture.EntityB;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorResolverAuthored,
    "Ck.UiAuthoring.EcsDebugger.ResolverInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorResolverAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_resolver_authored_test;

    auto InvalidInspector = FCkInspector_Resolver{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored Resolver shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_ResolverAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_ResolverAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_ResolverAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid authored values fail closed without fragment access"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_FinalValueText() == TEXT("--")
            && InvalidAuthored->Get_PhaseText() == TEXT("-- / 0")
            && InvalidAuthored->Get_MetadataTagsText() == TEXT("--")
            && InvalidAuthored->Get_ModifierOpsText() == TEXT("--")
            && InvalidAuthored->Get_MetadataOpsText() == TEXT("--"));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    const FGameplayTag MetadataTagA = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Attack")}, false);
    const FGameplayTag MetadataTagB = FGameplayTag::RequestGameplayTag(FName{TEXT("Combat.Defense")}, false);
    if (NOT TestTrue(TEXT("fixture reuses two registered runtime gameplay tags"),
        MetadataTagA.IsValid() && MetadataTagB.IsValid() && MetadataTagA != MetadataTagB))
    { return false; }

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(TEXT("fixture creates two distinct processor-free Resolver states"),
        CreateFixture(World, MetadataTagA, MetadataTagB, Fixture))) { return false; }

    auto Inspector = FCkInspector_Resolver{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.EntityB);
    if (NOT TestEqual(TEXT("first build returns authored Resolver widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_ResolverAuthored")})
        || NOT TestEqual(TEXT("second build returns authored Resolver widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_ResolverAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_ResolverAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_ResolverAuthored>(RenderedA);
    const TSharedRef<SCkInspector_ResolverAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_ResolverAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted Resolver view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && AuthoredA->Get_IsAvailable()
            && AuthoredB->Get_IsAvailable() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    TestTrue(TEXT("authored Resolver rows project every distinct state value"),
        AuthoredA->Get_FinalValueText() == TEXT("12.500") && AuthoredA->Get_PhaseText() == TEXT("1 / 2")
            && AuthoredA->Get_MetadataTagsText().Contains(TEXT("Combat.Attack"))
            && AuthoredA->Get_ModifierOpsText() == TEXT("1") && AuthoredA->Get_MetadataOpsText() == TEXT("2")
            && AuthoredB->Get_FinalValueText() == TEXT("83.250") && AuthoredB->Get_PhaseText() == TEXT("3 / 3")
            && AuthoredB->Get_MetadataTagsText().Contains(TEXT("Combat.Defense"))
            && AuthoredB->Get_ModifierOpsText() == TEXT("3") && AuthoredB->Get_MetadataOpsText() == TEXT("1"));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native builder remains multi-selection authority for every Resolver row"),
        RowsA.FindRef(TEXT("Final Value:")) == TEXT("12.500") && RowsB.FindRef(TEXT("Final Value:")) == TEXT("83.250")
            && RowsA.FindRef(TEXT("Phase:")) == TEXT("1 / 2") && RowsB.FindRef(TEXT("Phase:")) == TEXT("3 / 3")
            && RowsA.FindRef(TEXT("Metadata Tags:")).Contains(TEXT("Combat.Attack"))
            && RowsB.FindRef(TEXT("Metadata Tags:")).Contains(TEXT("Combat.Defense"))
            && RowsA.FindRef(TEXT("Modifier Ops:")) == TEXT("1") && RowsB.FindRef(TEXT("Modifier Ops:")) == TEXT("3")
            && RowsA.FindRef(TEXT("Metadata Ops:")) == TEXT("2") && RowsB.FindRef(TEXT("Metadata Ops:")) == TEXT("1")
            && Differing.Num() == 5 && Differing.Contains(TEXT("Final Value:")) && Differing.Contains(TEXT("Phase:"))
            && Differing.Contains(TEXT("Metadata Tags:")) && Differing.Contains(TEXT("Modifier Ops:"))
            && Differing.Contains(TEXT("Metadata Ops:")));

    TSharedPtr<SCkInspector_ResolverAuthored> DiffAuthored;
    { const auto DiffScope = FCkInspector_DiffMarkScope{&Differing}; DiffAuthored = StaticCastSharedRef<SCkInspector_ResolverAuthored>(Inspector.Build_Inspector(Fixture.EntityA)); }
    TestTrue(TEXT("all authored Resolver labels receive exact diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_FinalValueDiffMarked() && DiffAuthored->Is_PhaseDiffMarked()
        && DiffAuthored->Is_MetadataTagsDiffMarked() && DiffAuthored->Is_ModifierOpsDiffMarked()
        && DiffAuthored->Is_MetadataOpsDiffMarked());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Resolver resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorResolver.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorResolver.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource owns every Resolver header, label, and typed value binding"),
        Markup.Contains(TEXT(">Resolution</text>")) && Markup.Contains(TEXT(">Pending</text>"))
            && Markup.Contains(TEXT(">Final Value:</text>")) && Markup.Contains(TEXT(">Phase:</text>"))
            && Markup.Contains(TEXT(">Metadata Tags:</text>")) && Markup.Contains(TEXT(">Modifier Ops:</text>"))
            && Markup.Contains(TEXT(">Metadata Ops:</text>")) && Markup.Contains(TEXT("resolver-final-value"))
            && Markup.Contains(TEXT("resolver-phase")) && Markup.Contains(TEXT("resolver-metadata-tags"))
            && Markup.Contains(TEXT("resolver-modifier-ops")) && Markup.Contains(TEXT("resolver-metadata-ops")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by Resolver A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Resolver A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing Resolver binding is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"value\" bind=\"missing-binding\" /></region></ui>"),
        TEXT(""), TEXT("Resolver B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    const TSharedPtr<SWidget> PendingSectionA = FindTaggedWidget(
        ViewA->GetRegion(TEXT("main")), FName{TEXT("resolver-pending-section")});
    if (NOT TestTrue(TEXT("authored Resolver exposes the pending section node"), PendingSectionA.IsValid()))
    { return false; }
    TestEqual(TEXT("pending section is visible while PendingOperations exists"),
        PendingSectionA->GetVisibility(), EVisibility::Visible);
    TestTrue(TEXT("PendingOperations removal succeeds for visibility coverage"),
        Fixture.EntityA.Try_Remove<ck::FFragment_ResolverDataBundle_PendingOperations>());
    TestTrue(TEXT("PendingOperations removal is synchronous and bound values fail closed"),
        NOT Fixture.EntityA.Has<ck::FFragment_ResolverDataBundle_PendingOperations>()
            && AuthoredA->Get_ModifierOpsText() == TEXT("--")
            && AuthoredA->Get_MetadataOpsText() == TEXT("--"));
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    TestEqual(TEXT("pending section collapses live after PendingOperations removal"),
        PendingSectionA->GetVisibility(), EVisibility::Collapsed);
    auto& RestoredPending = Fixture.EntityA.Add<ck::FFragment_ResolverDataBundle_PendingOperations>();
    RestoredPending._PendingModifiersOperations.Emplace();
    RestoredPending._PendingMetadataOperations.Emplace();
    RestoredPending._PendingMetadataOperations.Emplace();
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("pending section reappears live with restored count values"),
        PendingSectionA->GetVisibility() == EVisibility::Visible
            && AuthoredA->Get_ModifierOpsText() == TEXT("1")
            && AuthoredA->Get_MetadataOpsText() == TEXT("2"));

    auto& CurrentA = Fixture.EntityA.Get<ck::FFragment_ResolverDataBundle_Current>();
    CurrentA._CurrentPhaseIndex = 2;
    TestEqual(TEXT("out-of-range current phase fails closed against configured phases"),
        AuthoredA->Get_PhaseText(), FString{TEXT("-- / 2")});
    CurrentA._CurrentPhaseIndex = 0;

    auto PartialEntity = Fixture.EntityA;
    TestTrue(TEXT("Params removal succeeds for the partial Resolver fixture"),
        PartialEntity.Try_Remove<ck::FFragment_ResolverDataBundle_Params>());
    TestTrue(TEXT("Current-only Resolver composition remains inspectable"), Inspector.CanInspect(PartialEntity));
    TestTrue(TEXT("missing Params degrades only phase state"),
        AuthoredA->Get_IsAvailable() && AuthoredA->Get_FinalValueText() == TEXT("12.500")
            && AuthoredA->Get_PhaseText() == TEXT("-- / 0")
            && AuthoredA->Get_MetadataTagsText().Contains(TEXT("Combat.Attack"))
            && AuthoredA->Get_ModifierOpsText() == TEXT("1")
            && AuthoredA->Get_MetadataOpsText() == TEXT("2"));
    TestTrue(TEXT("Current removal succeeds for the unavailable Resolver fixture"),
        PartialEntity.Try_Remove<ck::FFragment_ResolverDataBundle_Current>());
    TestFalse(TEXT("Resolver composition without Current is not inspectable"), Inspector.CanInspect(PartialEntity));
    TestTrue(TEXT("mounted authored reads fail closed after Current removal"),
        NOT AuthoredA->Get_IsAvailable() && AuthoredA->Get_FinalValueText() == TEXT("--")
            && AuthoredA->Get_PhaseText() == TEXT("-- / 0") && AuthoredA->Get_MetadataTagsText() == TEXT("--")
            && AuthoredA->Get_ModifierOpsText() == TEXT("--") && AuthoredA->Get_MetadataOpsText() == TEXT("--"));

    TSharedPtr<SCkInspector_ResolverAuthored> DestructorAuthored;
    { auto DestructorInspector = MakeUnique<FCkInspector_Resolver>(); DestructorAuthored = StaticCastSharedRef<SCkInspector_ResolverAuthored>(DestructorInspector->Build_Inspector(Fixture.EntityB)); }
    TestTrue(TEXT("inspector destruction makes its authored Resolver build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained Resolver view inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build Resolver view"), ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
