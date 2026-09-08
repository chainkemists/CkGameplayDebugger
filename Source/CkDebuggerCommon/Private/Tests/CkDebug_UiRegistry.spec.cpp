#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"

#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkFlexLayoutTypes.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Styling/CoreStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_debug_ui_registry_tests
{
    auto FindWidget(const TSharedRef<SWidget>& InRoot, const FString& InType) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTypeAsString() == InType) { return InRoot; }
        const FChildren* Children = InRoot->GetChildren();
        if (Children == nullptr) { return nullptr; }
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType); Found.IsValid())
            {
                return Found;
            }
        }
        return nullptr;
    }

    auto RegionContent(const TSharedRef<FCkUiView>& InView) -> TSharedRef<SWidget>
    {
        const TSharedRef<SWidget> Region = InView->GetRegion(TEXT("main"));
        return ConstCastSharedRef<SWidget>(Region->GetChildren()->GetChildAt(0));
    }

    auto Markup() -> FString
    {
        return TEXT("<ui version=\"1\"><region name=\"main\"><column id=\"root\"><debug-meter id=\"meter\" fraction-bind=\"fraction\" fill-bind=\"fill\" width=\"96\" height=\"4\"/><debug-status id=\"status\" label-bind=\"label\" foreground-bind=\"foreground\" background-bind=\"background\" show-dot=\"false\"/></column></region></ui>");
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };

    auto Tick(FSlateApplication& InSlate) -> void { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug_UiRegistry_Runtime,
    "Ck.UiAuthoring.Debugger.CommonWidgets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebug_UiRegistry_Runtime::RunTest(const FString&) -> bool
{
    using namespace ck_debug_ui_registry_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Debug widget registry test requires initialized Slate.")); return false; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (!TestTrue(TEXT("Debug widget registry creates atomically"), FCkDebug_UiRegistry::TryCreate(Registry).Succeeded) || !Registry.IsValid()) { return false; }
    if (!TestTrue(TEXT("Debug registry has meter and status definitions"), Registry->Find(TEXT("debug-meter")) != nullptr && Registry->Find(TEXT("debug-status")) != nullptr)) { return false; }

    FText Label = FText::FromString(TEXT("Loading"));
    FLinearColor Fill = FLinearColor::Blue;
    FLinearColor Foreground = FLinearColor::Yellow;
    FLinearColor Background = FLinearColor::Black;
    auto Data = FCkUiView::FDataBindings{};
    Data.Number.Add(TEXT("fraction"), TAttribute<float>(0.5f));
    Data.Color.Add(TEXT("fill"), TAttribute<FLinearColor>::CreateLambda([&Fill]() { return Fill; }));
    Data.Text.Add(TEXT("label"), TAttribute<FText>::CreateLambda([&Label]() { return Label; }));
    Data.Color.Add(TEXT("foreground"), TAttribute<FLinearColor>::CreateLambda([&Foreground]() { return Foreground; }));
    Data.Color.Add(TEXT("background"), TAttribute<FLinearColor>::CreateLambda([&Background]() { return Background; }));
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, {}, FSlateFontInfo{}, MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{320.0f, 180.0f}).CreateTitleBar(false).HasCloseButton(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    if (!TestTrue(TEXT("Authored debug widgets load"), View->TryReload(Markup(), TEXT(""), TEXT("DebugUiRegistry")).Succeeded)) { return false; }
    Tick(Slate);

    const TSharedRef<SWidget> Root = RegionContent(View);
    const TSharedPtr<SCkDebug_MeterBar> Meter = StaticCastSharedPtr<SCkDebug_MeterBar>(FindWidget(Root, TEXT("SCkDebug_MeterBar")));
    const TSharedPtr<SCkDebug_StatusPill> Status = StaticCastSharedPtr<SCkDebug_StatusPill>(FindWidget(Root, TEXT("SCkDebug_StatusPill")));
    const TSharedPtr<STextBlock> StatusLabel = StaticCastSharedPtr<STextBlock>(Status.IsValid() ? FindWidget(Status.ToSharedRef(), TEXT("STextBlock")) : nullptr);
    if (!TestTrue(TEXT("Authored meter and status produce their real widgets"), Meter.IsValid() && Status.IsValid() && StatusLabel.IsValid())) { return false; }
    TestEqual(TEXT("Status label reads bound data"), StatusLabel->GetText().ToString(), FString(TEXT("Loading")));
    TestTrue(TEXT("Status foreground reads bound color"), StatusLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(Foreground));

    Label = FText::FromString(TEXT("Ready"));
    Foreground = FLinearColor::Green;
    Tick(Slate);
    TestEqual(TEXT("Status label updates through the generated child"), StatusLabel->GetText().ToString(), FString(TEXT("Ready")));
    TestTrue(TEXT("Status foreground updates through the generated child"), StatusLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(Foreground));

    const int64 Revision = View->GetRevision();
    const TSharedPtr<SWidget> OriginalMeter = Meter;
    const TSharedPtr<SWidget> OriginalStatus = Status;
    const FCkUiLoadResult BadWidth = View->TryReload(Markup().Replace(TEXT("width=\"96\""), TEXT("width=\"-1\"")), TEXT(""), TEXT("DebugUiRegistryBadWidth"));
    TestFalse(TEXT("Malformed meter width rejects"), BadWidth.Succeeded);
    TestTrue(TEXT("Malformed meter width reports an error"), !BadWidth.Errors.IsEmpty());
    TestEqual(TEXT("Malformed meter width retains the accepted revision"), View->GetRevision(), Revision);
    TestTrue(TEXT("Malformed meter width retains the complete accepted tree"),
        FindWidget(RegionContent(View), TEXT("SCkDebug_MeterBar")) == OriginalMeter
        && FindWidget(RegionContent(View), TEXT("SCkDebug_StatusPill")) == OriginalStatus);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug_UiRegistry_Inspector,
    "Ck.UiAuthoring.Debugger.InspectorContainer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebug_UiRegistry_Inspector::RunTest(const FString&) -> bool
{
    using namespace ck_debug_ui_registry_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Inspector test requires Slate.")); return false; }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (!TestTrue(TEXT("Inspector registry creates"), FCkDebug_UiRegistry::TryCreate(Registry).Succeeded)) { return false; }
    if (!TestTrue(TEXT("Shared inspector tag is registered"), Registry->Find(TEXT("debug-inspector")) != nullptr)) { return false; }
    FText Title = NSLOCTEXT("CkUiInspectorTests", "Title", "Profiles");
    const FText Alternate = NSLOCTEXT("CkUiInspectorTests", "Alternate", "Details");
    FText Query;
    FCkUiView::FDataBindings Data;
    Data.Text.Add(TEXT("title"), TAttribute<FText>::CreateLambda([&Title]() { return Title; }));
    Data.Text.Add(TEXT("alternate"), Alternate);
    Data.Text.Add(TEXT("query"), TAttribute<FText>::CreateLambda([&Query]() { return Query; }));
    Data.TextChanged.Add(TEXT("query"), FOnTextChanged::CreateLambda([&Query](const FText& Value) { Query = Value; }));
    const auto View = FCkUiView::Create({}, {}, {}, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12), MoveTemp(Data), Registry);
    const auto Region = View->GetRegion(TEXT("main"));
    const FString Source = TEXT("<ui version=\"1\"><region name=\"main\"><debug-inspector id=\"inspector\" title-bind=\"title\"><slot name=\"body\"><column id=\"details\"><text id=\"help\">Profiles apply a complete curated style. Editing an individual control changes the active profile to Custom.</text><search id=\"query\" bind=\"query\"/></column></slot></debug-inspector></region></ui>");
    const auto Load = View->TryReload(Source, TEXT(""));
    if (!TestTrue(*FString::Join(Load.Errors, TEXT("\n")), Load.Succeeded)) { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope(Slate);
    Scope.Window = SNew(SWindow).ClientSize(FVector2D(420, 320)).CreateTitleBar(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    Tick(Slate);
    const auto Panel = StaticCastSharedPtr<SCkDebug_InspectorPanel>(FindWidget(Region, TEXT("SCkDebug_InspectorPanel")));
    const auto Search = StaticCastSharedPtr<SSearchBox>(FindWidget(Region, TEXT("SSearchBox")));
    if (!TestTrue(TEXT("Authored inspector mounts shared panel and native search"), Panel.IsValid() && Search.IsValid())) { return false; }
    const auto Header = StaticCastSharedPtr<STextBlock>(FindWidget(Panel.ToSharedRef(), TEXT("STextBlock")));
    if (!TestTrue(TEXT("Inspector header exists"), Header.IsValid())) { return false; }
    TestTrue(TEXT("Inspector preserves localized title identity"), Header->GetText().IdenticalTo(Title));
    Title = NSLOCTEXT("CkUiInspectorTests", "ChangedTitle", "Resources");
    Tick(Slate);
    TestTrue(TEXT("Inspector title binding updates live"), Header->GetText().IdenticalTo(Title));
    const auto Measure = Panel->GetMetaData<FCkFlexMeasureMetaData>();
    if (!TestTrue(TEXT("Inspector exposes constrained body measurement"), Measure.IsValid())) { return false; }
    FCkFlexMeasureArgs Args;
    Args.WidthMode = YGMeasureModeExactly;
    Args.AvailableWidth = 420;
    const FVector2D Wide = Measure->Measure(Args);
    Args.AvailableWidth = 90;
    const FVector2D Narrow = Measure->Measure(Args);
    TestTrue(TEXT("Inspector body wraps to its constrained width"), FMath::IsNearlyEqual(Narrow.X, 90.0) && Narrow.Y > Wide.Y);
    Search->SetText(FText::FromString(TEXT("Retained draft")));
    Panel->Set_Expanded(false);
    const FVector2D Collapsed = Measure->Measure(Args);
    TestTrue(TEXT("Collapsed inspector measures header only"), Collapsed.Y > 0 && Collapsed.Y < Wide.Y);
    const auto Reload = View->TryReload(Source.Replace(TEXT("title-bind=\"title\""), TEXT("title-bind=\"alternate\"")), TEXT(""));
    if (!TestTrue(*FString::Join(Reload.Errors, TEXT("\n")), Reload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Reload preserves collapsed panel and authored search"), FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == Panel
        && FindWidget(Region, TEXT("SSearchBox")) == Search && !Panel->Is_Expanded() && Search->GetText().ToString() == TEXT("Retained draft"));
    TestTrue(TEXT("Committed reload rebinds localized title"), Header->GetText().IdenticalTo(Alternate));
    const int64 Revision = View->GetRevision();
    const auto Rejected = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><debug-inspector id=\"inspector\" title-bind=\"title\"/></region></ui>"), TEXT(""));
    TestTrue(TEXT("Missing inspector body rejects without changing accepted state"), !Rejected.Succeeded && !Rejected.Errors.IsEmpty()
        && View->GetRevision() == Revision && FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == Panel
        && !Panel->Is_Expanded() && Header->GetText().IdenticalTo(Alternate));
    return true;
}

#endif
