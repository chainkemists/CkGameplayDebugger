#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_IskmRenderer.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkIskmRenderer/AnimCollection/CkIskmAnimCollection_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Fragment.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Fragment_Data.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_MeshDesc.h"
#include "CkIskmRenderer/Renderer/CkIskmRenderer_Utils.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_inspector_iskm_renderer_authored_test
{
    auto ContainsText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock"))
        {
            const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        if (InRoot->GetTypeAsString() == TEXT("SCkFlexText"))
        {
            const TSharedRef<SCkFlexText> Text = StaticCastSharedRef<SCkFlexText>(InRoot);
            if (Text->GetText().ToString() == InText) { return true; }
        }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto SetObjectProperty(UObject* InObject, const FName InName, UObject* InValue,
        const UClass* InExpectedClass) -> bool
    {
        const auto* Property = InObject != nullptr
            ? FindFProperty<FObjectProperty>(InObject->GetClass(), InName) : nullptr;
        if (Property == nullptr || InExpectedClass == nullptr
            || NOT Property->PropertyClass->IsChildOf(InExpectedClass))
        { return false; }
        Property->SetObjectPropertyValue_InContainer(InObject, InValue);
        return true;
    }

    auto SetStructArrayCount(UObject* InObject, const FName InName, UScriptStruct* InExpectedStruct,
        const int32 InCount) -> bool
    {
        auto* ArrayProperty = InObject != nullptr
            ? FindFProperty<FArrayProperty>(InObject->GetClass(), InName) : nullptr;
        auto* ElementProperty = ArrayProperty != nullptr ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
        if (ElementProperty == nullptr || ElementProperty->Struct != InExpectedStruct || InCount < 0)
        { return false; }

        FScriptArrayHelper Values{ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(InObject)};
        Values.EmptyValues();
        for (int32 Index = 0; Index < InCount; ++Index)
        { Values.AddValue(); }
        return Values.Num() == InCount;
    }

    struct FFixture final
    {
        TStrongObjectPtr<UCk_IskmAnimCollection_Data> CollectionA;
        TStrongObjectPtr<UCk_IskmAnimCollection_Data> CollectionB;
        TStrongObjectPtr<UCk_IskmRenderer_Data> RendererA;
        TStrongObjectPtr<UCk_IskmRenderer_Data> RendererB;
        FCk_Handle EntityA;
        FCk_Handle EntityB;
    };

    auto CreateFixture(ck::FEcsWorld& InWorld, FFixture& OutFixture) -> bool
    {
        OutFixture.CollectionA = TStrongObjectPtr<UCk_IskmAnimCollection_Data>{
            NewObject<UCk_IskmAnimCollection_Data>(GetTransientPackage(), TEXT("Fixture_AnimCollection_A"))};
        OutFixture.CollectionB = TStrongObjectPtr<UCk_IskmAnimCollection_Data>{
            NewObject<UCk_IskmAnimCollection_Data>(GetTransientPackage(), TEXT("Fixture_AnimCollection_B"))};
        OutFixture.RendererA = TStrongObjectPtr<UCk_IskmRenderer_Data>{
            NewObject<UCk_IskmRenderer_Data>(GetTransientPackage(), TEXT("Fixture_RendererData_A"))};
        OutFixture.RendererB = TStrongObjectPtr<UCk_IskmRenderer_Data>{
            NewObject<UCk_IskmRenderer_Data>(GetTransientPackage(), TEXT("Fixture_RendererData_B"))};
        if (NOT OutFixture.CollectionA.IsValid() || NOT OutFixture.CollectionB.IsValid()
            || NOT OutFixture.RendererA.IsValid() || NOT OutFixture.RendererB.IsValid())
        { return false; }

        const bool bAuthored =
            SetStructArrayCount(OutFixture.CollectionA.Get(), TEXT("_Sequences"),
                FCk_IskmAnimCollection_SequenceDef::StaticStruct(), 2)
            && SetStructArrayCount(OutFixture.CollectionB.Get(), TEXT("_Sequences"),
                FCk_IskmAnimCollection_SequenceDef::StaticStruct(), 4)
            && SetObjectProperty(OutFixture.RendererA.Get(), TEXT("_AnimCollection"), OutFixture.CollectionA.Get(),
                UCk_IskmAnimCollection_Data::StaticClass())
            && SetObjectProperty(OutFixture.RendererB.Get(), TEXT("_AnimCollection"), OutFixture.CollectionB.Get(),
                UCk_IskmAnimCollection_Data::StaticClass())
            && SetStructArrayCount(OutFixture.RendererA.Get(), TEXT("_Submeshes"),
                FCk_IskmRenderer_MeshDesc::StaticStruct(), 1)
            && SetStructArrayCount(OutFixture.RendererB.Get(), TEXT("_Submeshes"),
                FCk_IskmRenderer_MeshDesc::StaticStruct(), 3);
        if (NOT bAuthored) { return false; }

        OutFixture.EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        OutFixture.EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        const auto IskmA = UCk_Utils_IskmRenderer_UE::Add(OutFixture.EntityA, OutFixture.RendererA.Get());
        const auto IskmB = UCk_Utils_IskmRenderer_UE::Add(OutFixture.EntityB, OutFixture.RendererB.Get());
        return ck::IsValid(IskmA) && ck::IsValid(IskmB)
            && OutFixture.EntityA.Try_Remove<ck::FTag_IskmRenderer_NeedsSetup>()
            && OutFixture.EntityB.Try_Remove<ck::FTag_IskmRenderer_NeedsSetup>();
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorIskmRendererAuthored,
    "Ck.UiAuthoring.EcsDebugger.IskmRendererInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorIskmRendererAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_iskm_renderer_authored_test;

    auto InvalidInspector = FCkInspector_IskmRenderer{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored ISKM Renderer shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_IskmRendererAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_IskmRendererAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_IskmRendererAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid authored values fail closed without typed fragment access"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_RendererDataText() == TEXT("--")
            && InvalidAuthored->Get_AnimCollectionText() == TEXT("--")
            && InvalidAuthored->Get_SubmeshesText() == TEXT("--")
            && InvalidAuthored->Get_SequencesText() == TEXT("--")
            && InvalidAuthored->Get_DefaultAnimInstanceText() == TEXT("--")
            && InvalidAuthored->Get_CustomDataSlotsText() == TEXT("--"));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid authored shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    auto World = ck::FEcsWorld{};
    auto Fixture = FFixture{};
    if (NOT TestTrue(TEXT("fixture creates two distinct, processor-free ISKM renderer entities"),
        CreateFixture(World, Fixture) && ck::IsValid(Fixture.EntityA) && ck::IsValid(Fixture.EntityB)
            && Fixture.EntityA != Fixture.EntityB && UCk_Utils_IskmRenderer_UE::Has(Fixture.EntityA)
            && UCk_Utils_IskmRenderer_UE::Has(Fixture.EntityB)))
    { return false; }

    auto Inspector = FCkInspector_IskmRenderer{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(Fixture.EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(Fixture.EntityB);
    if (NOT TestEqual(TEXT("first build returns authored ISKM Renderer widget"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_IskmRendererAuthored")})
        || NOT TestEqual(TEXT("second build returns authored ISKM Renderer widget"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_IskmRendererAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_IskmRendererAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_IskmRendererAuthored>(RenderedA);
    const TSharedRef<SCkInspector_IskmRendererAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_IskmRendererAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each production build owns an independent accepted ISKM Renderer view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && AuthoredA->Get_IsAvailable()
            && AuthoredB->Get_IsAvailable() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    TestTrue(TEXT("authored values preserve exact independent names and counts"),
        AuthoredA->Get_RendererDataText() == TEXT("Fixture_RendererData_A")
            && AuthoredA->Get_AnimCollectionText() == TEXT("Fixture_AnimCollection_A")
            && AuthoredA->Get_SubmeshesText() == TEXT("1") && AuthoredA->Get_SequencesText() == TEXT("2")
            && AuthoredB->Get_RendererDataText() == TEXT("Fixture_RendererData_B")
            && AuthoredB->Get_AnimCollectionText() == TEXT("Fixture_AnimCollection_B")
            && AuthoredB->Get_SubmeshesText() == TEXT("3") && AuthoredB->Get_SequencesText() == TEXT("4"));
    TestTrue(TEXT("authored values preserve the remaining native presentation"),
        AuthoredA->Get_DefaultAnimInstanceText() == TEXT("(IskmNotify fallback)")
            && AuthoredA->Get_CustomDataSlotsText() == TEXT("0")
            && ContainsText(RenderedA, TEXT("Fixture_RendererData_A"))
            && ContainsText(RenderedA, TEXT("Fixture_AnimCollection_A"))
            && ContainsText(RenderedA, TEXT("(IskmNotify fallback)")));

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native builder remains multi-selection authority for every ISKM Renderer row"),
        RowsA.FindRef(TEXT("RendererData:")) != RowsB.FindRef(TEXT("RendererData:"))
            && RowsA.FindRef(TEXT("AnimCollection:")) != RowsB.FindRef(TEXT("AnimCollection:"))
            && RowsA.FindRef(TEXT("Submeshes:")) != RowsB.FindRef(TEXT("Submeshes:"))
            && RowsA.FindRef(TEXT("Sequences:")) != RowsB.FindRef(TEXT("Sequences:"))
            && RowsA.FindRef(TEXT("Default AnimInstance:")) == TEXT("(IskmNotify fallback)")
            && RowsA.FindRef(TEXT("Custom Data Slots:")) == TEXT("0")
            && Differing.Num() == 4 && Differing.Contains(TEXT("RendererData:"))
            && Differing.Contains(TEXT("AnimCollection:")) && Differing.Contains(TEXT("Submeshes:"))
            && Differing.Contains(TEXT("Sequences:"))
            && NOT Differing.Contains(TEXT("Default AnimInstance:"))
            && NOT Differing.Contains(TEXT("Custom Data Slots:")));

    TSharedPtr<SCkInspector_IskmRendererAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_IskmRendererAuthored>(Inspector.Build_Inspector(Fixture.EntityA));
    }
    TestTrue(TEXT("all authored ISKM Renderer labels receive exact diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_RendererDataDiffMarked() && DiffAuthored->Is_AnimCollectionDiffMarked()
        && DiffAuthored->Is_SubmeshesDiffMarked() && DiffAuthored->Is_SequencesDiffMarked()
        && NOT DiffAuthored->Is_DefaultAnimInstanceDiffMarked()
        && NOT DiffAuthored->Is_CustomDataSlotsDiffMarked());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed ISKM Renderer resource is readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmRenderer.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmRenderer.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource preserves exact presentation labels and typed value bindings"),
        Markup.Contains(TEXT(">RendererData:</text>")) && Markup.Contains(TEXT(">AnimCollection:</text>"))
            && Markup.Contains(TEXT(">Submeshes:</text>")) && Markup.Contains(TEXT(">Sequences:</text>"))
            && Markup.Contains(TEXT(">Default AnimInstance:</text>"))
            && Markup.Contains(TEXT(">Custom Data Slots:</text>"))
            && Markup.Contains(TEXT("bind=\"iskm-renderer-data\""))
            && Markup.Contains(TEXT("bind=\"iskm-anim-collection\""))
            && Markup.Contains(TEXT("label-bind=\"iskm-submeshes\""))
            && Markup.Contains(TEXT("label-bind=\"iskm-sequences\""))
            && Markup.Contains(TEXT("bind=\"iskm-default-anim-instance\""))
            && Markup.Contains(TEXT("label-bind=\"iskm-custom-data-slots\"")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible reload is accepted by ISKM Renderer A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("ISKM Renderer A compatible candidate")).Succeeded);
    TestTrue(TEXT("A reload retains its identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing ISKM Renderer binding is rejected atomically by B"), ViewB->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"value\" bind=\"missing-binding\" /></region></ui>"),
        TEXT(""), TEXT("ISKM Renderer B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected B reload retains its tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    auto StaleEntity = Fixture.EntityA;
    TestTrue(TEXT("Current removal succeeds for the stale-fragment fixture"),
        StaleEntity.Try_Remove<ck::FFragment_IskmRenderer_Current>());
    TestFalse(TEXT("partial ISKM Renderer composition is no longer inspectable"),
        UCk_Utils_IskmRenderer_UE::Has(StaleEntity) || Inspector.CanInspect(StaleEntity));
    TestTrue(TEXT("mounted authored reads fail closed after Current removal"),
        NOT AuthoredA->Get_IsAvailable() && AuthoredA->Get_RendererDataText() == TEXT("--")
            && AuthoredA->Get_AnimCollectionText() == TEXT("--") && AuthoredA->Get_SubmeshesText() == TEXT("--")
            && AuthoredA->Get_SequencesText() == TEXT("--")
            && AuthoredA->Get_DefaultAnimInstanceText() == TEXT("--")
            && AuthoredA->Get_CustomDataSlotsText() == TEXT("--"));
    auto StaleRows = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(Fixture.EntityA); StaleRows = Capture.Get_Rows(); }
    TestTrue(TEXT("native captured reads fail closed after Current removal"),
        StaleRows.FindRef(TEXT("RendererData:")) == TEXT("--")
            && StaleRows.FindRef(TEXT("AnimCollection:")) == TEXT("--")
            && StaleRows.FindRef(TEXT("Submeshes:")) == TEXT("0")
            && StaleRows.FindRef(TEXT("Sequences:")) == TEXT("0")
            && StaleRows.FindRef(TEXT("Default AnimInstance:")) == TEXT("--")
            && StaleRows.FindRef(TEXT("Custom Data Slots:")) == TEXT("0"));

    TSharedPtr<SCkInspector_IskmRendererAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_IskmRenderer>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_IskmRendererAuthored>(
            DestructorInspector->Build_Inspector(Fixture.EntityB));
    }
    TestTrue(TEXT("inspector destruction makes its authored ISKM Renderer build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every retained ISKM Renderer view inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build ISKM Renderer view"),
        ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
