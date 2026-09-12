#include "Misc/AutomationTest.h"
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "CkObjectPoolingDebugger/Window/SCkObjectPoolingDebuggerWindow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiStyledButton.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/Children.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace ck_object_pooling_debugger_authored_tests
{
    auto Tick(FSlateApplication& S) -> void
    {
        S.PumpMessages();
        S.Tick();
        S.Tick();
    }
    auto FindTaggedWidget(const TSharedRef<SWidget>& R, FName Tag) -> TSharedPtr<SWidget>
    {
        if (R->GetTag() == Tag)
            return R;
        const FChildren* C = R->GetChildren();
        for (int32 I = 0; C && I < C->Num(); ++I)
            if (auto F = FindTaggedWidget(ConstCastSharedRef<SWidget>(C->GetChildAt(I)), Tag))
                return F;
        return {};
    }
    auto FindDescendantByType(const TSharedRef<SWidget>& R, const FString& T) -> TSharedPtr<SWidget>
    {
        const FString Type = R->GetTypeAsString();
        if (Type == T || Type.StartsWith(T + TEXT("<")))
            return R;
        const FChildren* C = R->GetChildren();
        for (int32 I = 0; C && I < C->Num(); ++I)
            if (auto F = FindDescendantByType(ConstCastSharedRef<SWidget>(C->GetChildAt(I)), T))
                return F;
        return {};
    }
    auto FindText(const TSharedRef<SWidget>& R, const FString& T) -> bool
    {
        if (R->GetTypeAsString() == TEXT("STextBlock") &&
            StaticCastSharedRef<STextBlock>(R)->GetText().ToString().Contains(T))
            return true;
        if (R->GetTypeAsString() == TEXT("SCkFlexText") &&
            StaticCastSharedRef<SCkFlexText>(R)->GetText().ToString().Contains(T))
            return true;
        const FChildren* C = R->GetChildren();
        for (int32 I = 0; C && I < C->Num(); ++I)
            if (FindText(ConstCastSharedRef<SWidget>(C->GetChildAt(I)), T))
                return true;
        return false;
    }
    auto GetTaggedText(const TSharedRef<SWidget>& R, FName Tag) -> FString
    {
        const TSharedPtr<SWidget> Tagged = FindTaggedWidget(R, Tag);
        if (!Tagged.IsValid()) { return {}; }
        if (Tagged->GetTypeAsString() == TEXT("STextBlock"))
            return StaticCastSharedPtr<STextBlock>(Tagged)->GetText().ToString();
        if (Tagged->GetTypeAsString() == TEXT("SCkFlexText"))
            return StaticCastSharedPtr<SCkFlexText>(Tagged)->GetText().ToString();
        if (const TSharedPtr<SWidget> FlexText = FindDescendantByType(Tagged.ToSharedRef(), TEXT("SCkFlexText")); FlexText.IsValid())
            return StaticCastSharedPtr<SCkFlexText>(FlexText)->GetText().ToString();
        if (const TSharedPtr<SWidget> TextBlock = FindDescendantByType(Tagged.ToSharedRef(), TEXT("STextBlock")); TextBlock.IsValid())
            return StaticCastSharedPtr<STextBlock>(TextBlock)->GetText().ToString();
        return {};
    }
    auto FindTaggedConcrete(const TSharedRef<SWidget>& R, FName Tag, const FString& Type) -> TSharedPtr<SWidget>
    {
        const auto Tagged = FindTaggedWidget(R, Tag);
        return Tagged.IsValid() ? FindDescendantByType(Tagged.ToSharedRef(), Type) : TSharedPtr<SWidget>{};
    }
    auto WidgetPathContains(const FWidgetPath& P, const TSharedRef<SWidget>& W) -> bool
    {
        for (int32 Index = 0; Index < P.Widgets.Num(); ++Index)
            if (P.Widgets[Index].Widget == W)
                return true;
        return false;
    }
    auto Click(FSlateApplication& S, const TSharedRef<SWidget>& W) -> bool
    {
        auto Win = S.FindWidgetWindow(W);
        if (!Win.IsValid() || !Win->GetNativeWindow().IsValid())
            return false;
        Win->BringToFront(true);
        Tick(S);
        auto G = W->GetCachedGeometry();
        if (G.GetLocalSize().X <= 0 || G.GetLocalSize().Y <= 0)
            return false;
        auto Pos = G.LocalToAbsolute(G.GetLocalSize() * .5f);
        const TSet<FKey> N;
        const TSet<FKey> L{EKeys::LeftMouseButton};
        FPointerEvent M(0, FSlateApplication::CursorPointerIndex, Pos, Pos, N, EKeys::Invalid, 0, FModifierKeysState{});
        FPointerEvent D(0, FSlateApplication::CursorPointerIndex, Pos, Pos, L, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        FPointerEvent U(0, FSlateApplication::CursorPointerIndex, Pos, Pos, N, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        S.SetCursorPos(Pos);
        S.ProcessMouseMoveEvent(M, true);
        if (!WidgetPathContains(S.LocateWindowUnderMouse(Pos, S.GetInteractiveTopLevelWindows(), false, 0), W))
            return false;
        const bool B = S.ProcessMouseButtonDownEvent(Win->GetNativeWindow(), D);
        const bool E = S.ProcessMouseButtonUpEvent(U);
        Tick(S);
        return B && E;
    }
    auto ReplaceText(FSlateApplication& S, const TSharedRef<SEditableText>& E, const FString& T) -> bool
    {
        if (!Click(S, E) || S.GetUserFocusedWidget(0) != E)
            return false;
        const FModifierKeysState C{false, false, true, false, false, false, false, false, false};
        if (!S.ProcessKeyDownEvent(FKeyEvent{EKeys::A, C, 0, false, 0, 0}))
            return false;
        for (TCHAR Ch : T)
            if (!S.ProcessKeyCharEvent(FCharacterEvent{Ch, FModifierKeysState{}, 0, false}))
                return false;
        bool B = S.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0});
        Tick(S);
        return B && E->GetText().ToString() == T;
    }
    auto SaveCapture(FSlateApplication& S, const TSharedRef<SWidget>& R, const FString& P) -> bool
    {
        TArray<FColor> Px;
        FIntVector Z = FIntVector::ZeroValue;
        if (!S.TakeScreenshot(R, Px, Z) || Z.X <= 0 || Z.Y <= 0 || Px.Num() < Z.X * Z.Y)
            return false;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(P), true);
        return FImageUtils::SaveImageByExtension(*P, FImageView(Px.GetData(), Z.X, Z.Y, ERawImageFormat::BGRA8));
    }
} // namespace ck_object_pooling_debugger_authored_tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkObjectPoolingDebugger_AuthoredWindow, "Ck.UiAuthoring.ObjectPoolingDebugger.Authored.Window",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
auto FCkObjectPoolingDebugger_AuthoredWindow::RunTest(const FString&) -> bool
{
    using namespace ck_object_pooling_debugger_authored_tests;
    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Object Pooling authored window test requires Slate."));
        return false;
    }
    FSlateApplication& S = FSlateApplication::Get();
    TSharedPtr<SCkObjectPoolingDebuggerWindow> Panel = SNew(SCkObjectPoolingDebuggerWindow);
    TSharedPtr<SWindow> Host = SNew(SWindow)
                                   .AutoCenter(EAutoCenter::None)
                                   .ClientSize(FVector2D{1200, 760})
                                   .CreateTitleBar(false)
                                   .HasCloseButton(false)[Panel.ToSharedRef()];
    ON_SCOPE_EXIT
    {
        if (Host.IsValid())
            S.DestroyWindowImmediately(Host.ToSharedRef());
    };
    S.AddWindow(Host.ToSharedRef(), true);
    Tick(S);
    auto View = Panel->Get_AuthoredView();
    if (!View.IsValid() || !View->GetLastResult().Succeeded)
    {
        AddError(View.IsValid()
            ? FString::Printf(TEXT("Real Object Pooling window failed to mount authored resources: %s"),
                *FString::Join(View->GetLastResult().Errors, TEXT("\n")))
            : TEXT("Real Object Pooling window did not retain its authored view."));
        return false;
    }
    const auto Controls = View->GetRegion(TEXT("controls"));
    const auto Context = View->GetRegion(TEXT("context"));
    const auto Search = View->GetRegion(TEXT("search"));
    const auto Overview = View->GetRegion(TEXT("overview"));
    const auto CW = FindTaggedWidget(Controls, TEXT("pool-in-use-only"));
    const auto FW = FindTaggedWidget(Search, TEXT("pool-filter"));
    const auto HW = FindTaggedWidget(Search, TEXT("pool-highlight"));
    const auto EW = FindTaggedWidget(Controls, TEXT("pool-export"));
    const auto CWConcrete = CW.IsValid() ? FindDescendantByType(CW.ToSharedRef(), TEXT("SCheckBox")) : TSharedPtr<SWidget>{};
    const auto FWConcrete = FW.IsValid() ? FindDescendantByType(FW.ToSharedRef(), TEXT("SSearchBox")) : TSharedPtr<SWidget>{};
    const auto HWConcrete = HW.IsValid() ? FindDescendantByType(HW.ToSharedRef(), TEXT("SSearchBox")) : TSharedPtr<SWidget>{};
    const auto EWConcrete = EW.IsValid() ? FindDescendantByType(EW.ToSharedRef(), TEXT("SCkUiStyledButton")) : TSharedPtr<SWidget>{};
    auto FEW = FW.IsValid() ? FindDescendantByType(FW.ToSharedRef(), TEXT("SEditableText")) : TSharedPtr<SWidget>{};
    auto HEW = HW.IsValid() ? FindDescendantByType(HW.ToSharedRef(), TEXT("SEditableText")) : TSharedPtr<SWidget>{};
    if (!TestTrue(TEXT("Concrete checkbox, searches, editors and styled export are mounted"),
                  CW.IsValid() && CWConcrete.IsValid() && FW.IsValid() && FWConcrete.IsValid() && HW.IsValid() &&
                      HWConcrete.IsValid() && FEW.IsValid() && HEW.IsValid() && EW.IsValid() && EWConcrete.IsValid()))
        return false;
    auto Check = StaticCastSharedRef<SCheckBox>(CWConcrete.ToSharedRef());
    auto Filter = StaticCastSharedRef<SSearchBox>(FWConcrete.ToSharedRef());
    auto Highlight = StaticCastSharedRef<SSearchBox>(HWConcrete.ToSharedRef());
    auto FEdit = StaticCastSharedRef<SEditableText>(FEW.ToSharedRef());
    auto HEdit = StaticCastSharedRef<SEditableText>(HEW.ToSharedRef());
    auto Live = FindTaggedWidget(Context, TEXT("pool-status-live")),
         Missing = FindTaggedWidget(Context, TEXT("pool-status-missing"));
    TestTrue(TEXT("Exact status IDs and text start missing and live hidden"),
             Live.IsValid() && Missing.IsValid() && !Live->GetVisibility().IsVisible() &&
                 Missing->GetVisibility().IsVisible() && FindText(Context, TEXT("Subsystem Live")) &&
                 FindText(Context, TEXT("No Subsystem")) && FindText(Context, TEXT("start PIE")));
    const auto HeroCard = FindTaggedWidget(Overview, TEXT("pool-hero"));
    const FString OverviewInUseText = GetTaggedText(Overview, TEXT("pool-in-use"));
    const FString HeroCurrentText = GetTaggedText(Overview, TEXT("pool-hero-current"));
    const auto HeroSparklineWidget = FindTaggedConcrete(Overview, TEXT("pool-hero-chart"), TEXT("SCkDebug_Sparkline"));
    const TSharedPtr<SCkDebug_Sparkline> HeroSparkline = HeroSparklineWidget.IsValid()
        ? StaticCastSharedPtr<SCkDebug_Sparkline>(HeroSparklineWidget) : nullptr;
    const TSharedPtr<FCkUiFloatSeries> TotalInUseSeries = Panel->Get_TotalInUseSeries();
    const TSharedPtr<const FCkUiFloatSeries> MountedTotalInUseSeries = HeroSparkline.IsValid()
        ? HeroSparkline->Get_FloatSeries().Pin() : nullptr;
    if (!TestTrue(TEXT("Header, list, authored hero card/chart and gather text are present"),
             FindText(Panel.ToSharedRef(), TEXT("CLASS / ARCHETYPE")) &&
                 FindDescendantByType(Panel.ToSharedRef(), TEXT("SListView")).IsValid() &&
                 HeroCard.IsValid() && FindText(HeroCard.ToSharedRef(), TEXT("IN USE — ALL POOLS")) &&
                 !OverviewInUseText.IsEmpty() && HeroCurrentText == OverviewInUseText &&
                 HeroSparkline.IsValid() &&
                 FindText(Panel.ToSharedRef(), TEXT("gather:"))))
        return false;
    const int64 TotalInUseRevision = TotalInUseSeries.IsValid() ? TotalInUseSeries->GetRevision() : -1;
    TestTrue(TEXT("Authored hero holds the live host-owned total-in-use series"),
             TotalInUseSeries.IsValid() && MountedTotalInUseSeries.IsValid() &&
                 MountedTotalInUseSeries.Get() == TotalInUseSeries.Get());
    TestTrue(TEXT("Host updates authored hero samples in place"),
             TotalInUseSeries.IsValid() && TotalInUseSeries->TrySetSamples({1.0f, 3.0f, 2.0f}).Succeeded &&
                 TotalInUseSeries->GetRevision() == TotalInUseRevision + 1 &&
                 TotalInUseSeries->GetSamples() == TArray<float>{1.0f, 3.0f, 2.0f} &&
                 HeroSparkline->Get_FloatSeries().Pin().Get() == TotalInUseSeries.Get());
    TestTrue(TEXT("Physical filter callback updates state"),
             ReplaceText(S, FEdit, TEXT("PoolFilter")) && Panel->Get_FilterString() == TEXT("PoolFilter"));
    TestTrue(TEXT("Physical highlight callback updates state"),
             ReplaceText(S, HEdit, TEXT("PoolHighlight")) && Panel->Get_HighlightString() == TEXT("PoolHighlight"));
    TestTrue(TEXT("Physical checkbox callback updates state"),
             Click(S, Check) && Panel->Get_ShowInUseOnly() && Check->GetCheckedState() == ECheckBoxState::Checked);
    auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!TestTrue(TEXT("Debugger plugin resolves resources"), Plugin.IsValid()))
        return false;
    auto Dir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    auto Cap = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ObjectPoolingDebugger"));
    TestTrue(TEXT("Wide capture saves PNG"),
             SaveCapture(S, Panel.ToSharedRef(), FPaths::Combine(Cap, TEXT("Window-Wide.png"))));
    Host->Resize(FVector2D{560, 620});
    Tick(S);
    TestTrue(TEXT("Narrow capture saves PNG"),
             SaveCapture(S, Panel.ToSharedRef(), FPaths::Combine(Cap, TEXT("Window-Narrow.png"))));
    Host->Resize(FVector2D{1200, 760});
    Tick(S);
    int64 Rev = View->GetRevision();
    const int64 RevisionBeforeReload = TotalInUseSeries->GetRevision();
    TestTrue(TEXT("Compatible ReloadFiles succeeds"),
             View->ReloadFiles(FPaths::Combine(Dir, TEXT("ObjectPoolingDebugger.ui.html")),
                               FPaths::Combine(Dir, TEXT("ObjectPoolingDebugger.ui.css")))
                 .Succeeded);
    const auto ReloadedHeroCard = FindTaggedWidget(Overview, TEXT("pool-hero"));
    const auto ReloadedHeroSparklineWidget = FindTaggedConcrete(Overview, TEXT("pool-hero-chart"), TEXT("SCkDebug_Sparkline"));
    const TSharedPtr<SCkDebug_Sparkline> ReloadedHeroSparkline = ReloadedHeroSparklineWidget.IsValid()
        ? StaticCastSharedPtr<SCkDebug_Sparkline>(ReloadedHeroSparklineWidget) : nullptr;
    if (!TestTrue(TEXT("Compatible reload remounts the authored hero sparkline"), ReloadedHeroSparkline.IsValid()))
        return false;
    const FString ReloadedOverviewInUseText = GetTaggedText(Overview, TEXT("pool-in-use"));
    const FString ReloadedHeroCurrentText = GetTaggedText(Overview, TEXT("pool-hero-current"));
    TestTrue(TEXT("Compatible reload preserves View and increments revision"),
             Panel->Get_AuthoredView() == View && View->GetRevision() > Rev);
    TestTrue(TEXT("Compatible reload preserves four region mounts"),
             View->GetRegion(TEXT("controls")) == Controls && View->GetRegion(TEXT("context")) == Context &&
                 View->GetRegion(TEXT("search")) == Search && View->GetRegion(TEXT("overview")) == Overview);
    TestTrue(TEXT("Compatible reload preserves hero card, title, and live current value"),
             ReloadedHeroCard.IsValid() && FindText(ReloadedHeroCard.ToSharedRef(), TEXT("IN USE — ALL POOLS")) &&
                 !ReloadedOverviewInUseText.IsEmpty() && ReloadedHeroCurrentText == ReloadedOverviewInUseText);
    TestTrue(TEXT("Compatible reload remounts hero chart with the same live series and revision"),
             ReloadedHeroSparkline.IsValid() && ReloadedHeroSparkline->Get_FloatSeries().Pin().Get() == TotalInUseSeries.Get() &&
                 TotalInUseSeries->GetRevision() == RevisionBeforeReload);
    TestTrue(TEXT("Compatible reload preserves control widget identities"),
             FindTaggedConcrete(Controls, TEXT("pool-in-use-only"), TEXT("SCheckBox")) == CWConcrete &&
                 FindTaggedConcrete(Search, TEXT("pool-filter"), TEXT("SSearchBox")) == FWConcrete &&
                 FindTaggedConcrete(Search, TEXT("pool-highlight"), TEXT("SSearchBox")) == HWConcrete);
    TestTrue(TEXT("Compatible reload preserves drafts and filter state"),
             Filter->GetText().ToString() == TEXT("PoolFilter") && Highlight->GetText().ToString() == TEXT("PoolHighlight") &&
                 Panel->Get_ShowInUseOnly());
    int64 Accepted = View->GetRevision();
    const int64 AcceptedSeriesRevision = TotalInUseSeries->GetRevision();
    TestFalse(TEXT("Rejected TryReload fails"), View->TryReload(TEXT("<ui>"), TEXT("")).Succeeded);
    TestTrue(TEXT("Rejected reload preserves the accepted View revision"), View->GetRevision() == Accepted);
    TestTrue(TEXT("Rejected reload preserves control widget identities"),
             FindTaggedConcrete(Controls, TEXT("pool-in-use-only"), TEXT("SCheckBox")) == CWConcrete &&
                 FindTaggedConcrete(Search, TEXT("pool-filter"), TEXT("SSearchBox")) == FWConcrete &&
                 FindTaggedConcrete(Search, TEXT("pool-highlight"), TEXT("SSearchBox")) == HWConcrete);
    TestTrue(TEXT("Rejected reload preserves hero card and chart identities"),
             FindTaggedWidget(Overview, TEXT("pool-hero")) == ReloadedHeroCard &&
                 FindTaggedConcrete(Overview, TEXT("pool-hero-chart"), TEXT("SCkDebug_Sparkline")) == ReloadedHeroSparklineWidget);
    TestTrue(TEXT("Rejected reload preserves hero title and live current value"),
             ReloadedHeroCard.IsValid() && FindText(ReloadedHeroCard.ToSharedRef(), TEXT("IN USE — ALL POOLS")) &&
                 GetTaggedText(Overview, TEXT("pool-hero-current")) == ReloadedOverviewInUseText);
    TestTrue(TEXT("Rejected reload preserves live series identity and revision"),
             ReloadedHeroSparkline.IsValid() && ReloadedHeroSparkline->Get_FloatSeries().Pin().Get() == TotalInUseSeries.Get() &&
                 TotalInUseSeries->GetRevision() == AcceptedSeriesRevision);
    TestTrue(TEXT("Rejected reload preserves drafts and filter state"),
             Panel->Get_FilterString() == TEXT("PoolFilter") && Panel->Get_HighlightString() == TEXT("PoolHighlight") &&
                 Panel->Get_ShowInUseOnly());
    TestTrue(TEXT("Callbacks remain live after rejected reload"),
             Click(S, Check) && !Panel->Get_ShowInUseOnly() && ReplaceText(S, FEdit, TEXT("PoolFilterLive")) &&
                 Panel->Get_FilterString() == TEXT("PoolFilterLive"));
    TWeakPtr<SCkObjectPoolingDebuggerWindow> Weak = Panel;
    S.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Panel.Reset();
    Tick(S);
    FEdit->SetText(FText::FromString(TEXT("released")));
    HEdit->SetText(FText::FromString(TEXT("released")));
    Check->SetIsChecked(ECheckBoxState::Checked);
    TestTrue(TEXT("Owner release expires panel; held widgets are safe and inert"), !Weak.IsValid());
    return true;
}
#endif
