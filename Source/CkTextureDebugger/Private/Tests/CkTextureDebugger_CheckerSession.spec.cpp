#include "CkTextureDebugger/Model/CkTextureDebugger_CheckerOverrideSession.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

namespace ck_texture_debugger_checker_session_spec
{
    struct FFixture
    {
        FFixture()
        {
            const auto WorldName = MakeUniqueObjectName(
                GetTransientPackage(), UWorld::StaticClass(), TEXT("CkTextureDebuggerSession"));
            World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(EWorldType::Game, false, WorldName)};

            BaseMaterial = TStrongObjectPtr<UMaterialInterface>{UMaterial::GetDefaultMaterial(MD_Surface)};
            Mesh = TStrongObjectPtr<UStaticMesh>{NewObject<UStaticMesh>(GetTransientPackage())};
            Mesh->GetStaticMaterials().Add(FStaticMaterial{BaseMaterial.Get()});
            Mesh->GetStaticMaterials().Add(FStaticMaterial{BaseMaterial.Get()});

            Checker = TStrongObjectPtr<UMaterialInstanceDynamic>{
                UMaterialInstanceDynamic::Create(BaseMaterial.Get(), GetTransientPackage())};
        }

        ~FFixture()
        {
            if (World.IsValid())
            { World->DestroyWorld(false); }
        }

        auto MakeComponent() -> UStaticMeshComponent*
        {
            auto* Actor = World->SpawnActor<AActor>();
            if (Actor == nullptr)
            { return nullptr; }

            auto* Component = NewObject<UStaticMeshComponent>(Actor);
            Actor->SetRootComponent(Component);
            Actor->AddInstanceComponent(Component);
            Component->SetStaticMesh(Mesh.Get());
            Component->RegisterComponent();
            return Component;
        }

        auto MakeTarget(
            UMeshComponent* InComponent,
            TArray<int32> InSlots) -> FCkTextureDebugger_CheckerTarget
        {
            auto Target = FCkTextureDebugger_CheckerTarget{};
            Target.Component = InComponent;
            Target.SlotIndices = MoveTemp(InSlots);
            return Target;
        }

