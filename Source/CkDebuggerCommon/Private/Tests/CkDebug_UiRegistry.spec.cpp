#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/CkFlexLayoutTypes.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiFloatSeries.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSearchBox.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_debug_ui_registry_tests
{
    auto HasNativeText(const TSharedRef<SWidget>& Root, const FString& Text) -> bool
    {
        if (Root->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString() == Text) { return true; }
        const FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { if (HasNativeText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), Text)) { return true; } }
        return false;
    }

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

    auto FindWidgets(const TSharedRef<SWidget>& InRoot, const FString& InType, TArray<TSharedPtr<SWidget>>& OutWidgets) -> void
    {
        if (InRoot->GetTypeAsString() == InType) { OutWidgets.Add(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { FindWidgets(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InType, OutWidgets); }
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
    if (!TestTrue(TEXT("Debug registry has meter, sparkline, status, and icon definitions"), Registry->Find(TEXT("debug-meter")) != nullptr
        && Registry->Find(TEXT("debug-sparkline")) != nullptr
        && Registry->Find(TEXT("debug-status")) != nullptr && Registry->Find(TEXT("debug-icon")) != nullptr)) { return false; }

    FText Label = FText::FromString(TEXT("Loading"));
    FLinearColor Fill = FLinearColor::Blue;
    FLinearColor Foreground = FLinearColor::Yellow;
    FLinearColor Background = FLinearColor::Black;
    FText IconMeaning = FText::FromString(TEXT("Actor"));
    FLinearColor IconColor = FLinearColor::Red;
    const TSharedRef<FSlateBrush> LiteralIconBrush = MakeShared<FSlateBrush>();
    TSharedPtr<FSlateBrush> BoundIconBrush = MakeShared<FSlateBrush>();
    const TSharedRef<FSlateBrush> UpdatedBoundIconBrush = MakeShared<FSlateBrush>();
    const TSharedRef<FSlateBrush> ProblemDotBrush = MakeShared<FSlateBrush>();
    const TSharedRef<FSlateBrush> CollectionIconBrush = MakeShared<FSlateBrush>();
    TSharedPtr<FCkUiCollection> Icons;
    TSharedPtr<FCkUiFloatSeries> SparklineSamples;
    if (!TestTrue(TEXT("Sparkline sample owner creates"), FCkUiFloatSeries::TryCreate({1.0f, 2.0f}, SparklineSamples).Succeeded)) { return false; }
    if (!TestTrue(TEXT("Icon repeat collection creates"), FCkUiCollection::TryCreate({
        {TEXT("icon"), ECkUiFieldKind::Image}, {TEXT("meaning"), ECkUiFieldKind::Text}, {TEXT("color"), ECkUiFieldKind::Color}}, Icons).Succeeded)) { return false; }
    FCkUiRecordData IconRecord;
    IconRecord.Key = TEXT("row");
    IconRecord.Fields.Add(TEXT("icon"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Image, .Image = CollectionIconBrush});
    IconRecord.Fields.Add(TEXT("meaning"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(TEXT("Camera"))});
    IconRecord.Fields.Add(TEXT("color"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = FLinearColor::Blue});
    if (!TestTrue(TEXT("Icon repeat record publishes"), Icons->TrySetRecords({MoveTemp(IconRecord)}).Succeeded)) { return false; }
    auto Data = FCkUiView::FDataBindings{};
    Data.Number.Add(TEXT("fraction"), TAttribute<float>(0.5f));
    Data.Color.Add(TEXT("fill"), TAttribute<FLinearColor>::CreateLambda([&Fill]() { return Fill; }));
    Data.Text.Add(TEXT("label"), TAttribute<FText>::CreateLambda([&Label]() { return Label; }));
    Data.Color.Add(TEXT("foreground"), TAttribute<FLinearColor>::CreateLambda([&Foreground]() { return Foreground; }));
    Data.Color.Add(TEXT("background"), TAttribute<FLinearColor>::CreateLambda([&Background]() { return Background; }));
    Data.Images.Add(TEXT("literal-icon"), &LiteralIconBrush.Get());
    Data.Images.Add(TEXT("bound-icon"), TAttribute<const FSlateBrush*>::CreateLambda([&BoundIconBrush]() { return BoundIconBrush.Get(); }));
    Data.Images.Add(TEXT("problem-dot"), &ProblemDotBrush.Get());
    Data.Text.Add(TEXT("icon-meaning"), TAttribute<FText>::CreateLambda([&IconMeaning]() { return IconMeaning; }));
    Data.Color.Add(TEXT("icon-color"), TAttribute<FLinearColor>::CreateLambda([&IconColor]() { return IconColor; }));
    Data.Collections.Add(TEXT("icons"), Icons);
    Data.FloatSeries.Add(TEXT("samples"), SparklineSamples);
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, {}, FSlateFontInfo{}, MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{320.0f, 180.0f}).CreateTitleBar(false).HasCloseButton(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    const FString IconMarkup = TEXT("<ui version=\"1\"><region name=\"main\"><column id=\"root\"><debug-meter id=\"meter\" fraction-bind=\"fraction\" fill-bind=\"fill\" width=\"96\" height=\"4\"/><debug-status id=\"status\" label-bind=\"label\" foreground-bind=\"foreground\" background-bind=\"background\" show-dot=\"false\"/><debug-icon id=\"literal\" icon-bind=\"literal-icon\" meaning-bind=\"icon-meaning\" color-bind=\"icon-color\" size=\"12\"/><debug-icon id=\"bound\" icon-bind=\"bound-icon\" meaning-bind=\"icon-meaning\" color-bind=\"icon-color\"/><debug-icon id=\"problem\" icon-bind=\"problem-dot\" meaning-bind=\"icon-meaning\"/><repeat id=\"icons\" bind=\"icons\"><debug-icon id=\"item\" icon-field=\"icon\" meaning-field=\"meaning\" color-field=\"color\"/></repeat></column></region></ui>");
    if (!TestTrue(TEXT("Authored debug widgets load"), View->TryReload(IconMarkup, TEXT(""), TEXT("DebugUiRegistry")).Succeeded)) { return false; }
    Tick(Slate);

    const TSharedRef<SWidget> Root = RegionContent(View);
    const TSharedPtr<SCkDebug_MeterBar> Meter = StaticCastSharedPtr<SCkDebug_MeterBar>(FindWidget(Root, TEXT("SCkDebug_MeterBar")));
    const TSharedPtr<SCkDebug_StatusPill> Status = StaticCastSharedPtr<SCkDebug_StatusPill>(FindWidget(Root, TEXT("SCkDebug_StatusPill")));
    const TSharedPtr<SCkDebug_Icon> Icon = StaticCastSharedPtr<SCkDebug_Icon>(FindWidget(Root, TEXT("SCkDebug_Icon")));
    const TSharedPtr<STextBlock> StatusLabel = StaticCastSharedPtr<STextBlock>(Status.IsValid() ? FindWidget(Status.ToSharedRef(), TEXT("STextBlock")) : nullptr);
    if (!TestTrue(TEXT("Authored meter, status, and icon produce their real widgets"), Meter.IsValid() && Status.IsValid() && StatusLabel.IsValid() && Icon.IsValid())) { return false; }
    TestEqual(TEXT("Status label reads bound data"), StatusLabel->GetText().ToString(), FString(TEXT("Loading")));
    TestTrue(TEXT("Status foreground reads bound color"), StatusLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(Foreground));
    TestEqual(TEXT("Icon meaning reads live binding"), Icon->Get_Meaning().ToString(), FString(TEXT("Actor")));
    if (!TestTrue(TEXT("Typed icon binding resolves to the supplied brush"), Icon->Get_Brush() == &LiteralIconBrush.Get())) { return false; }
    TestTrue(TEXT("Icon color reads live binding"), Icon->Get_ColorAndOpacity().GetSpecifiedColor().Equals(IconColor));
    auto MountedIcons = TArray<TSharedPtr<SWidget>>{};
    FindWidgets(Root, TEXT("SCkDebug_Icon"), MountedIcons);
    if (!TestEqual(TEXT("Typed, bound, problem-dot, and collection icons mount"), MountedIcons.Num(), 4)) { return false; }
    const TSharedPtr<SCkDebug_Icon> BoundIcon = StaticCastSharedPtr<SCkDebug_Icon>(MountedIcons[1]);
    const TSharedPtr<SCkDebug_Icon> ProblemIcon = StaticCastSharedPtr<SCkDebug_Icon>(MountedIcons[2]);
    const TSharedPtr<SCkDebug_Icon> CollectionIcon = StaticCastSharedPtr<SCkDebug_Icon>(MountedIcons[3]);
    TestTrue(TEXT("Bound icon forwards its exact typed brush"), BoundIcon.IsValid() && BoundIcon->Get_Brush() == BoundIconBrush.Get());
    TestTrue(TEXT("Problem dot forwards its exact supplied brush"), ProblemIcon.IsValid() && ProblemIcon->Get_Brush() == &ProblemDotBrush.Get());
    TestTrue(TEXT("Collection fields forward their exact typed icon brush"), CollectionIcon.IsValid() && CollectionIcon->Get_Brush() == &CollectionIconBrush.Get());

    BoundIconBrush = UpdatedBoundIconBrush;
    Tick(Slate);
    TestTrue(TEXT("Bound icon follows its live brush binding after mount"), BoundIcon->Get_Brush() == &UpdatedBoundIconBrush.Get());

    Label = FText::FromString(TEXT("Ready"));
    Foreground = FLinearColor::Green;
    IconMeaning = FText::FromString(TEXT("Updated actor"));
    IconColor = FLinearColor::Green;
    Tick(Slate);
    TestEqual(TEXT("Status label updates through the generated child"), StatusLabel->GetText().ToString(), FString(TEXT("Ready")));
    TestTrue(TEXT("Status foreground updates through the generated child"), StatusLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(Foreground));
    TestEqual(TEXT("Icon meaning updates through its binding"), Icon->Get_Meaning().ToString(), FString(TEXT("Updated actor")));
    TestTrue(TEXT("Icon color updates through its binding"), Icon->Get_ColorAndOpacity().GetSpecifiedColor().Equals(IconColor));

    const int64 Revision = View->GetRevision();
    const TSharedPtr<SWidget> OriginalMeter = Meter;
    const TSharedPtr<SWidget> OriginalStatus = Status;
    const FCkUiLoadResult BadIcon = View->TryReload(IconMarkup.Replace(TEXT("icon-bind=\"literal-icon\""), TEXT("icon-bind=\"missing\"")), TEXT(""), TEXT("DebugUiRegistryBadIcon"));
    TestFalse(TEXT("Unavailable icon binding rejects"), BadIcon.Succeeded);
    TestTrue(TEXT("Unavailable icon binding reports an error"), !BadIcon.Errors.IsEmpty());
    TestEqual(TEXT("Rejected icon reload retains the accepted revision"), View->GetRevision(), Revision);
    TestTrue(TEXT("Rejected icon reload preserves the accepted mounted tree"),
        FindWidget(RegionContent(View), TEXT("SCkDebug_MeterBar")) == OriginalMeter
        && FindWidget(RegionContent(View), TEXT("SCkDebug_StatusPill")) == OriginalStatus);
    const FString SparklineMarkup = TEXT("<ui version=\"1\"><region name=\"main\"><debug-sparkline id=\"sparkline\" samples-bind=\"samples\" width=\"120\" height=\"26\" color=\"#FFFFFFFF\" fill-opacity=\"0.25\"/></region></ui>");
    if (!TestTrue(TEXT("Authored debug sparkline loads"), View->TryReload(SparklineMarkup, TEXT(""), TEXT("DebugUiSparkline")).Succeeded)) { return false; }
    Tick(Slate);
    const TSharedPtr<SCkDebug_Sparkline> Sparkline = StaticCastSharedPtr<SCkDebug_Sparkline>(FindWidget(RegionContent(View), TEXT("SCkDebug_Sparkline")));
    TSharedPtr<const FCkUiFloatSeries> MountedSeries = Sparkline.IsValid() ? Sparkline->Get_FloatSeries().Pin() : nullptr;
    if (!TestTrue(TEXT("Authored sparkline mounts its physical widget with the supplied live series"), Sparkline.IsValid() && MountedSeries.IsValid() && MountedSeries.Get() == SparklineSamples.Get())) { return false; }
    const int64 SeriesRevision = SparklineSamples->GetRevision();
    if (!TestTrue(TEXT("Sparkline owner updates samples in place"), SparklineSamples->TrySetSamples({3.0f, 5.0f, 8.0f}).Succeeded)) { return false; }
    Tick(Slate);
    TSharedPtr<const FCkUiFloatSeries> UpdatedSeries = Sparkline->Get_FloatSeries().Pin();
    TestTrue(TEXT("Mounted sparkline observes the owner revision and replacement samples without reconstruction"), UpdatedSeries.IsValid()
        && UpdatedSeries.Get() == SparklineSamples.Get() && UpdatedSeries->GetRevision() == SeriesRevision + 1
        && UpdatedSeries->GetSamples() == TArray<float>({3.0f, 5.0f, 8.0f}));
    const int64 SparklineRevision = View->GetRevision();
    const TSharedPtr<SWidget> OriginalSparkline = Sparkline;
    const auto RejectSparkline = [this, &View, SparklineRevision, OriginalSparkline](const FString& Name, const FString& Markup)
    {
        const FCkUiLoadResult Rejected = View->TryReload(Markup, TEXT(""), Name);
        TestTrue(*(Name + TEXT(" rejects before construction and preserves the accepted revision/tree/widget")), !Rejected.Succeeded
            && !Rejected.Errors.IsEmpty() && View->GetRevision() == SparklineRevision
            && FindWidget(RegionContent(View), TEXT("SCkDebug_Sparkline")) == OriginalSparkline);
    };
    RejectSparkline(TEXT("Missing sparkline samples reject"), TEXT("<ui version=\"1\"><region name=\"main\"><debug-sparkline id=\"sparkline\"/></region></ui>"));
    RejectSparkline(TEXT("Wrong sparkline samples type rejects"), TEXT("<ui version=\"1\"><region name=\"main\"><debug-sparkline id=\"sparkline\" samples-bind=\"fill\"/></region></ui>"));
    RejectSparkline(TEXT("Invalid sparkline width rejects"), SparklineMarkup.Replace(TEXT("width=\"120\""), TEXT("width=\"0\"")));
    RejectSparkline(TEXT("Invalid sparkline fill opacity rejects"), SparklineMarkup.Replace(TEXT("fill-opacity=\"0.25\""), TEXT("fill-opacity=\"1.1\"")));
    MountedSeries.Reset();
    UpdatedSeries.Reset();
    SparklineSamples.Reset();
    RejectSparkline(TEXT("Expired sparkline samples reject"), SparklineMarkup);
    const FCkUiLoadResult BadSize = View->TryReload(IconMarkup.Replace(TEXT("size=\"12\""), TEXT("size=\"25\"")), TEXT(""), TEXT("DebugUiRegistryBadIconSize"));
    TestTrue(TEXT("Out-of-range icon size rejects without changing the accepted revision or tree"), !BadSize.Succeeded
        && !BadSize.Errors.IsEmpty() && View->GetRevision() == SparklineRevision
        && FindWidget(RegionContent(View), TEXT("SCkDebug_Sparkline")) == OriginalSparkline);
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
    const FString ConfiguredSource = Source.Replace(TEXT("title-bind=\"title\""), TEXT("title-bind=\"title\" count=\"3\" start-expanded=\"false\""));
    const auto Load = View->TryReload(ConfiguredSource, TEXT(""));
    if (!TestTrue(*FString::Join(Load.Errors, TEXT("\n")), Load.Succeeded)) { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope(Slate);
    Scope.Window = SNew(SWindow).ClientSize(FVector2D(420, 320)).CreateTitleBar(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    Tick(Slate);
    const auto Panel = StaticCastSharedPtr<SCkDebug_InspectorPanel>(FindWidget(Region, TEXT("SCkDebug_InspectorPanel")));
    const auto Search = StaticCastSharedPtr<SSearchBox>(FindWidget(Region, TEXT("SSearchBox")));
    if (!TestTrue(TEXT("Authored inspector mounts shared panel and native search"), Panel.IsValid() && Search.IsValid())) { return false; }
    TestTrue(TEXT("Inspector honors the initial collapsed state and count badge"), !Panel->Is_Expanded() && HasNativeText(Panel.ToSharedRef(), TEXT("3")));
    Panel->Set_Expanded(true);
    Tick(Slate);
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
    const FString UpdatedSource = ConfiguredSource.Replace(TEXT("title-bind=\"title\""), TEXT("title-bind=\"alternate\""))
        .Replace(TEXT("count=\"3\""), TEXT("count=\"4\"")).Replace(TEXT("start-expanded=\"false\""), TEXT("start-expanded=\"true\""));
    const auto Reload = View->TryReload(UpdatedSource, TEXT(""));
    if (!TestTrue(*FString::Join(Reload.Errors, TEXT("\n")), Reload.Succeeded)) { return false; }
    Tick(Slate);
    TestTrue(TEXT("Reload preserves collapsed panel and authored search"), FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == Panel
        && FindWidget(Region, TEXT("SSearchBox")) == Search && !Panel->Is_Expanded() && Search->GetText().ToString() == TEXT("Retained draft"));
    TestTrue(TEXT("Committed reload rebinds localized title"), Header->GetText().IdenticalTo(Alternate));
    TestTrue(TEXT("Committed reload updates count without applying the construction expansion hint"), HasNativeText(Panel.ToSharedRef(), TEXT("4")) && !Panel->Is_Expanded());
    const int64 Revision = View->GetRevision();
    const auto Rejected = View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><debug-inspector id=\"inspector\" title-bind=\"title\"/></region></ui>"), TEXT(""));
    TestTrue(TEXT("Missing inspector body rejects without changing accepted state"), !Rejected.Succeeded && !Rejected.Errors.IsEmpty()
        && View->GetRevision() == Revision && FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == Panel
        && !Panel->Is_Expanded() && Header->GetText().IdenticalTo(Alternate));
    const auto BadExpansion = View->TryReload(UpdatedSource.Replace(TEXT("start-expanded=\"true\""), TEXT("start-expanded=\"invalid\"")), TEXT(""));
    TestTrue(TEXT("Malformed initial expansion rejects without changing the accepted count or revision"), !BadExpansion.Succeeded
        && View->GetRevision() == Revision && !Panel->Is_Expanded() && HasNativeText(Panel.ToSharedRef(), TEXT("4")));
    const auto WithoutCount = View->TryReload(UpdatedSource.Replace(TEXT(" count=\"4\""), TEXT("")), TEXT(""));
    TestTrue(TEXT("Removing the optional count clears its badge and retains the panel and expansion"), WithoutCount.Succeeded
        && FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == Panel && !Panel->Is_Expanded()
        && !HasNativeText(Panel.ToSharedRef(), TEXT("4")));
    const FString DefaultSource = Source.Replace(TEXT("id=\"inspector\""), TEXT("id=\"default-inspector\""));
    const auto DefaultLoad = View->TryReload(DefaultSource, TEXT(""));
    const auto DefaultPanel = StaticCastSharedPtr<SCkDebug_InspectorPanel>(FindWidget(Region, TEXT("SCkDebug_InspectorPanel")));
    if (!TestTrue(TEXT("An inspector with omitted options starts expanded without a count"), DefaultLoad.Succeeded
        && DefaultPanel.IsValid() && DefaultPanel != Panel && DefaultPanel->Is_Expanded() && !HasNativeText(DefaultPanel.ToSharedRef(), TEXT("4")))) { return false; }
    const auto AddedCount = View->TryReload(DefaultSource.Replace(TEXT("title-bind=\"title\""), TEXT("title-bind=\"title\" count=\"7\" start-expanded=\"false\"")), TEXT(""));
    TestTrue(TEXT("Adding a count on reload retains the default panel and its current expansion"), AddedCount.Succeeded
        && FindWidget(Region, TEXT("SCkDebug_InspectorPanel")) == DefaultPanel && DefaultPanel->Is_Expanded() && HasNativeText(DefaultPanel.ToSharedRef(), TEXT("7")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug_UiRegistry_DebugSwitch,
    "Ck.UiAuthoring.Debugger.DebugSwitch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebug_UiRegistry_DebugSwitch::RunTest(const FString&) -> bool
{
    using namespace ck_debug_ui_registry_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Debug switch test requires initialized Slate.")); return false; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (!TestTrue(TEXT("Debug switch registry creates"), FCkDebug_UiRegistry::TryCreate(Registry).Succeeded) || !Registry.IsValid()) { return false; }
    if (!TestTrue(TEXT("Debug switch tag is registered"), Registry->Find(TEXT("debug-switch")) != nullptr)) { return false; }

    bool Checked = false;
    bool Alternate = false;
    bool Enabled = true;
    bool CanDispatch = true;
    int32 FirstChangedCount = 0;
    int32 SecondChangedCount = 0;
    auto Data = FCkUiView::FDataBindings{};
    Data.Visibility.Add(TEXT("checked"), TAttribute<bool>::CreateLambda([&Checked]() { return Checked; }));
    Data.Visibility.Add(TEXT("alternate"), TAttribute<bool>::CreateLambda([&Alternate]() { return Alternate; }));
    Data.Visibility.Add(TEXT("enabled"), TAttribute<bool>::CreateLambda([&Enabled]() { return Enabled; }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([&CanDispatch]() { return CanDispatch; });
    Data.BoolChanged.Add(TEXT("changed"), FCkUiOnBoolChanged::CreateLambda([&Checked, &FirstChangedCount](const bool InChecked)
    { Checked = InChecked; ++FirstChangedCount; }));
    Data.BoolChanged.Add(TEXT("changed-next"), FCkUiOnBoolChanged::CreateLambda([&Checked, &SecondChangedCount](const bool InChecked)
    { Checked = InChecked; ++SecondChangedCount; }));

    TSharedPtr<FCkUiView> View = FCkUiView::Create({}, {}, {}, FSlateFontInfo{}, MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const FString Source = TEXT("<ui version=\"1\"><region name=\"main\"><debug-switch id=\"retained-switch\" checked-bind=\"checked\" changed=\"changed\"/></region></ui>");
    const FString SourceWithEnabled = TEXT("<ui version=\"1\"><region name=\"main\"><debug-switch id=\"retained-switch\" checked-bind=\"checked\" changed=\"changed\" enabled-bind=\"enabled\"/></region></ui>");
    if (!TestTrue(TEXT("Authored debug switch loads"), View->TryReload(Source, TEXT(""), TEXT("DebugSwitch")).Succeeded)) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{120.0f, 60.0f}).CreateTitleBar(false).HasCloseButton(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    Tick(Slate);
    const TSharedPtr<SCkDebug_Switch> Switch = StaticCastSharedPtr<SCkDebug_Switch>(FindWidget(Region, TEXT("SCkDebug_Switch")));
    if (!TestTrue(TEXT("Debug switch mounts the existing switch widget"), Switch.IsValid())) { return false; }
    TestTrue(TEXT("Omitted enabled binding defaults the switch to enabled"), Switch->IsEnabled());

    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{});
    const TSet<FKey> PressedButtons{EKeys::LeftMouseButton};
    const FPointerEvent Click{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, PressedButtons,
        EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    Checked = true;
    Tick(Slate);
    Switch->OnMouseButtonDown(Geometry, Click);
    Tick(Slate);
    if (!TestTrue(TEXT("Omitted enabled binding leaves mouse toggle interactive"), !Checked && FirstChangedCount == 1)) { return false; }
    Slate.SetUserFocus(0, Switch, EFocusCause::SetDirectly);
    if (!TestTrue(TEXT("Switch accepts keyboard focus"), Slate.GetUserFocusedWidget(0) == Switch)) { return false; }
    if (!TestTrue(TEXT("Space routes to the mounted switch"), Slate.ProcessKeyDownEvent(FKeyEvent{EKeys::SpaceBar, FModifierKeysState{}, 0, false, 0, 0}))) { return false; }
    Tick(Slate);
    if (!TestTrue(TEXT("Omitted enabled binding leaves keyboard toggle interactive"), Checked && FirstChangedCount == 2)) { return false; }

    const int64 Revision = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(Source.Replace(TEXT("checked-bind=\"checked\""), TEXT("checked-bind=\"alternate\"")), TEXT(""), TEXT("DebugSwitchRejected"));
    if (!TestTrue(TEXT("Changing the retained checked binding rejects without replacing the switch"), !Rejected.Succeeded
        && !Rejected.Errors.IsEmpty() && View->GetRevision() == Revision
        && FindWidget(Region, TEXT("SCkDebug_Switch")) == Switch && Slate.GetUserFocusedWidget(0) == Switch)) { return false; }

    const auto RejectRequiredBinding = [this, &View, &Region, &Slate, Revision, Switch](const FString& Name, const FString& Markup)
    {
        const FCkUiLoadResult RejectedRequiredBinding = View->TryReload(Markup, TEXT(""), Name);
        TestTrue(*Name, !RejectedRequiredBinding.Succeeded && !RejectedRequiredBinding.Errors.IsEmpty()
            && View->GetRevision() == Revision && FindWidget(Region, TEXT("SCkDebug_Switch")) == Switch
            && Slate.GetUserFocusedWidget(0) == Switch);
    };
    RejectRequiredBinding(TEXT("Missing required checked binding rejects without disturbing retained focus"),
        Source.Replace(TEXT("checked-bind=\"checked\" "), TEXT("")));
    RejectRequiredBinding(TEXT("Missing required changed callback rejects without disturbing retained focus"),
        Source.Replace(TEXT(" changed=\"changed\""), TEXT("")));

    const FCkUiLoadResult Rebound = View->TryReload(SourceWithEnabled.Replace(TEXT("changed=\"changed\""), TEXT("changed=\"changed-next\"")), TEXT(""), TEXT("DebugSwitchRebound"));
    if (!TestTrue(TEXT("Compatible reload retains switch identity and focus"), Rebound.Succeeded
        && FindWidget(Region, TEXT("SCkDebug_Switch")) == Switch && Slate.GetUserFocusedWidget(0) == Switch)) { return false; }
    Switch->OnMouseButtonDown(Geometry, Click);
    Tick(Slate);
    if (!TestTrue(TEXT("Compatible reload transactionally replaces the callback"), !Checked && FirstChangedCount == 2 && SecondChangedCount == 1)) { return false; }

    Enabled = false;
    Tick(Slate);
    Switch->OnMouseButtonDown(Geometry, Click);
    Switch->OnKeyDown(Geometry, FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
    if (!TestTrue(TEXT("Disabled switch does not dispatch mouse or keyboard toggles"), !Checked && SecondChangedCount == 1)) { return false; }
    Enabled = true;
    CanDispatch = false;
    Switch->OnMouseButtonDown(Geometry, Click);
    if (!TestTrue(TEXT("Dispatch gate makes retained callback inert"), !Checked && SecondChangedCount == 1)) { return false; }
    CanDispatch = true;

    View.Reset();
    Switch->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("Owner release makes a retained switch callback inert"), !Checked && SecondChangedCount == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug_UiRegistry_EntityRef,
    "Ck.UiAuthoring.Debugger.EntityRef",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkDebug_UiRegistry_EntityRef::RunTest(const FString&) -> bool
{
    using namespace ck_debug_ui_registry_tests;
    if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Entity reference test requires initialized Slate.")); return false; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    if (!TestTrue(TEXT("Entity reference registry creates"), FCkDebug_UiRegistry::TryCreate(Registry).Succeeded)
        || !TestTrue(TEXT("Entity reference tag is registered"), Registry.IsValid() && Registry->Find(TEXT("debug-entity-ref")) != nullptr)) { return false; }

    UCkDebuggerStyleSettings* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    if (!TestNotNull(TEXT("Entity reference style settings are available"), StyleSettings)) { return false; }
    const FCkDebuggerStyleSelection SavedSelection = StyleSettings->Selection;
    ON_SCOPE_EXIT
    {
        StyleSettings->Selection = SavedSelection;
        StyleSettings->NotifyChanged();
    };
    StyleSettings->Selection.EntityIdStyle = ECkDebugAxis_EntityIdStyle::NameAndId;
    StyleSettings->NotifyChanged();

    FText Id = FText::FromString(TEXT("42|1(42)"));
    FText Name = FText::FromString(TEXT("AuthoredEntity"));
    bool CanDispatch = true;
    int32 FirstActionCount = 0;
    int32 SecondActionCount = 0;
    FString FirstActionKey;
    FString SecondActionKey;
    TSharedPtr<FCkUiCollection> Entities;
    if (!TestTrue(TEXT("Entity reference collection creates"), FCkUiCollection::TryCreate({
        {TEXT("id"), ECkUiFieldKind::Text}, {TEXT("name"), ECkUiFieldKind::Text}}, Entities).Succeeded)) { return false; }
    auto Record = FCkUiRecordData{};
    Record.Key = TEXT("stable-42");
    Record.Fields.Add(TEXT("id"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Id});
    Record.Fields.Add(TEXT("name"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = Name});
    if (!TestTrue(TEXT("Entity reference record publishes"), Entities->TrySetRecords({MoveTemp(Record)}).Succeeded)) { return false; }
    auto Data = FCkUiView::FDataBindings{};
    Data.Collections.Add(TEXT("entities"), Entities);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([&CanDispatch]() { return CanDispatch; });
    Data.ItemActions.Add(TEXT("navigate"), FCkUiOnItemAction::CreateLambda([&FirstActionCount, &FirstActionKey](FString InKey)
    { ++FirstActionCount; FirstActionKey = MoveTemp(InKey); }));
    Data.ItemActions.Add(TEXT("navigate-next"), FCkUiOnItemAction::CreateLambda([&SecondActionCount, &SecondActionKey](FString InKey)
    { ++SecondActionCount; SecondActionKey = MoveTemp(InKey); }));
    TSharedPtr<FCkUiView> View = FCkUiView::Create({}, {}, {}, FSlateFontInfo{}, MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const FString Source = TEXT("<ui version=\"1\"><region name=\"main\"><repeat id=\"entities\" bind=\"entities\"><debug-entity-ref id=\"entity\" entity-id-field=\"id\" name-field=\"name\" item-action=\"navigate\" show-name=\"true\"/></repeat></region></ui>");
    if (!TestTrue(TEXT("Authored entity reference loads"), View->TryReload(Source, TEXT(""), TEXT("DebugEntityRef")).Succeeded)) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope Scope{Slate};
    Scope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{220.0f, 60.0f}).CreateTitleBar(false).HasCloseButton(false)[Region];
    Slate.AddWindow(Scope.Window.ToSharedRef(), true);
    Tick(Slate);
    const TSharedPtr<SCkDebug_EntityRef> EntityRef = StaticCastSharedPtr<SCkDebug_EntityRef>(FindWidget(Region, TEXT("SCkDebug_EntityRef")));
    const TSharedPtr<STextBlock> EntityText = EntityRef.IsValid()
        ? StaticCastSharedPtr<STextBlock>(FindWidget(EntityRef.ToSharedRef(), TEXT("STextBlock"))) : nullptr;
    if (!TestTrue(TEXT("Authored entity reference mounts the real shared widget"), EntityRef.IsValid() && EntityText.IsValid())) { return false; }

    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{120.0f, 20.0f}, FSlateLayoutTransform{});
    const FPointerEvent LeftClick{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::LeftMouseButton},
        EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    const FPointerEvent RightClick{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::RightMouseButton},
        EKeys::RightMouseButton, 0.0f, FModifierKeysState{}};
    const FString ExpectedPreviewText = ck::debug_axes::Make_EntityIdText(StyleSettings->Selection, Name.ToString(), Id.ToString()).ToString();
    TestEqual(TEXT("Entity reference forwards authored preview id and name into the real visual composition"), EntityText->GetText().ToString(), ExpectedPreviewText);
    const TSharedPtr<IToolTip> Tooltip = EntityText->GetToolTip();
    const TSharedPtr<SWidget> TooltipContent = Tooltip.IsValid() ? TSharedPtr<SWidget>(Tooltip->GetContentWidget()) : nullptr;
    const TSharedPtr<STextBlock> TooltipText = TooltipContent.IsValid()
        ? StaticCastSharedPtr<STextBlock>(FindWidget(TooltipContent.ToSharedRef(), TEXT("STextBlock"))) : nullptr;
    TestTrue(TEXT("Delegate navigation tooltip is truthful and does not promise the ECS Debugger"), TooltipText.IsValid()
        && TooltipText->GetText().ToString() == TEXT("Click to follow entity reference — right-click to copy"));
    const FCursorReply ActiveCursor = EntityRef->OnCursorQuery(Geometry, LeftClick);
    TestTrue(TEXT("Dispatchable entity reference advertises a hand cursor"), ActiveCursor.IsEventHandled() && ActiveCursor.GetCursorType() == EMouseCursor::Hand);
    TestTrue(TEXT("Entity reference left click dispatches its authored action"), EntityRef->OnMouseButtonDown(Geometry, LeftClick).IsEventHandled());
    TestTrue(TEXT("Entity reference action receives the authored stable key"), FirstActionCount == 1 && FirstActionKey == TEXT("stable-42"));
    TestTrue(TEXT("Entity reference preserves the shared right-click copy surface"), EntityRef->OnMouseButtonDown(Geometry, RightClick).IsEventHandled());
    Slate.DismissAllMenus();
    Tick(Slate);

    CanDispatch = false;
    Tick(Slate);
    const FCursorReply DisabledCursor = EntityRef->OnCursorQuery(Geometry, LeftClick);
    TestTrue(TEXT("Dispatch gate removes the navigation cursor and makes clicks inert"), !DisabledCursor.IsEventHandled()
        && !EntityRef->OnMouseButtonDown(Geometry, LeftClick).IsEventHandled() && FirstActionCount == 1);
    CanDispatch = true;
    Tick(Slate);

    const int64 Revision = View->GetRevision();
    const auto Rejected = View->TryReload(Source.Replace(TEXT(" item-action=\"navigate\""), TEXT("")), TEXT(""), TEXT("DebugEntityRefRejected"));
    if (!TestTrue(TEXT("Missing entity action rejects without replacing the retained reference"), !Rejected.Succeeded && !Rejected.Errors.IsEmpty()
        && View->GetRevision() == Revision && FindWidget(Region, TEXT("SCkDebug_EntityRef")) == EntityRef)) { return false; }
    EntityRef->OnMouseButtonDown(Geometry, LeftClick);
    TestEqual(TEXT("Rejected reload keeps the accepted action only"), FirstActionCount, 2);

    const auto Reloaded = View->TryReload(Source.Replace(TEXT("item-action=\"navigate\""), TEXT("item-action=\"navigate-next\""))
        .Replace(TEXT("show-name=\"true\""), TEXT("show-name=\"false\"")), TEXT(""), TEXT("DebugEntityRefRebound"));
    if (!TestTrue(TEXT("Compatible entity reference reload retains the widget"), Reloaded.Succeeded
        && FindWidget(Region, TEXT("SCkDebug_EntityRef")) == EntityRef)) { return false; }
    const FString ExpectedIdOnlyText = ck::debug_axes::Make_EntityIdText(StyleSettings->Selection, FString{}, Id.ToString()).ToString();
    TestEqual(TEXT("Compatible reload applies show-name false to the retained entity reference composition"), EntityText->GetText().ToString(), ExpectedIdOnlyText);
    EntityRef->OnMouseButtonDown(Geometry, LeftClick);
    if (!TestTrue(TEXT("Compatible reload atomically replaces the action and makes the stale action inert"), FirstActionCount == 2
        && SecondActionCount == 1 && SecondActionKey == TEXT("stable-42"))) { return false; }

    View.Reset();
    const FCursorReply ReleasedCursor = EntityRef->OnCursorQuery(Geometry, LeftClick);
    TestTrue(TEXT("Released view leaves held entity reference inert"), !ReleasedCursor.IsEventHandled()
        && !EntityRef->OnMouseButtonDown(Geometry, LeftClick).IsEventHandled() && SecondActionCount == 1);
    return true;
}

#endif
