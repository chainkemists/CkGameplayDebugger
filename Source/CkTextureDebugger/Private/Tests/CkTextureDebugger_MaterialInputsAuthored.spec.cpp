#include "CkTextureDebugger/Data/CkTextureDebugger_LoadedWorldCollector.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiTable.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SHeaderRow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_material_inputs_authored_tests
{
    using FAuthoredList = SListView<SCkUiTable::FRecord>;
    struct FFixture
    {
        FFixture()
        {
            World = TStrongObjectPtr<UWorld>{UWorld::CreateWorld(EWorldType::Game, false)};
            Mesh = TStrongObjectPtr<UStaticMesh>{NewObject<UStaticMesh>(GetTransientPackage())};
            Checker = TStrongObjectPtr<UMaterialInterface>{LoadObject<UMaterialInterface>(nullptr, TEXT("/CkDebugger/TextureDebugger/Materials/M_CkTextureChecker.M_CkTextureChecker"))};
            Mesh->GetStaticMaterials().Add(FStaticMaterial{});
            Mesh->GetStaticMaterials().Add(FStaticMaterial{});
            Mesh->GetStaticMaterials().Add(FStaticMaterial{Checker.Get()});
        }

        ~FFixture() { if (World.IsValid()) { World->DestroyWorld(false); } }

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

    auto Get_TextField(const SCkUiTable::FRecord& InRecord, const FString& InName) -> FString
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InName) : nullptr;
        return Field != nullptr ? Field->Text.ToString() : FString{};
    }

    auto Get_ColorField(const SCkUiTable::FRecord& InRecord, const FString& InName) -> FLinearColor
    {
        const FCkUiFieldValue* Field = InRecord.IsValid() ? InRecord->FindField(InName) : nullptr;
        return Field != nullptr ? Field->Color : FLinearColor::Transparent;
    }

    auto Find_SearchBoxes(const TSharedRef<SWidget>& InWidget, TArray<TSharedPtr<SSearchBox>>& OutBoxes) -> void
    {
        if (InWidget->GetTypeAsString() == TEXT("SSearchBox")) { OutBoxes.Add(StaticCastSharedRef<SSearchBox>(InWidget)); }
        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index) { Find_SearchBoxes(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutBoxes); }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkTextureDebugger_MaterialInputs_Authored,
    "Ck.TextureDebugger.MaterialInputs.Authored", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_MaterialInputs_Authored::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_material_inputs_authored_tests;
    auto Fixture = FFixture{};
    if (!TestNotNull(TEXT("Material Inputs fixture loads the installed checker material"), Fixture.Checker.Get())) { return false; }
    const auto Component = Fixture.MakeComponent();
    if (!TestNotNull(TEXT("Material Inputs fixture creates a loaded component"), Component)) { return false; }
    const FCkTextureDebugger_LoadedWorldSnapshot Snapshot = ck::texture_debugger::collector::Collect_LoadedWorld(Fixture.World.Get());
    const FCkTextureDebugger_ComponentRow* Row = Snapshot.Components.FindByPredicate([Component](const FCkTextureDebugger_ComponentRow& Value) { return Value.NavigationTarget.Get() == Component; });
    if (!TestNotNull(TEXT("Production collector publishes the fixture component"), Row) || !TestTrue(TEXT("Production collector preserves both null and installed-checker slots"), Row->MaterialSlots.Num() == 3) || !TestTrue(TEXT("Production collector retains the installed checker material"), Row->MaterialSlots[2].NavigationTarget.Get() == Fixture.Checker.Get())) { return false; }

    const TSharedRef<SCkTextureDebugger_MaterialInputsPage> Page = SNew(SCkTextureDebugger_MaterialInputsPage);
    Page->Set_Context(*Row, {}, {Row->MaterialSlots[0].SlotIndex, Row->MaterialSlots[1].SlotIndex, Row->MaterialSlots[2].SlotIndex});
    const TSharedPtr<SCkUiTable> Table = Page->Get_AuthoredTable();
    if (!TestTrue(TEXT("Material Inputs parses its installed authored layout"), Page->Get_LayoutRevision() > 0 && Page->Get_LayoutError().IsEmpty() && Table.IsValid())) { AddError(Page->Get_LayoutError().ToString()); return false; }
    Table->TryRefresh();
    const TSharedPtr<FAuthoredList> List = Table->GetList();
    if (!TestTrue(TEXT("Material Inputs projects the two explicit null-material analysis rows"), List.IsValid() && List->GetItems().Num() >= 2)) { return false; }
    const auto Header = List->GetHeaderRow();
    if (!TestTrue(TEXT("Material Inputs exposes all five native headers"), Header.IsValid() && Header->GetColumns().Num() == 5)) { return false; }
    const TArray<FText> ExpectedHeaders{
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "ParameterColumn", "Parameter"),
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "TextureColumn", "Texture"),
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "ProvenanceColumn", "Provenance"),
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "SlotColumn", "Slot"),
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "VariantColumn", "Quality / platform")};
    for (int32 Index = 0; Index < ExpectedHeaders.Num(); ++Index)
    {
        TestTrue(TEXT("Native header retains its original localized FText"), Header->GetColumns()[Index].DefaultText.Get().IdenticalTo(ExpectedHeaders[Index]));
    }
    const auto DetailItem = List->GetItems()[0];
    TestEqual(TEXT("Material Inputs projects unavailable provenance truthfully"), Get_TextField(DetailItem, TEXT("provenance")), FString{TEXT("Unavailable")});
    TestEqual(TEXT("Material Inputs preserves the unavailable analysis reason"), Get_TextField(DetailItem, TEXT("detail")), FString{TEXT("No material is resolved for this slot.")});
    TestEqual(TEXT("Material Inputs table disables row selection"), List->Private_GetSelectionMode(), ESelectionMode::None);

    auto Searches = TArray<TSharedPtr<SSearchBox>>{};
    Find_SearchBoxes(Page, Searches);
    if (!TestEqual(TEXT("Material Inputs exposes two real authored search controls"), Searches.Num(), 2)) { return false; }
    TestTrue(TEXT("Filter hint retains its original localized FText"), Searches[0]->GetHintText().IdenticalTo(
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "MaterialFilter", "Filter parameter, texture, slot, or provenance…")));
    TestTrue(TEXT("Highlight hint retains its original localized FText"), Searches[1]->GetHintText().IdenticalTo(
        NSLOCTEXT("SCkTextureDebugger_DiagnosticPages", "MaterialHighlight", "Highlight matches…")));
    Searches[0]->SetText(FText::FromString(TEXT("Unavailable")));
    Table->TryRefresh();
    TestTrue(TEXT("Filter retains matching unavailable records"), List->GetItems().Num() >= 2);
    Searches[0]->SetText(FText::FromString(TEXT("no-match")));
    Table->TryRefresh();
    TestEqual(TEXT("Filter removes nonmatching rows"), List->GetItems().Num(), 0);
    Searches[0]->SetText(FText::GetEmpty());
    Table->TryRefresh();
    if (!TestTrue(TEXT("Filter reset rebuilds the explicit null-material rows"), List->GetItems().Num() >= 2)) { return false; }
    const auto HighlightedItem = List->GetItems()[0];
    const auto DimmedItem = List->GetItems()[1];
    Searches[1]->SetText(FText::FromString(Get_TextField(HighlightedItem, TEXT("slot"))));
    Table->TryRefresh();
    TestTrue(TEXT("Highlight keeps all rows visible"), List->GetItems().Contains(HighlightedItem) && List->GetItems().Contains(DimmedItem));
    TestTrue(TEXT("Highlight preserves matching text color"), Get_ColorField(HighlightedItem, TEXT("text-color")).Equals(CkStyle::Accent()));
    TestTrue(TEXT("Highlight dims nonmatching text color"), Get_ColorField(DimmedItem, TEXT("text-color")).Equals(CkStyle::TextMute()));
    Searches[1]->SetText(FText::GetEmpty());
    Table->TryRefresh();
    const auto StableItem = List->GetItems()[0];
    Page->Set_Context(*Row, {}, {Row->MaterialSlots[0].SlotIndex, Row->MaterialSlots[1].SlotIndex, Row->MaterialSlots[2].SlotIndex});
    Table->TryRefresh();
    TestTrue(TEXT("Stable material analysis key retains the authored record identity"), List->GetItems().Contains(StableItem));

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    if (!Plugin.IsValid() || !FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/MaterialInputs.ui.html"))) || !FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI/MaterialInputs.ui.css")))) { AddError(TEXT("Material Inputs installed resources are unavailable")); return false; }
    const int64 Revision = Page->Get_LayoutRevision();
    TestTrue(TEXT("Material Inputs reload preserves authored state"), Page->TryReload_Layout(Markup, Stylesheet).Succeeded);
    TestEqual(TEXT("Material Inputs successful reload advances revision once"), Page->Get_LayoutRevision(), Revision + 1);
    Page->Get_AuthoredTable()->TryRefresh();
    TestTrue(TEXT("Material Inputs reload retains its stable projected record"), Page->Get_AuthoredTable()->GetList()->GetItems().Contains(StableItem));
    TestFalse(TEXT("Material Inputs rejects an unknown collection binding atomically"), Page->TryReload_Layout(Markup.Replace(TEXT("material-inputs"), TEXT("missing")), Stylesheet).Succeeded);
    return true;
}

#endif
