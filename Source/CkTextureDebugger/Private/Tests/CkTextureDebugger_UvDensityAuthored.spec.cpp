#include "CkTextureDebugger/Window/SCkTextureDebugger_DiagnosticPages.h"

#include "CkSlateLayout/CkFlexText.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_uv_density_authored_tests
{
    auto Contains_Text(
        const TSharedRef<SWidget>& InWidget,
        const FString& InText)
        -> bool
    {
        if (InWidget->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InWidget)->GetText().ToString().Contains(InText))
        {
            return true;
        }

        if (InWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && StaticCastSharedRef<SCkFlexText>(InWidget)->GetText().ToString().Contains(InText))
        {
            return true;
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            if (Contains_Text(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            {
                return true;
            }
        }
        return false;
    }

    auto Find_WidgetByTag(
        const TSharedRef<SWidget>& InWidget,
        const FName InTag)
        -> TSharedPtr<SWidget>
    {
        if (InWidget->GetTag() == InTag)
        {
            return InWidget;
        }

        const auto* Children = InWidget->GetChildren();
        for (auto Index = 0; Index < Children->Num(); ++Index)
        {
            const TSharedPtr<SWidget> Found = Find_WidgetByTag(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag);
            if (Found.IsValid())
            {
                return Found;
            }
        }
        return {};
    }

    auto Make_ComponentRow(UStaticMeshComponent* InComponent) -> FCkTextureDebugger_ComponentRow
    {
        auto Result = FCkTextureDebugger_ComponentRow{};
        Result.NavigationTarget = InComponent;
        Result.ActorDisplayName = TEXT("DensityActor");
        Result.ComponentDisplayName = TEXT("DensityComponent");
        return Result;
    }

    auto Make_Selection(
        UStaticMeshComponent* InComponent,
        UTexture2D* InTexture,
        const int32 InSlotIndex)
        -> FCkTextureDebugger_TextureHealthSelection
    {
        auto Result = FCkTextureDebugger_TextureHealthSelection{};
        Result.Component = InComponent;
        Result.Texture = InTexture;
        Result.SlotIndex = InSlotIndex;
        Result.DisplayName = TEXT("DensityTexture");
        Result.Health.CookedWidth = 1024;
        Result.Health.CookedHeight = 1024;
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_UvDensity_Authored,
    "Ck.TextureDebugger.UvDensity.Authored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkTextureDebugger_UvDensity_Authored::RunTest(const FString&) -> bool
{
    using namespace ck_texture_debugger_uv_density_authored_tests;

    const TSharedRef<SCkTextureDebugger_UvDensityPage> Page = SNew(SCkTextureDebugger_UvDensityPage);
    Page->SlatePrepass();
    if (NOT TestTrue(TEXT("UV & Density loads its installed authored document"), Page->Get_LayoutRevision() > 0))
    {
        AddError(TEXT("UV & Density layout failed to load: ") + Page->Get_LayoutError().ToString());
        return false;
    }
    TestTrue(TEXT("UV & Density installed authored document has no diagnostics"), Page->Get_LayoutError().IsEmpty());
    TestTrue(TEXT("UV & Density exposes its authored purpose"), Contains_Text(Page, TEXT("Measures texels per centimetre only")));

    Page->Set_Context({}, {}, {});
    Page->SlatePrepass();
    TestTrue(TEXT("UV & Density renders the real empty component context"), Contains_Text(Page, TEXT("No component")));
    TestTrue(TEXT("UV & Density renders the real empty result reason"), Contains_Text(Page, TEXT("No component is selected.")));

    const auto Component = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    const auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    Page->Set_Context(Make_ComponentRow(Component.Get()), Make_Selection(Component.Get(), Texture.Get(), 0), {0});
    Page->SlatePrepass();
    TestTrue(TEXT("UV & Density binds the selected component context"), Contains_Text(Page, TEXT("DensityActor · DensityComponent")));
    TestTrue(TEXT("UV & Density binds the selected texture context"), Contains_Text(Page, TEXT("DensityTexture")));
    TestTrue(TEXT("UV & Density keeps the capability result authoritative"), Contains_Text(Page, TEXT("MISSING PREREQUISITE")));

    Page->Set_Context(Make_ComponentRow(Component.Get()), Make_Selection(Component.Get(), Texture.Get(), 1), {0});
    Page->SlatePrepass();
    TestTrue(TEXT("UV & Density preserves the material-slot binding proof"), Contains_Text(Page, TEXT("different component or material slot")));

    const TSharedPtr<SWidget> InputsBody = Find_WidgetByTag(Page, TEXT("inputs-body"));
    const TSharedPtr<SWidget> ResultBody = Find_WidgetByTag(Page, TEXT("result-body"));
    const TSharedPtr<SWidget> InputsToggleWidget = Find_WidgetByTag(Page, TEXT("inputs-toggle"));
    const TSharedPtr<SWidget> ResultToggleWidget = Find_WidgetByTag(Page, TEXT("result-toggle"));
    if (NOT TestNotNull(TEXT("UV & Density mounts its inputs body"), InputsBody.Get())
        || NOT TestNotNull(TEXT("UV & Density mounts its result body"), ResultBody.Get())
        || NOT TestNotNull(TEXT("UV & Density mounts its inputs toggle"), InputsToggleWidget.Get())
        || NOT TestNotNull(TEXT("UV & Density mounts its result toggle"), ResultToggleWidget.Get()))
    {
        return false;
    }

    if (NOT TestEqual(TEXT("UV & Density inputs toggle is an authored Slate button"), InputsToggleWidget->GetTypeAsString(), FString{TEXT("SButton")})
        || NOT TestEqual(TEXT("UV & Density result toggle is an authored Slate button"), ResultToggleWidget->GetTypeAsString(), FString{TEXT("SButton")}))
    {
        return false;
    }
    const TSharedPtr<SButton> InputsToggle = StaticCastSharedPtr<SButton>(InputsToggleWidget);
    const TSharedPtr<SButton> ResultToggle = StaticCastSharedPtr<SButton>(ResultToggleWidget);

    InputsToggle->SimulateClick();
    Page->SlatePrepass();
    TestEqual(TEXT("UV & Density inputs toggle collapses only the inputs body"), InputsBody->GetVisibility(), EVisibility::Collapsed);
    TestEqual(TEXT("UV & Density inputs toggle leaves the result body expanded"), ResultBody->GetVisibility(), EVisibility::Visible);
    ResultToggle->SimulateClick();
    Page->SlatePrepass();
    TestEqual(TEXT("UV & Density result toggle collapses only the result body"), ResultBody->GetVisibility(), EVisibility::Collapsed);
    Page->Set_Context(Make_ComponentRow(Component.Get()), Make_Selection(Component.Get(), Texture.Get(), 0), {0});
    Page->SlatePrepass();
    TestEqual(TEXT("UV & Density inputs collapse survives context refresh"), InputsBody->GetVisibility(), EVisibility::Collapsed);
    TestEqual(TEXT("UV & Density result collapse survives context refresh"), ResultBody->GetVisibility(), EVisibility::Collapsed);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT TestTrue(TEXT("UV & Density test resolves the installed debugger resource directory"), Plugin.IsValid()))
    {
        return false;
    }

    const FString UiDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    auto Markup = FString{};
    auto Stylesheet = FString{};
    const int64 RevisionBeforeReload = Page->Get_LayoutRevision();
    if (NOT TestTrue(TEXT("UV & Density test reads its installed markup"), FFileHelper::LoadFileToString(Markup, *FPaths::Combine(UiDirectory, TEXT("UvDensity.ui.html"))))
        || NOT TestTrue(TEXT("UV & Density test reads its installed stylesheet"), FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(UiDirectory, TEXT("UvDensity.ui.css"))))
        || NOT TestTrue(TEXT("UV & Density reloads its installed layout without resetting state"), Page->TryReload_Layout(Markup, Stylesheet).Succeeded))
    {
        return false;
    }
    TestEqual(TEXT("UV & Density accepted layout reload advances the revision"), Page->Get_LayoutRevision(), RevisionBeforeReload + 1);
    Page->SlatePrepass();

    const TSharedPtr<SWidget> ReloadedInputsBody = Find_WidgetByTag(Page, TEXT("inputs-body"));
    const TSharedPtr<SWidget> ReloadedResultBody = Find_WidgetByTag(Page, TEXT("result-body"));
    if (NOT TestNotNull(TEXT("UV & Density remounts its inputs body after layout reload"), ReloadedInputsBody.Get())
        || NOT TestNotNull(TEXT("UV & Density remounts its result body after layout reload"), ReloadedResultBody.Get()))
    {
        return false;
    }
    TestEqual(TEXT("UV & Density inputs collapse survives layout reload"), ReloadedInputsBody->GetVisibility(), EVisibility::Collapsed);
    TestEqual(TEXT("UV & Density result collapse survives layout reload"), ReloadedResultBody->GetVisibility(), EVisibility::Collapsed);
    const TSharedPtr<SWidget> ReloadedInputsToggleWidget = Find_WidgetByTag(Page, TEXT("inputs-toggle"));
    const TSharedPtr<SWidget> ReloadedResultToggleWidget = Find_WidgetByTag(Page, TEXT("result-toggle"));
    if (NOT TestEqual(TEXT("UV & Density reload keeps the inputs toggle clickable"), ReloadedInputsToggleWidget.IsValid() ? ReloadedInputsToggleWidget->GetTypeAsString() : FString{}, FString{TEXT("SButton")})
        || NOT TestEqual(TEXT("UV & Density reload keeps the result toggle clickable"), ReloadedResultToggleWidget.IsValid() ? ReloadedResultToggleWidget->GetTypeAsString() : FString{}, FString{TEXT("SButton")}))
    {
        return false;
    }
    StaticCastSharedPtr<SButton>(ReloadedInputsToggleWidget)->SimulateClick();
    Page->SlatePrepass();
    TestEqual(TEXT("UV & Density inputs reopen independently after layout reload"), ReloadedInputsBody->GetVisibility(), EVisibility::Visible);
    TestEqual(TEXT("UV & Density result remains collapsed when inputs reopen"), ReloadedResultBody->GetVisibility(), EVisibility::Collapsed);
    StaticCastSharedPtr<SButton>(ReloadedResultToggleWidget)->SimulateClick();
    Page->SlatePrepass();
    TestEqual(TEXT("UV & Density result reopens independently after layout reload"), ReloadedResultBody->GetVisibility(), EVisibility::Visible);
    return true;
}

#endif
