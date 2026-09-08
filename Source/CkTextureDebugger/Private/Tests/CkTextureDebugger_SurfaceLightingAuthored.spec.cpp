#include "CkTextureDebugger/Data/CkTextureDebugger_LoadedWorldCollector.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"

#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/CkFlexText.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_surface_lighting_authored_tests
{
    struct FFixture
    {
        FFixture()
        {
            World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(EWorldType::Game, false)};
            Mesh = TStrongObjectPtr<UStaticMesh>{NewObject<UStaticMesh>(GetTransientPackage())};
            Checker = TStrongObjectPtr<UMaterialInterface>{LoadObject<UMaterialInterface>(nullptr, TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker"))};
            Mesh->GetStaticMaterials().Add(FStaticMaterial{Checker.Get()});
            Mesh->GetStaticMaterials().Add(FStaticMaterial{});
            Mesh->GetStaticMaterials().Add(FStaticMaterial{Checker.Get()});
        }

        ~FFixture()
        {
            if (World.IsValid()) { World->DestroyWorld(false); }
        }

        auto MakeComponent() -> UStaticMeshComponent*
        {
            auto* Actor = World->SpawnActor<AActor>();
            if (Actor == nullptr) { return nullptr; }
            auto* Component = NewObject<UStaticMeshComponent>(Actor);
            Actor->SetRootComponent(Component);
            Actor->AddInstanceComponent(Component);
            Component->SetStaticMesh(Mesh.Get());
            Component->RegisterComponent();
            return Component;
        }

        TStrongObjectPtr<UWorld> World;
        TStrongObjectPtr<UStaticMesh> Mesh;
        TStrongObjectPtr<UMaterialInterface> Checker;
    };

    auto Find_WidgetByTag(const TSharedRef<SWidget>& InWidget, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InWidget->GetTag() == InTag) { return InWidget; }
        const auto* Children = InWidget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SWidget> Found = Find_WidgetByTag(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto Contains_Text(const TSharedRef<SWidget>& InWidget, const FString& InText) -> bool
    {
        if (InWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InWidget)->GetText().ToString().Contains(InText)) { return true; }
        if (InWidget->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InWidget)->GetText().ToString().Contains(InText))
        {
            return true;
        }
        const auto* Children = InWidget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (Contains_Text(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto Contains_IdenticalText(const TSharedRef<SWidget>& InWidget, const FText& InText) -> bool
    {
        if (InWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InWidget)->GetText().IdenticalTo(InText)) { return true; }
        if (InWidget->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InWidget)->GetText().IdenticalTo(InText))
        {
            return true;
        }
        const auto* Children = InWidget->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (Contains_IdenticalText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText)) { return true; }
        }
        return false;
    }

    auto Find_ComponentRow(const FCkTextureDebugger_LoadedWorldSnapshot& InSnapshot, UStaticMeshComponent* InComponent) -> const FCkTextureDebugger_ComponentRow*
    {
        return InSnapshot.Components.FindByPredicate([InComponent](const FCkTextureDebugger_ComponentRow& Value)
        {
            return Value.NavigationTarget.Get() == InComponent;
        });
    }

    auto Make_SlotKey(const FCkTextureDebugger_MaterialSlotRow& InSlot) -> FString
    {
        return FString::Printf(TEXT("%d|%u|%s|%s"),
            InSlot.SlotIndex,
            GetTypeHash(FObjectKey{InSlot.NavigationTarget.Get()}),
            *InSlot.MaterialPath.ToString(),
            *InSlot.DisplayName);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_SurfaceLighting_Authored,
    "Ck.TextureDebugger.SurfaceLighting.Authored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_SurfaceLighting_Authored::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_surface_lighting_authored_tests;

    FFixture Fixture{};
    if (!TestNotNull(TEXT("Surface Lighting fixture loads the installed checker material"), Fixture.Checker.Get())) { return false; }
    UStaticMeshComponent* const Component = Fixture.MakeComponent();
    if (!TestNotNull(TEXT("Surface Lighting fixture creates a loaded static-mesh component"), Component)) { return false; }

    const FCkTextureDebugger_LoadedWorldSnapshot Snapshot = ck::texture_debugger::collector::Collect_LoadedWorld(Fixture.World.Get());
    const FCkTextureDebugger_ComponentRow* const Row = Find_ComponentRow(Snapshot, Component);
    if (!TestNotNull(TEXT("Production collector publishes the fixture component"), Row)
        || !TestEqual(TEXT("Production snapshot retains all authored material slots"), Row->MaterialSlots.Num(), 3)
        || !TestTrue(TEXT("Production snapshot retains the first checker material slot"), Row->MaterialSlots[0].NavigationTarget.Get() == Fixture.Checker.Get())
        || !TestFalse(TEXT("Production snapshot retains the explicit empty middle slot"), Row->MaterialSlots[1].NavigationTarget.IsValid()))
    {
        return false;
    }

    const TSharedRef<SCkTextureDebugger_SurfaceLightingPage> Page = SNew(SCkTextureDebugger_SurfaceLightingPage);
    Page->SlatePrepass();
    if (!TestTrue(TEXT("Surface Lighting loads its installed authored layout"), Page->Get_LayoutRevision() > 0 && Page->Get_LayoutError().IsEmpty()))
    {
        AddError(Page->Get_LayoutError().ToString());
        return false;
    }
    TestTrue(TEXT("Surface Lighting purpose preserves its localized identity"), Contains_IdenticalText(Page,
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "SurfacePurpose",
            "Reports public runtime material and component facts for the selected slots. These facts do not diagnose Lumen, VSM, light leaks, blurry textures, or final rendered appearance.")));

    Page->Set_Context(*Row, {}, {0, 1, 2});
    const FString Slot0Key = Make_SlotKey(Row->MaterialSlots[0]);
    const FString Slot1Key = Make_SlotKey(Row->MaterialSlots[1]);
    const FString Slot2Key = Make_SlotKey(Row->MaterialSlots[2]);
    const TSharedPtr<SCkUiRepeat> Repeat = Page->Get_AuthoredRepeat();
    if (!TestTrue(TEXT("Surface Lighting refreshes its repeat after the production snapshot"), Repeat.IsValid() && Repeat->TryRefresh())) { return false; }
    Page->SlatePrepass();
    if (!TestTrue(TEXT("Surface Lighting exposes its authored repeat"), Repeat->GetItemCount() == 3)) { return false; }

    const TSharedPtr<SWidget> Slot0Root = Repeat->GetItemWidget(Slot0Key);
    const TSharedPtr<SWidget> Slot1Root = Repeat->GetItemWidget(Slot1Key);
    const TSharedPtr<SWidget> Slot2Root = Repeat->GetItemWidget(Slot2Key);
    if (!TestNotNull(TEXT("Surface Lighting exposes the first retained slot root"), Slot0Root.Get())
        || !TestNotNull(TEXT("Surface Lighting exposes the empty-slot root"), Slot1Root.Get())
        || !TestNotNull(TEXT("Surface Lighting exposes the third retained slot root"), Slot2Root.Get()))
    {
        return false;
    }
    TestTrue(TEXT("Surface Lighting projects the installed material name"), Contains_Text(Slot0Root.ToSharedRef(), Fixture.Checker->GetName()));
    TestTrue(TEXT("Surface Lighting projects explicit empty-material truthfully"), Contains_Text(Slot1Root.ToSharedRef(), TEXT("(empty)")));

    const TSharedPtr<SWidget> Slot0ToggleWidget = Find_WidgetByTag(Slot0Root.ToSharedRef(), TEXT("slot-toggle"));
    const TSharedPtr<SWidget> Slot1ToggleWidget = Find_WidgetByTag(Slot1Root.ToSharedRef(), TEXT("slot-toggle"));
    const TSharedPtr<SWidget> Slot0Details = Find_WidgetByTag(Slot0Root.ToSharedRef(), TEXT("slot-details"));
    const TSharedPtr<SWidget> Slot1Details = Find_WidgetByTag(Slot1Root.ToSharedRef(), TEXT("slot-details"));
    const TSharedPtr<SWidget> CastShadowValue = Find_WidgetByTag(Slot0Root.ToSharedRef(), TEXT("cast-shadow"));
    if (!TestEqual(TEXT("Surface Lighting slot zero heading is a native authored button"), Slot0ToggleWidget.IsValid() ? Slot0ToggleWidget->GetTypeAsString() : FString{}, FString{TEXT("SButton")})
        || !TestEqual(TEXT("Surface Lighting slot one heading is a native authored button"), Slot1ToggleWidget.IsValid() ? Slot1ToggleWidget->GetTypeAsString() : FString{}, FString{TEXT("SButton")})
        || !TestNotNull(TEXT("Surface Lighting slot zero mounts authored details"), Slot0Details.Get())
        || !TestNotNull(TEXT("Surface Lighting slot one mounts authored details"), Slot1Details.Get())
        || !TestNotNull(TEXT("Surface Lighting slot zero mounts its native cast-shadow value"), CastShadowValue.Get()))
    {
        return false;
    }

    StaticCastSharedPtr<SButton>(Slot0ToggleWidget)->SimulateClick();
    Repeat->TryRefresh();
    Page->SlatePrepass();
    const TSharedPtr<SWidget> FirstToggleSlot0Root = Repeat->GetItemWidget(Slot0Key);
    const TSharedPtr<SWidget> FirstToggleSlot1Root = Repeat->GetItemWidget(Slot1Key);
    const TSharedPtr<SWidget> FirstToggleSlot0Details = FirstToggleSlot0Root.IsValid() ? Find_WidgetByTag(FirstToggleSlot0Root.ToSharedRef(), TEXT("slot-details")) : nullptr;
    const TSharedPtr<SWidget> FirstToggleSlot1Details = FirstToggleSlot1Root.IsValid() ? Find_WidgetByTag(FirstToggleSlot1Root.ToSharedRef(), TEXT("slot-details")) : nullptr;
    TestTrue(TEXT("Surface Lighting preserves both slot roots after the first independent toggle"), FirstToggleSlot0Root == Slot0Root && FirstToggleSlot1Root == Slot1Root);
    TestEqual(TEXT("Surface Lighting slot zero heading collapses only its own details"), FirstToggleSlot0Details.IsValid() ? FirstToggleSlot0Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Collapsed);
    TestEqual(TEXT("Surface Lighting slot one remains independently expanded"), FirstToggleSlot1Details.IsValid() ? FirstToggleSlot1Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Visible);
    const TSharedPtr<SWidget> FirstToggleSlot1Button = FirstToggleSlot1Root.IsValid() ? Find_WidgetByTag(FirstToggleSlot1Root.ToSharedRef(), TEXT("slot-toggle")) : nullptr;
    if (!TestEqual(TEXT("Surface Lighting keeps slot one heading actionable after slot zero toggles"), FirstToggleSlot1Button.IsValid() ? FirstToggleSlot1Button->GetTypeAsString() : FString{}, FString{TEXT("SButton")})) { return false; }
    StaticCastSharedPtr<SButton>(FirstToggleSlot1Button)->SimulateClick();
    Repeat->TryRefresh();
    Page->SlatePrepass();
    const TSharedPtr<SWidget> SecondToggleSlot0Root = Repeat->GetItemWidget(Slot0Key);
    const TSharedPtr<SWidget> SecondToggleSlot1Root = Repeat->GetItemWidget(Slot1Key);
    const TSharedPtr<SWidget> SecondToggleSlot1Details = SecondToggleSlot1Root.IsValid() ? Find_WidgetByTag(SecondToggleSlot1Root.ToSharedRef(), TEXT("slot-details")) : nullptr;
    TestEqual(TEXT("Surface Lighting slot one heading collapses its own details"), SecondToggleSlot1Details.IsValid() ? SecondToggleSlot1Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Collapsed);
    const TSharedPtr<SWidget> SecondToggleSlot0Button = SecondToggleSlot0Root.IsValid() ? Find_WidgetByTag(SecondToggleSlot0Root.ToSharedRef(), TEXT("slot-toggle")) : nullptr;
    if (!TestEqual(TEXT("Surface Lighting keeps slot zero heading actionable after slot one toggles"), SecondToggleSlot0Button.IsValid() ? SecondToggleSlot0Button->GetTypeAsString() : FString{}, FString{TEXT("SButton")})) { return false; }
    StaticCastSharedPtr<SButton>(SecondToggleSlot0Button)->SimulateClick();
    Repeat->TryRefresh();
    Page->SlatePrepass();
    const TSharedPtr<SWidget> ThirdToggleSlot0Root = Repeat->GetItemWidget(Slot0Key);
    const TSharedPtr<SWidget> ThirdToggleSlot1Root = Repeat->GetItemWidget(Slot1Key);
    const TSharedPtr<SWidget> ThirdToggleSlot0Details = ThirdToggleSlot0Root.IsValid() ? Find_WidgetByTag(ThirdToggleSlot0Root.ToSharedRef(), TEXT("slot-details")) : nullptr;
    const TSharedPtr<SWidget> ThirdToggleSlot1Details = ThirdToggleSlot1Root.IsValid() ? Find_WidgetByTag(ThirdToggleSlot1Root.ToSharedRef(), TEXT("slot-details")) : nullptr;
    TestEqual(TEXT("Surface Lighting slot zero reopens independently"), ThirdToggleSlot0Details.IsValid() ? ThirdToggleSlot0Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Visible);
    TestEqual(TEXT("Surface Lighting slot one remains collapsed while slot zero reopens"), ThirdToggleSlot1Details.IsValid() ? ThirdToggleSlot1Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Collapsed);

    Component->SetCastShadow(false);
    const FCkTextureDebugger_LoadedWorldSnapshot RefreshedSnapshot = ck::texture_debugger::collector::Collect_LoadedWorld(Fixture.World.Get());
    const FCkTextureDebugger_ComponentRow* const RefreshedRow = Find_ComponentRow(RefreshedSnapshot, Component);
    if (!TestNotNull(TEXT("Production collector refreshes the modified component fact"), RefreshedRow)) { return false; }
    const TSharedPtr<SWidget> Slot0RootBeforeValueRefresh = Repeat->GetItemWidget(Slot0Key);
    const TSharedPtr<SWidget> Slot1RootBeforeValueRefresh = Repeat->GetItemWidget(Slot1Key);
    if (!TestNotNull(TEXT("Surface Lighting keeps slot zero mounted before its value refresh"), Slot0RootBeforeValueRefresh.Get())
        || !TestNotNull(TEXT("Surface Lighting keeps slot one mounted before its value refresh"), Slot1RootBeforeValueRefresh.Get()))
    {
        return false;
    }
    Page->Set_Context(*RefreshedRow, {}, {0, 1, 2});
    Repeat->TryRefresh();
    Page->SlatePrepass();
    TestTrue(TEXT("Surface Lighting retains the stable slot zero root across value refresh"), Repeat->GetItemWidget(Slot0Key) == Slot0RootBeforeValueRefresh);
    TestTrue(TEXT("Surface Lighting retains the stable empty-slot root across value refresh"), Repeat->GetItemWidget(Slot1Key) == Slot1RootBeforeValueRefresh);
    const TSharedPtr<SWidget> UpdatedCastShadowValue = Find_WidgetByTag(Slot0RootBeforeValueRefresh.ToSharedRef(), TEXT("cast-shadow"));
    TestTrue(TEXT("Surface Lighting refreshes the changed public shadow fact"), UpdatedCastShadowValue.IsValid() && Contains_IdenticalText(UpdatedCastShadowValue.ToSharedRef(),
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "Disabled", "NO")));
    const TSharedPtr<SWidget> RefreshedSlot1Details = Find_WidgetByTag(Slot1RootBeforeValueRefresh.ToSharedRef(), TEXT("slot-details"));
    TestEqual(TEXT("Surface Lighting slot one collapse survives a value refresh"), RefreshedSlot1Details.IsValid() ? RefreshedSlot1Details->GetVisibility() : EVisibility::HitTestInvisible, EVisibility::Collapsed);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Surface Lighting resolves the installed debugger UI resources"), Plugin.IsValid())) { return false; }
    FString Markup;
    FString Stylesheet;
    const FString UiDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    if (!TestTrue(TEXT("Surface Lighting reads its installed markup"), FFileHelper::LoadFileToString(Markup, *FPaths::Combine(UiDirectory, TEXT("SurfaceLighting.ui.html"))))
        || !TestTrue(TEXT("Surface Lighting reads its installed stylesheet"), FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(UiDirectory, TEXT("SurfaceLighting.ui.css")))))
    {
        return false;
    }
    const int64 RevisionBeforeReload = Page->Get_LayoutRevision();
    if (!TestTrue(TEXT("Surface Lighting reloads its installed layout"), Page->TryReload_Layout(Markup, Stylesheet).Succeeded))
    {
        AddError(Page->Get_LayoutError().ToString());
        return false;
    }
    TestEqual(TEXT("Surface Lighting accepted reload advances revision once"), Page->Get_LayoutRevision(), RevisionBeforeReload + 1);
    const TSharedPtr<SCkUiRepeat> ReloadedRepeat = Page->Get_AuthoredRepeat();
    if (!TestTrue(TEXT("Surface Lighting reload remounts the authored repeat"), ReloadedRepeat.IsValid() && ReloadedRepeat->GetItemCount() == 3)) { return false; }
    const TSharedPtr<SWidget> ReloadedSlot1Root = ReloadedRepeat->GetItemWidget(Slot1Key);
    const int64 RevisionBeforeRejectedReload = Page->Get_LayoutRevision();
    TestFalse(TEXT("Surface Lighting rejects an unknown collection binding atomically"), Page->TryReload_Layout(Markup.Replace(TEXT("surface-lighting\""), TEXT("missing\"")), Stylesheet).Succeeded);
    TestEqual(TEXT("Surface Lighting rejected reload retains revision"), Page->Get_LayoutRevision(), RevisionBeforeRejectedReload);
    TestFalse(TEXT("Surface Lighting rejected reload exposes a designer diagnostic"), Page->Get_LayoutError().IsEmpty());
    TestTrue(TEXT("Surface Lighting rejected reload preserves retained slot roots"), Page->Get_AuthoredRepeat() == ReloadedRepeat && ReloadedRepeat->GetItemWidget(Slot1Key) == ReloadedSlot1Root);
    TestFalse(TEXT("Surface Lighting steady authored files need no reload"), Page->Poll_LayoutFiles());

    Page->Set_Context({}, {}, {});
    Page->Get_AuthoredRepeat()->TryRefresh();
    Page->SlatePrepass();
    TestTrue(TEXT("Surface Lighting renders the real empty component state"), Page->Get_AuthoredRepeat()->GetItemCount() == 0 && Contains_IdenticalText(Page,
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "SurfaceNoComponent", "Select a component to inspect surface and lighting facts.")));
    return true;
}

#endif
