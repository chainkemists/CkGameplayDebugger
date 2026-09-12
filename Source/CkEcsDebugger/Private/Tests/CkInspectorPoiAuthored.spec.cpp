#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Poi.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"
#include "CkEntityTag/CkEntityTag_Processor.h"
#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkPoi/CkPoi_Fragment.h"
#include "CkPoi/CkPoi_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ck_inspector_poi_authored_test
{
    auto FindSwitch(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_Switch") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SCkDebug_Switch>(InRoot); }

        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SCkDebug_Switch> Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto MakeClick() -> FPointerEvent
    {
        return FPointerEvent{
            0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::LeftMouseButton},
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    }

    auto CreatePoi(
        ck::FEcsWorld& InWorld,
        const FGameplayTag InCategory,
        const FGameplayTag InLabel,
        const FVector& InLocation) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (ck::Is_NOT_Valid(Entity))
        { return {}; }

        UCk_Utils_Transform_UE::Add(Entity, FTransform{InLocation}, ECk_Replication::DoesNotReplicate);
        auto Params = FCk_Fragment_Poi_ParamsData{InCategory};
        Params.Set_Label(InLabel);
        return UCk_Utils_Poi_UE::Add(Entity, Params);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorPoiAuthored,
    "Ck.UiAuthoring.EcsDebugger.PoiInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorPoiAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_poi_authored_test;

    auto InvalidInspector = FCkInspector_Poi{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored POI shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_PoiAuthored")}))
    {
        AddError(InvalidInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_PoiAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_PoiAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid POI shell fails every authored projection closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_LabelText() == TEXT("--") && InvalidAuthored->Get_StateText() == TEXT("--")
            && NOT InvalidAuthored->Get_IsDisabled() && InvalidAuthored->Get_WorldPositionAxisText(0) == TEXT("--")
            && InvalidAuthored->Get_StateForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral)
            && InvalidAuthored->Get_StateBackground() == CkStyle::GetToneDimColor(ECk_Tone::Neutral));
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid POI shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    const FGameplayTag CategoryA = FGameplayTag::RequestGameplayTag(FName{TEXT("Poi.Category.Area")}, false);
    const FGameplayTag CategoryB = FGameplayTag::RequestGameplayTag(FName{TEXT("Poi.Category.Danger")}, false);
    const FGameplayTag LabelA = CategoryA;
    const FGameplayTag LabelB = CategoryB;
    if (NOT TestTrue(TEXT("fixture reuses distinct registered POI categories and labels"),
        CategoryA.IsValid() && CategoryB.IsValid() && LabelA.IsValid() && LabelB.IsValid()
            && CategoryA != CategoryB && LabelA != LabelB)) { return false; }

    auto World = ck::FEcsWorld{};
    auto RequestProcessor = ck::FProcessor_EntityTag_HandleRequests{World.Get_Registry()};
    auto PoiA = CreatePoi(World, CategoryA, LabelA, FVector{125.0f, -75.0f, 20.0f});
    auto PoiB = CreatePoi(World, CategoryB, LabelB, FVector{-8.0f, 444.0f, 96.0f});
    if (NOT TestTrue(TEXT("fixture composes two hermetic POIs through public lifetime, transform, and POI APIs"),
        ck::IsValid(PoiA) && ck::IsValid(PoiB) && PoiA != PoiB
            && UCk_Utils_Transform_UE::Has(PoiA) && UCk_Utils_Transform_UE::Has(PoiB)
            && UCk_Utils_Poi_UE::Has(PoiA) && UCk_Utils_Poi_UE::Has(PoiB))) { return false; }
    auto Inspector = FCkInspector_Poi{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(PoiA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(PoiB);
    if (NOT TestEqual(TEXT("A mounts the authored POI inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_PoiAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored POI inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_PoiAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_PoiAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_PoiAuthored>(RenderedA);
    const TSharedRef<SCkInspector_PoiAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_PoiAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TSharedPtr<FCkUiCollection> CategoriesA = AuthoredA->Get_CategoryTagsCollection();
    TSharedPtr<FCkUiCollection> CategoriesB = AuthoredB->Get_CategoryTagsCollection();
    if (NOT TestTrue(TEXT("each production build owns independent accepted POI view and repeat state"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && CategoriesA.IsValid() && CategoriesB.IsValid() && CategoriesA != CategoriesB
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    {
        AddError(AuthoredA->Get_LoadError());
        AddError(AuthoredB->Get_LoadError());
        return false;
    }
    TestTrue(TEXT("authored POI mounts safely before deferred categories materialize"),
        CategoriesA->GetRecords().IsEmpty() && CategoriesB->GetRecords().IsEmpty());
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    AuthoredB->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    if (NOT TestTrue(TEXT("authored POI reconciles deferred category materialization"),
        CategoriesA->GetRecords().Num() == 1 && CategoriesB->GetRecords().Num() == 1))
    {
        return false;
    }
    TestTrue(TEXT("two POIs retain distinct authored category, label, position, state, and exact status tones"),
        AuthoredA->Get_IsAvailable() && AuthoredB->Get_IsAvailable()
            && CategoriesA->GetRecords()[0]->GetKey() == CategoryA.ToString()
            && CategoriesB->GetRecords()[0]->GetKey() == CategoryB.ToString()
            && AuthoredA->Get_LabelText() == LabelA.ToString() && AuthoredB->Get_LabelText() == LabelB.ToString()
            && AuthoredA->Get_StateText() == TEXT("Enabled") && AuthoredB->Get_StateText() == TEXT("Enabled")
            && NOT AuthoredA->Get_IsDisabled() && NOT AuthoredB->Get_IsDisabled()
            && AuthoredA->Get_WorldPositionAxisText(0) == TEXT("125")
            && AuthoredA->Get_WorldPositionAxisText(1) == TEXT("-75")
            && AuthoredA->Get_WorldPositionAxisText(2) == TEXT("20")
            && AuthoredB->Get_WorldPositionAxisText(0) == TEXT("-8")
            && AuthoredB->Get_WorldPositionAxisText(1) == TEXT("444")
            && AuthoredB->Get_WorldPositionAxisText(2) == TEXT("96")
            && AuthoredA->Get_StateForeground() == CkStyle::GetToneColor(ECk_Tone::Ok)
            && AuthoredA->Get_StateBackground() == CkStyle::GetToneDimColor(ECk_Tone::Ok));

    const TSharedPtr<const FCkUiRecord> StableCategoryA = CategoriesA->FindRecord(CategoryA.ToString());
    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(PoiA, CategoryB);
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("later category add refreshes the repeat while retaining the existing stable record"),
        StableCategoryA.IsValid() && CategoriesA->GetRecords().Num() == 2
            && CategoriesA->FindRecord(CategoryA.ToString()) == StableCategoryA
            && CategoriesA->FindRecord(CategoryB.ToString()).IsValid());
    UCk_Utils_EntityTag_UE::Request_TryRemove_UsingGameplayTag(PoiA, CategoryB, {});
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("later category removal refreshes the repeat without replacing the retained category"),
        CategoriesA->GetRecords().Num() == 1
            && CategoriesA->FindRecord(CategoryA.ToString()) == StableCategoryA
            && NOT CategoriesA->FindRecord(CategoryB.ToString()).IsValid());

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(PoiA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(PoiB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native POI capture remains the five-row comparison authority"),
        RowsA.Num() == 5 && RowsB.Num() == 5
            && RowsA.FindRef(TEXT("Category Tags:")) != RowsB.FindRef(TEXT("Category Tags:"))
            && RowsA.FindRef(TEXT("Label:")) != RowsB.FindRef(TEXT("Label:"))
            && RowsA.FindRef(TEXT("State:")) == RowsB.FindRef(TEXT("State:"))
            && RowsA.FindRef(TEXT("Disabled:")) == RowsB.FindRef(TEXT("Disabled:"))
            && RowsA.FindRef(TEXT("World Pos:")) != RowsB.FindRef(TEXT("World Pos:"))
            && Differing.Num() == 3 && Differing.Contains(TEXT("Category Tags:"))
            && Differing.Contains(TEXT("Label:")) && Differing.Contains(TEXT("World Pos:")));
    TSharedPtr<SCkInspector_PoiAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_PoiAuthored>(Inspector.Build_Inspector(PoiA));
    }
    TestTrue(TEXT("authored POI receives the exact native five-row diff mark set"), DiffAuthored.IsValid()
        && DiffAuthored->Get_View().IsValid() && DiffAuthored->Is_CategoryTagsDiffMarked()
        && DiffAuthored->Is_LabelDiffMarked() && NOT DiffAuthored->Is_StateDiffMarked()
        && NOT DiffAuthored->Is_DisabledDiffMarked() && DiffAuthored->Is_WorldPositionDiffMarked());

    const TSharedPtr<SCkDebug_Switch> HeldSwitch = FindSwitch(RenderedA, TEXT("poi-disabled-switch"));
    if (NOT TestTrue(TEXT("mounted POI disabled switch is physical and initially enabled"),
        HeldSwitch.IsValid() && HeldSwitch->IsEnabled())) { return false; }
    const FGeometry SwitchGeometry = FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{});
    const FPointerEvent Click = MakeClick();
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    HeldSwitch->SlatePrepass();
    TestTrue(TEXT("mouse switch queues optimistically and serializes further input until the request drains"),
        NOT UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName)
            && AuthoredA->Get_IsDisabled() && NOT AuthoredA->Get_CanToggleDisabled()
            && NOT HeldSwitch->IsEnabled());
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    HeldSwitch->SlatePrepass();
    TestTrue(TEXT("pumped public EntityTag request applies disabled state and Err tone"),
        UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName)
            && AuthoredA->Get_IsDisabled() && AuthoredA->Get_CanToggleDisabled() && HeldSwitch->IsEnabled()
            && AuthoredA->Get_StateText() == TEXT("Disabled")
            && AuthoredA->Get_StateForeground() == CkStyle::GetToneColor(ECk_Tone::Err)
            && AuthoredA->Get_StateBackground() == CkStyle::GetToneDimColor(ECk_Tone::Err));
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    HeldSwitch->SlatePrepass();
    TestTrue(TEXT("second mounted switch mouse path queues and applies public disabled-tag removal"),
        NOT UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName)
            && NOT AuthoredA->Get_IsDisabled() && AuthoredA->Get_StateText() == TEXT("Enabled"));

    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(PoiA, Tag_Poi_DisabledName);
    UCk_Utils_EntityTag_UE::Add_UsingGameplayTag(PoiA, Tag_Poi_DisabledName);
    RequestProcessor.Pump();
    TestTrue(TEXT("fixture establishes the counted disabled-tag case"), AuthoredA->Get_IsDisabled());
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    HeldSwitch->SlatePrepass();
    TestTrue(TEXT("one enable drains one counted claim and leaves the switch available for the next claim"),
        AuthoredA->Get_IsDisabled() && AuthoredA->Get_CanToggleDisabled() && HeldSwitch->IsEnabled());
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("a second enable drains the final counted claim without locking the control"),
        NOT AuthoredA->Get_IsDisabled() && AuthoredA->Get_CanToggleDisabled()
            && NOT UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed POI HTML and CSS resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPoi.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorPoi.ui.css"))))) { return false; }
    TestTrue(TEXT("installed POI HTML/CSS owns every label, category repeat, and debug switch"),
        Markup.Contains(TEXT(">Category Tags:</text>")) && Markup.Contains(TEXT(">Label:</text>"))
            && Markup.Contains(TEXT(">State:</text>")) && Markup.Contains(TEXT(">Disabled:</text>"))
            && Markup.Contains(TEXT(">World Pos:</text>")) && Markup.Contains(TEXT("bind=\"poi-category-tags\""))
            && Markup.Contains(TEXT("<repeat")) && Markup.Contains(TEXT("<debug-switch id=\"poi-disabled-switch\""))
            && Stylesheet.Contains(TEXT("poi-category-tags-row")) && Stylesheet.Contains(TEXT("poi-label-value"))
            && Stylesheet.Contains(TEXT("poi-status")) && Stylesheet.Contains(TEXT("poi-switch"))
            && Stylesheet.Contains(TEXT("poi-position")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible POI reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("POI A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible POI reload retains A view and mounted switch identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore
            && ViewB->GetRevision() == RevisionBBefore && FindSwitch(RenderedA, TEXT("poi-disabled-switch")) == HeldSwitch);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing POI switch binding is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("changed=\"poi-disabled-changed\""), TEXT("changed=\"poi-missing-changed\"")),
        Stylesheet, TEXT("POI B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected POI reload retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    TSharedPtr<SCkInspector_PoiAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Poi>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_PoiAuthored>(DestructorInspector->Build_Inspector(PoiB));
    }
    TestTrue(TEXT("POI inspector destruction makes a valid retained authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    TestTrue(TEXT("removing POI identity after mount leaves its entity live"),
        PoiA.Try_Remove<ck::FTag_Poi>() && ck::IsValid(PoiA) && NOT Inspector.CanInspect(PoiA));
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    HeldSwitch->SlatePrepass();
    TestTrue(TEXT("POI-tag removal fails every mounted authored getter closed and disables the held switch"),
        NOT AuthoredA->Get_IsAvailable() && AuthoredA->Get_LabelText() == TEXT("--")
            && AuthoredA->Get_StateText() == TEXT("--") && NOT AuthoredA->Get_IsDisabled()
            && AuthoredA->Get_WorldPositionAxisText(0) == TEXT("--")
            && AuthoredA->Get_StateForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral)
            && AuthoredA->Get_StateBackground() == CkStyle::GetToneDimColor(ECk_Tone::Neutral)
            && CategoriesA->GetRecords().IsEmpty() && NOT HeldSwitch->IsEnabled());
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    TestFalse(TEXT("held disabled switch cannot mutate EntityTag state after POI removal"),
        UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName));

    TestTrue(TEXT("removing Transform after mount also invalidates POI authored state"),
        PoiB.Try_Remove<ck::FFragment_Transform>() && ck::IsValid(PoiB) && NOT Inspector.CanInspect(PoiB));
    AuthoredB->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f);
    TestTrue(TEXT("Transform removal fails POI getters closed without dereferencing partial state"),
        NOT AuthoredB->Get_IsAvailable() && AuthoredB->Get_LabelText() == TEXT("--")
            && AuthoredB->Get_StateText() == TEXT("--") && NOT AuthoredB->Get_IsDisabled()
            && AuthoredB->Get_WorldPositionAxisText(0) == TEXT("--")
            && AuthoredB->Get_StateForeground() == CkStyle::GetToneColor(ECk_Tone::Neutral)
            && AuthoredB->Get_StateBackground() == CkStyle::GetToneDimColor(ECk_Tone::Neutral)
            && CategoriesB->GetRecords().IsEmpty());
    Inspector.OnDeactivated();
    TestTrue(TEXT("POI deactivation releases all views and makes the held switch inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && NOT AuthoredA->Get_CategoryTagsCollection().IsValid() && NOT AuthoredB->Get_CategoryTagsCollection().IsValid());
    HeldSwitch->OnMouseButtonDown(SwitchGeometry, Click);
    RequestProcessor.Pump();
    TestFalse(TEXT("held switch remains inert after inspector release"),
        UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(PoiA, Tag_Poi_DisabledName));
    return true;
}

#endif