        TStrongObjectPtr<UWorld> World;
        TStrongObjectPtr<UStaticMesh> Mesh;
        TStrongObjectPtr<UMaterialInstanceDynamic> Checker;
        TStrongObjectPtr<UMaterialInterface> BaseMaterial;
    };

    auto
        MakeMid(
            UMaterialInterface* InParent) -> TStrongObjectPtr<UMaterialInstanceDynamic>
    {
        return TStrongObjectPtr<UMaterialInstanceDynamic>{
            UMaterialInstanceDynamic::Create(InParent, GetTransientPackage())};
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_CheckerSession_SwitchTexture,
    "Ck.TextureDebugger.CheckerSession.SwitchTexture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_CheckerSession_SwitchTexture::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_checker_session_spec;

    constexpr auto CheckerMaterialPath = TEXT(
        "/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker");
    constexpr auto CheckerTextureAPath = TEXT(
        "/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_ColorGrid_2K.T_CkTextureChecker_ColorGrid_2K");
    constexpr auto CheckerTextureBPath = TEXT(
        "/CkDebugger/TextureDebugger/Textures/T_CkTextureChecker_GoldGray_4K.T_CkTextureChecker_GoldGray_4K");
    const auto CheckerTextureParameter = FName{TEXT("CheckerTexture")};

    auto Fixture = FFixture{};
    auto* Component = Fixture.MakeComponent();
    auto Master = TStrongObjectPtr<UMaterialInterface>{LoadObject<UMaterialInterface>(nullptr, CheckerMaterialPath)};
    auto TextureA = TStrongObjectPtr<UTexture>{LoadObject<UTexture>(nullptr, CheckerTextureAPath)};
    auto TextureB = TStrongObjectPtr<UTexture>{LoadObject<UTexture>(nullptr, CheckerTextureBPath)};
    TestNotNull(TEXT("Registered static-mesh fixture"), Component);
    TestTrue(TEXT("Checker master material loads"), Master.IsValid());
    TestTrue(TEXT("First checker texture loads"), TextureA.IsValid());
    TestTrue(TEXT("Second checker texture loads"), TextureB.IsValid());
    if (Component == nullptr || NOT Master.IsValid() || NOT TextureA.IsValid() || NOT TextureB.IsValid())
    { return false; }

    auto Original = MakeMid(Fixture.BaseMaterial.Get());
    Component->SetMaterial(0, Original.Get());

    auto Checker = MakeMid(Master.Get());
    Checker->SetTextureParameterValue(CheckerTextureParameter, TextureA.Get());

    auto EmptySession = FCkTextureDebugger_CheckerOverrideSession{};
    const auto NoSession = EmptySession.SwitchCheckerTexture(TextureB.Get(), CheckerTextureParameter);
    TestEqual(TEXT("Switch without a session is rejected"), NoSession.Result,
        ECkTextureDebugger_CheckerSessionResult::NoActiveSession);

    auto Session = FCkTextureDebugger_CheckerOverrideSession{};
    const auto Apply = Session.Apply(Fixture.World.Get(), Checker.Get(),
        { Fixture.MakeTarget(Component, { 0 }) });
    TestTrue(TEXT("Checker A applies"), Apply.Succeeded());
    TestEqual(TEXT("Checker A is active"), Checker->K2_GetTextureParameterValue(CheckerTextureParameter), TextureA.Get());

    const auto InvalidTexture = Session.SwitchCheckerTexture(nullptr, CheckerTextureParameter);
    TestEqual(TEXT("Null texture is rejected"), InvalidTexture.Result,
        ECkTextureDebugger_CheckerSessionResult::InvalidCheckerTexture);
    TestEqual(TEXT("Rejected texture leaves checker A active"),
        Checker->K2_GetTextureParameterValue(CheckerTextureParameter), TextureA.Get());

    const auto InvalidParameter = Session.SwitchCheckerTexture(TextureB.Get(), TEXT("MissingCheckerTexture"));
    TestEqual(TEXT("Missing texture parameter is rejected"), InvalidParameter.Result,
        ECkTextureDebugger_CheckerSessionResult::InvalidCheckerParameter);
    TestEqual(TEXT("Rejected parameter leaves checker A active"),
        Checker->K2_GetTextureParameterValue(CheckerTextureParameter), TextureA.Get());

    const auto Switch = Session.SwitchCheckerTexture(TextureB.Get(), CheckerTextureParameter);
    TestTrue(TEXT("Checker switches directly from A to B"), Switch.Succeeded());
    TestEqual(TEXT("Checker B is active"), Checker->K2_GetTextureParameterValue(CheckerTextureParameter), TextureB.Get());
    TestEqual(TEXT("Switch keeps the same checker MID on the component"),
        Component->GetMaterial(0), static_cast<UMaterialInterface*>(Checker.Get()));

    const auto Restore = Session.TryRestore();
    TestTrue(TEXT("Restore succeeds after a live texture switch"), Restore.Succeeded());
    TestEqual(TEXT("Original material identity returns after the switch"),
        Component->GetMaterial(0), static_cast<UMaterialInterface*>(Original.Get()));

    TestTrue(TEXT("Non-MID checker applies for rejection fixture"),
        Session.Apply(Fixture.World.Get(), Fixture.BaseMaterial.Get(),
            { Fixture.MakeTarget(Component, { 0 }) }).Succeeded());
    const auto NonDynamic = Session.SwitchCheckerTexture(TextureA.Get(), CheckerTextureParameter);
    TestEqual(TEXT("Non-MID checker material is rejected"), NonDynamic.Result,
        ECkTextureDebugger_CheckerSessionResult::CheckerMaterialNotDynamic);
    TestEqual(TEXT("Rejected non-MID switch leaves the checker material unchanged"),
        Component->GetMaterial(0), Fixture.BaseMaterial.Get());
    TestTrue(TEXT("Original restores after non-MID rejection"), Session.TryRestore().Succeeded());

    Checker->SetTextureParameterValue(CheckerTextureParameter, TextureA.Get());
    TestTrue(TEXT("Checker reapplies for suspended-save rejection fixture"),
        Session.Apply(Fixture.World.Get(), Checker.Get(),
            { Fixture.MakeTarget(Component, { 0 }) }).Succeeded());
    TestTrue(TEXT("Session suspends for save"), Session.PrepareForWorldSave(Fixture.World.Get()).Succeeded());
    const auto Suspended = Session.SwitchCheckerTexture(TextureB.Get(), CheckerTextureParameter);
    TestEqual(TEXT("Texture switch is rejected while suspended for save"), Suspended.Result,
        ECkTextureDebugger_CheckerSessionResult::SessionSuspendedForWorldSave);
    TestEqual(TEXT("Suspended rejection leaves the original material in place"),
        Component->GetMaterial(0), static_cast<UMaterialInterface*>(Original.Get()));
    TestTrue(TEXT("Failed save completes without reapplying the checker"), Session.CompleteWorldSave(false).Succeeded());

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_CheckerSession_RestoreAndConflict,
    "Ck.TextureDebugger.CheckerSession.RestoreAndConflict",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_CheckerSession_RestoreAndConflict::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_checker_session_spec;

    auto Fixture = FFixture{};
    auto* Component = Fixture.MakeComponent();
    TestNotNull(TEXT("Registered static-mesh fixture"), Component);
    if (Component == nullptr)
    { return false; }

    auto Original = MakeMid(Fixture.BaseMaterial.Get());
    Component->SetMaterial(0, Original.Get());
    TestEqual(TEXT("Fixture begins with a one-entry override topology"), Component->OverrideMaterials.Num(), 1);

    auto Session = FCkTextureDebugger_CheckerOverrideSession{};
    const auto Apply = Session.Apply(Fixture.World.Get(), Fixture.Checker.Get(),
        { Fixture.MakeTarget(Component, { 0 }) });
    TestTrue(TEXT("Checker applies"), Apply.Succeeded());
    TestEqual(TEXT("Checker owns slot zero"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(Fixture.Checker.Get()));

    const auto Restore = Session.TryRestore();
    TestTrue(TEXT("Restore succeeds"), Restore.Succeeded());
    TestEqual(TEXT("Exact original override topology returns"), Component->OverrideMaterials.Num(), 1);
    TestEqual(TEXT("Original MID identity returns"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(Original.Get()));

    const auto Reapply = Session.Apply(Fixture.World.Get(), Fixture.Checker.Get(),
        { Fixture.MakeTarget(Component, { 0 }) });
    TestTrue(TEXT("Checker reapplies for conflict fixture"), Reapply.Succeeded());

    auto External = MakeMid(Fixture.BaseMaterial.Get());
    Component->SetMaterial(1, External.Get());

    const auto ConflictRestore = Session.TryRestore();
    TestTrue(TEXT("Conflict-preserving restore succeeds"), ConflictRestore.Succeeded());
    TestEqual(TEXT("Checker-owned slot restores"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(Original.Get()));
    TestEqual(TEXT("External later slot survives"), Component->GetMaterial(1), static_cast<UMaterialInterface*>(External.Get()));
    TestEqual(TEXT("External write is reported"), ConflictRestore.PreservedExternalSlotCount, 1);

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_CheckerSession_GcAndSave,
    "Ck.TextureDebugger.CheckerSession.GcAndSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_CheckerSession_GcAndSave::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_checker_session_spec;

    auto Fixture = FFixture{};
    auto* Component = Fixture.MakeComponent();
    TestNotNull(TEXT("Registered static-mesh fixture"), Component);
    if (Component == nullptr)
    { return false; }

    auto Original = MakeMid(Fixture.BaseMaterial.Get());
    auto OriginalWeak = TWeakObjectPtr<UMaterialInstanceDynamic>{Original.Get()};
    Component->SetMaterial(0, Original.Get());
    Original.Reset();

    auto Session = FCkTextureDebugger_CheckerOverrideSession{};
    TestTrue(TEXT("Checker applies before GC"), Session.Apply(Fixture.World.Get(), Fixture.Checker.Get(),
        { Fixture.MakeTarget(Component, { 0 }) }).Succeeded());

    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    TestTrue(TEXT("Ledger keeps removed original MID alive"), OriginalWeak.IsValid());

    const auto Prepare = Session.PrepareForWorldSave(Fixture.World.Get());
    TestTrue(TEXT("Prepare removes checker before save"), Prepare.Succeeded());
    TestEqual(TEXT("Prepare restored original MID"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(OriginalWeak.Get()));
    TestTrue(TEXT("Session is suspended for save"), Session.IsSuspendedForWorldSave());

    const auto Complete = Session.CompleteWorldSave(true);
    TestTrue(TEXT("Successful save reapplies checker"), Complete.Succeeded());
    TestEqual(TEXT("Checker reapplies after successful save"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(Fixture.Checker.Get()));

    TestTrue(TEXT("Checker restores before suspended-cleanup fixture"), Session.TryRestore().Succeeded());
    TestTrue(TEXT("Checker reapplies before suspended-cleanup fixture"), Session.Apply(
        Fixture.World.Get(), Fixture.Checker.Get(), { Fixture.MakeTarget(Component, { 0 }) }).Succeeded());
    TestTrue(TEXT("Second save suspension succeeds"), Session.PrepareForWorldSave(Fixture.World.Get()).Succeeded());
    TestEqual(TEXT("Suspended session still reports its world"), Session.GetWorld(), Fixture.World.Get());
    Session.ReleaseForWorldCleanup(Fixture.World.Get());
    TestFalse(TEXT("World cleanup releases suspended session"), Session.IsSuspendedForWorldSave());
    TestNull(TEXT("Released session no longer reports a world"), Session.GetWorld());
    TestEqual(TEXT("World cleanup leaves the already-restored original intact"), Component->GetMaterial(0), static_cast<UMaterialInterface*>(OriginalWeak.Get()));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_CheckerSession_DestroyAndReject,
    "Ck.TextureDebugger.CheckerSession.DestroyAndReject",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_CheckerSession_DestroyAndReject::RunTest(const FString& Parameters)
{
    using namespace ck_texture_debugger_checker_session_spec;

    auto Fixture = FFixture{};
    auto* ValidComponent = Fixture.MakeComponent();
    TestNotNull(TEXT("Registered static-mesh fixture"), ValidComponent);
    if (ValidComponent == nullptr)
    { return false; }

    auto Original = MakeMid(Fixture.BaseMaterial.Get());
    ValidComponent->SetMaterial(0, Original.Get());

    auto* InvalidComponent = NewObject<UStaticMeshComponent>(GetTransientPackage());
    InvalidComponent->SetStaticMesh(Fixture.Mesh.Get());

    auto Session = FCkTextureDebugger_CheckerOverrideSession{};
    const auto Rejected = Session.Apply(Fixture.World.Get(), Fixture.Checker.Get(),
        { Fixture.MakeTarget(ValidComponent, { 0 }), Fixture.MakeTarget(InvalidComponent, { 0 }) });
    TestFalse(TEXT("Unregistered target rejects whole batch"), Rejected.Succeeded());
    TestFalse(TEXT("Rejected batch publishes no session"), Session.HasActiveSession());
    TestEqual(TEXT("Valid earlier target was not partially changed"),
        ValidComponent->GetMaterial(0), static_cast<UMaterialInterface*>(Original.Get()));

    TestTrue(TEXT("Single valid target applies"), Session.Apply(Fixture.World.Get(), Fixture.Checker.Get(),
        { Fixture.MakeTarget(ValidComponent, { 0 }) }).Succeeded());

    auto WeakComponent = TWeakObjectPtr<UStaticMeshComponent>{ValidComponent};
    ValidComponent->DestroyComponent();
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

    TestFalse(TEXT("Destroyed component weak target is invalid"), WeakComponent.IsValid());
    TestEqual(TEXT("Destroyed component ledger is discarded without dereference"), Session.DiscardDestroyedComponents(), 1);
    TestFalse(TEXT("Discarding final destroyed component ends the session"), Session.HasActiveSession());

    return true;
}

#endif
