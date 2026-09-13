#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Jolt.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkJolt/Body/CkJoltBody_Fragment.h"
#include "CkJolt/Body/CkJoltBody_Fragment_Data.h"
#include "CkJolt/Character/CkJoltCharacter_Fragment.h"
#include "CkJolt/Character/CkJoltCharacter_Fragment_Data.h"
#include "CkJolt/StaticWorld/CkJoltStaticActor_Fragment.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorJoltAuthored,
    "Ck.UiAuthoring.EcsDebugger.JoltInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorJoltAuthored::RunTest(const FString&) -> bool
{
    auto InvalidInspector = FCkInspector_Jolt{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored Jolt shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_JoltAuthored")}))
    {
        AddError(InvalidInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_JoltAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_JoltAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid Jolt shell fails closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && NOT InvalidAuthored->Get_HasBody() && NOT InvalidAuthored->Get_HasCharacter()
            && NOT InvalidAuthored->Get_HasStaticActor());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid Jolt shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT InvalidAuthored->Get_View().IsValid() && NOT InvalidAuthored->Get_SpeedSeries().IsValid());

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    auto EntityA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    auto EntityB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(LifetimeOwner);
    if (NOT TestTrue(TEXT("fixture creates two independent entities"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB))
    { return false; }

    auto BodyParamsA = FCk_Fragment_JoltBody_ParamsData{ECk_JoltBody_ShapeSource::ExplicitShape};
    BodyParamsA.Set_MotionType(ECk_MotionType::Dynamic);
    EntityA.Add<ck::FFragment_JoltBody_Params>(BodyParamsA);
    EntityA.Add<ck::FFragment_JoltBody_Current>();
    EntityA.Add<ck::FFragment_JoltCharacter_Params>(FCk_Fragment_JoltCharacter_ParamsData{34.0f, 88.0f});
    auto& CharacterA = EntityA.Add<ck::FFragment_JoltCharacter_Current>();
    CharacterA.Set_GroundStateMirror(ECk_JoltCharacter_GroundState::OnGround);
    CharacterA.Set_GroundNormalMirror(FVector{0.0, 0.0, 1.0});
    CharacterA.Set_GroundVelocityMirror(FVector{12.5, -3.0, 0.25});
    EntityA.Add<ck::FFragment_JoltStaticActor_Current>();

    auto BodyParamsB = FCk_Fragment_JoltBody_ParamsData{ECk_JoltBody_ShapeSource::ExplicitShape};
    BodyParamsB.Set_MotionType(ECk_MotionType::Kinematic);
    EntityB.Add<ck::FFragment_JoltBody_Params>(BodyParamsB);
    EntityB.Add<ck::FFragment_JoltBody_Current>();
    EntityB.Add<ck::FTag_JoltBody_Sleeping>();

    auto Inspector = FCkInspector_Jolt{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("first Jolt entity mounts the authored inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_JoltAuthored")})
        || NOT TestEqual(TEXT("second Jolt entity mounts the authored inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_JoltAuthored")}))
    {
        AddError(Inspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_JoltAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_JoltAuthored>(RenderedA);
    const TSharedRef<SCkInspector_JoltAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_JoltAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("each Jolt build owns an independent view and speed series"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && AuthoredA->Get_SpeedSeries().IsValid()
            && AuthoredB->Get_SpeedSeries().IsValid()
            && AuthoredA->Get_SpeedSeries() != AuthoredB->Get_SpeedSeries());
    TestTrue(TEXT("authored Jolt projects the complete optional section topology"),
        AuthoredA->Get_HasBody() && AuthoredA->Get_HasCharacter() && AuthoredA->Get_HasStaticActor()
            && AuthoredB->Get_HasBody() && NOT AuthoredB->Get_HasCharacter()
            && NOT AuthoredB->Get_HasStaticActor());
    TestEqual(TEXT("authored Jolt projects A motion type"), AuthoredA->Get_MotionTypeText(), FString{TEXT("Dynamic")});
    TestEqual(TEXT("authored Jolt projects B motion type"), AuthoredB->Get_MotionTypeText(), FString{TEXT("Kinematic")});
    TestEqual(TEXT("authored Jolt projects A sleep state"), AuthoredA->Get_SleepStateText(), FString{TEXT("Awake")});
    TestEqual(TEXT("authored Jolt projects B sleep state"), AuthoredB->Get_SleepStateText(), FString{TEXT("Asleep")});
    TestEqual(TEXT("authored Jolt projects body-added state"), AuthoredA->Get_BodyAddedText(), FString{TEXT("No")});
    TestEqual(TEXT("authored Jolt projects linear velocity"), AuthoredA->Get_LinearVelocityAxisText(0), FString{TEXT("0.000")});
    TestEqual(TEXT("authored Jolt projects linear speed"), AuthoredA->Get_LinearSpeedText(), FString{TEXT("0.00")});
    TestEqual(TEXT("authored Jolt projects ground state"), AuthoredA->Get_GroundStateText(), FString{TEXT("On Ground")});
    TestEqual(TEXT("authored Jolt projects ground normal"), AuthoredA->Get_GroundNormalAxisText(2), FString{TEXT("1.000")});
    TestEqual(TEXT("authored Jolt projects ground velocity X"), AuthoredA->Get_GroundVelocityAxisText(0), FString{TEXT("12.500")});
    TestEqual(TEXT("authored Jolt projects ground velocity Y"), AuthoredA->Get_GroundVelocityAxisText(1), FString{TEXT("-3.000")});
    TestEqual(TEXT("authored Jolt projects static actor body count"), AuthoredA->Get_NumBodiesText(), FString{TEXT("0")});
    TestTrue(TEXT("each Jolt speed series receives only its own live samples"),
        AuthoredA->Get_SpeedSeries()->GetSamples().Num() == 1
            && AuthoredB->Get_SpeedSeries()->GetSamples().Num() == 1
            && AuthoredA->Get_SpeedSeries()->GetSamples()[0] == 0.0f
            && AuthoredB->Get_SpeedSeries()->GetSamples()[0] == 0.0f);

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestEqual(TEXT("native A capture retains every Jolt row"), RowsA.Num(), 11);
    TestEqual(TEXT("native B capture retains every Jolt row"), RowsB.Num(), 6);
    TestEqual(TEXT("native A capture projects motion type"), RowsA.FindRef(TEXT("Motion Type:")), FString{TEXT("Dynamic")});
    TestEqual(TEXT("native B capture projects motion type"), RowsB.FindRef(TEXT("Motion Type:")), FString{TEXT("Kinematic")});
    TestEqual(TEXT("native A capture projects sleep state"), RowsA.FindRef(TEXT("Sleep State:")), FString{TEXT("Awake")});
    TestEqual(TEXT("native B capture projects sleep state"), RowsB.FindRef(TEXT("Sleep State:")), FString{TEXT("Asleep")});
    TestEqual(TEXT("native A capture projects ground state"), RowsA.FindRef(TEXT("Ground State:")), FString{TEXT("On Ground")});
    TestEqual(TEXT("native A capture projects static body count"), RowsA.FindRef(TEXT("Num Bodies:")), FString{TEXT("0")});
    TestTrue(TEXT("native diff marks motion type"), Differing.Contains(TEXT("Motion Type:")));
    TestTrue(TEXT("native diff marks sleep state"), Differing.Contains(TEXT("Sleep State:")));
    TestTrue(TEXT("native diff marks ground state"), Differing.Contains(TEXT("Ground State:")));
    TestTrue(TEXT("native diff marks static body count"), Differing.Contains(TEXT("Num Bodies:")));
    TSharedPtr<SCkInspector_JoltAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_JoltAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored Jolt receives exact native diff marks"), DiffAuthored.IsValid()
        && DiffAuthored->Is_DiffMarked(TEXT("Motion Type:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Sleep State:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Ground State:"))
        && DiffAuthored->Is_DiffMarked(TEXT("Num Bodies:"))
        && NOT DiffAuthored->Is_DiffMarked(TEXT("Body Added:")));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Jolt resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorJolt.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorJolt.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML/CSS owns every Jolt section and row without native ports"),
        Markup.Contains(TEXT(">Jolt Body</text>")) && Markup.Contains(TEXT(">Jolt Character</text>"))
            && Markup.Contains(TEXT(">Jolt Static Actor</text>")) && Markup.Contains(TEXT(">Body Id:</text>"))
            && Markup.Contains(TEXT(">Linear Velocity:</text>")) && Markup.Contains(TEXT("debug-sparkline"))
            && Markup.Contains(TEXT(">Ground State:</text>")) && Markup.Contains(TEXT(">Ground Normal:</text>"))
            && Markup.Contains(TEXT(">Ground Velocity:</text>")) && Markup.Contains(TEXT(">Source Actor:</text>"))
            && Markup.Contains(TEXT(">Num Bodies:</text>")) && NOT Markup.Contains(TEXT("<native"))
            && Stylesheet.Contains(TEXT(".jolt-inspector")) && Stylesheet.Contains(TEXT(".jolt-section"))
            && Stylesheet.Contains(TEXT(".jolt-row")) && Stylesheet.Contains(TEXT(".jolt-sparkline-value")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible Jolt reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Jolt A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible Jolt reload advances A and isolates B"),
        ViewA->GetRevision() > RevisionABefore && ViewB->GetRevision() == RevisionBBefore);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RejectedRevision = ViewB->GetRevision();
    TestFalse(TEXT("missing Jolt text binding is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("bind=\"jolt-body-id\""), TEXT("bind=\"jolt-body-id-missing\"")),
        Stylesheet, TEXT("Jolt B rejected binding candidate")).Succeeded);
    TestTrue(TEXT("rejected Jolt reload retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get()
            && ViewB->GetRevision() == RejectedRevision);

    EntityA.Try_Remove<ck::FFragment_JoltBody_Params>();
    AuthoredA->Tick(FGeometry::MakeRoot(FVector2D{640.0f, 480.0f}, FSlateLayoutTransform{}), 0.0, 0.016f);
    TestTrue(TEXT("body composition loss hides only that section and clears its series"),
        ck::IsValid(EntityA) && NOT AuthoredA->Get_HasBody() && AuthoredA->Get_HasCharacter()
            && AuthoredA->Get_HasStaticActor() && AuthoredA->Get_BodyIdText() == TEXT("--")
            && AuthoredA->Get_SpeedSeries()->GetSamples().IsEmpty());
    EntityA.Try_Remove<ck::FFragment_JoltCharacter_Params>();
    EntityA.Try_Remove<ck::FFragment_JoltStaticActor_Current>();
    TestTrue(TEXT("complete Jolt composition loss makes the retained view unavailable"),
        ck::IsValid(EntityA) && NOT Inspector.CanInspect(EntityA) && NOT AuthoredA->Get_IsAvailable()
            && AuthoredA->Get_GroundStateText() == TEXT("--") && AuthoredA->Get_SourceActorText() == TEXT("--"));

    TSharedPtr<SCkInspector_JoltAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Jolt>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_JoltAuthored>(
            DestructorInspector->Build_Inspector(EntityB));
    }
    TestTrue(TEXT("Jolt inspector destruction releases its retained authored build"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT DestructorAuthored->Get_View().IsValid()
            && NOT DestructorAuthored->Get_SpeedSeries().IsValid());

    const TWeakPtr<FCkUiView> ReleasedA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedB = ViewB;
    const TWeakPtr<FCkUiView> ReleasedDiff = DiffAuthored->Get_View();
    Inspector.OnDeactivated();
    TestTrue(TEXT("Jolt deactivation makes every retained authored view inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset();
    ViewB.Reset();
    TestFalse(TEXT("Jolt deactivation releases every per-build view"),
        ReleasedA.IsValid() || ReleasedB.IsValid() || ReleasedDiff.IsValid());
    return true;
}

#endif
