#include "CkAudioDebugger/Window/SCkAudioDebuggerWindow.h"
#include "../../CkAudioDebugger_Module.h"

#include "CkAudio/AudioTrack/CkAudioTrack_Fragment.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkSlateLayout/CkFlexText.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_authored_shell_tests
{
    struct FScopedStyleSelection
    {
        FScopedStyleSelection()
        {
            if (const auto* Settings = UCkDebuggerStyleSettings::Get())
            {
                Selection = Settings->Selection;
                ProfileName = Settings->ActiveProfileName;
            }
        }

        ~FScopedStyleSelection()
        {
            if (auto* Settings = UCkDebuggerStyleSettings::Get_Mutable())
            {
                Settings->Selection = Selection;
                Settings->ActiveProfileName = ProfileName;
                Settings->NotifyChanged();
            }
        }

        FCkDebuggerStyleSelection Selection;
        FString ProfileName;
    };

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto TaggedText(const TSharedRef<SWidget>& InRoot, const TCHAR* InTag) -> FString
    {
        const TSharedPtr<SWidget> Widget = FindTaggedWidget(InRoot, FName{InTag});
        return Widget.IsValid() && Widget->GetTypeAsString() == TEXT("SCkFlexText")
            ? StaticCastSharedPtr<SCkFlexText>(Widget)->GetText().ToString() : FString{};
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InTarget) -> bool
    {
        if (InRoot == InTarget) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget))
            { return true; }
        }
        return false;
    }

    auto SubtreeHasText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> bool
    {
        if (InRoot->GetTypeAsString() == TEXT("STextBlock")
            && StaticCastSharedRef<STextBlock>(InRoot)->GetText().ToString() == InText)
        { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (SubtreeHasText(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText))
            { return true; }
        }
        return false;
    }

    auto FindButtonWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && SubtreeHasText(InRoot, InText))
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButtonWithText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindCheckBoxWithText(const TSharedRef<SWidget>& InRoot, const FString& InText) -> TSharedPtr<SCheckBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCheckBox") && SubtreeHasText(InRoot, InText))
        { return StaticCastSharedRef<SCheckBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCheckBox> Found = FindCheckBoxWithText(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InText); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto WidgetPathContains(const FWidgetPath& InPath, const TSharedRef<SWidget>& InWidget) -> bool
    {
        for (int32 Index = 0; Index < InPath.Widgets.Num(); ++Index)
        { if (InPath.Widgets[Index].Widget == InWidget) { return true; } }
        return false;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        Window->BringToFront(true);
        TickSlate(InSlate);
        InSlate.ReleaseAllPointerCapture(0);
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
        const TSet<FKey> NoButtons;
        const TSet<FKey> LeftDown{EKeys::LeftMouseButton};
        const FPointerEvent MoveEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::Invalid, 0.0f, FModifierKeysState{});
        const FPointerEvent DownEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            LeftDown, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        const FPointerEvent UpEvent(0, FSlateApplication::CursorPointerIndex, Position, Position,
            NoButtons, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(MoveEvent, true);
        const FWidgetPath Path = InSlate.LocateWindowUnderMouse(
            Position, InSlate.GetInteractiveTopLevelWindows(), false, 0);
        if (NOT WidgetPathContains(Path, InWidget)) { return false; }
        const bool DownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), DownEvent);
        InSlate.ProcessMouseButtonUpEvent(UpEvent);
        TickSlate(InSlate);
        return DownHandled;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_AuthoredShell,
    "Ck.AudioDebugger.AuthoredShell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_AuthoredShell::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_authored_shell_tests;

    const auto StyleGuard = FScopedStyleSelection{};
    auto* StyleSettings = UCkDebuggerStyleSettings::Get_Mutable();
    if (NOT TestNotNull(TEXT("Audio authored-shell test requires debugger style settings"), StyleSettings))
    { return false; }
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Normal;
    StyleSettings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Rounded;
    StyleSettings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Layered;
    StyleSettings->NotifyChanged();

    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Audio authored-shell test requires Slate."));
        return false;
    }

    auto& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> HostWindow;
    ON_SCOPE_EXIT
    {
        if (HostWindow.IsValid()) { Slate.DestroyWindowImmediately(HostWindow.ToSharedRef()); }
    };

    TSharedPtr<SCkAudioDebuggerWindow> DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);

    TSharedPtr<FCkUiView> View = DebuggerWindow->_AuthoredShellView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored shell"),
        View.IsValid() && View->GetLastResult().Succeeded))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }
    TSharedPtr<FCkUiView> CrossfadeView = DebuggerWindow->_AuthoredCrossfadeView;
    if (NOT TestTrue(TEXT("production Audio window admits its authored Crossfade page"),
        CrossfadeView.IsValid() && CrossfadeView->GetLastResult().Succeeded))
    {
        if (CrossfadeView.IsValid()) { AddError(FString::Join(CrossfadeView->GetLastResult().Errors, TEXT("\n"))); }
        return false;
    }

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    TestTrue(TEXT("authored shell retains three production native boundaries and authors live summary cards"),
        DebuggerWindow->_Tabs.IsValid()
            && DebuggerWindow->_StatCards.IsValid()
            && DebuggerWindow->_FilterRow.IsValid()
            && DebuggerWindow->_PageSwitcher.IsValid()
            && ContainsWidget(Main, DebuggerWindow->_Tabs.ToSharedRef())
            && NOT ContainsWidget(Main, DebuggerWindow->_StatCards.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(Main, DebuggerWindow->_PageSwitcher.ToSharedRef())
            && TaggedText(Main, TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && TaggedText(Main, TEXT("audio-stat-audible-label")) == TEXT("Audible")
            && TaggedText(Main, TEXT("audio-stat-fading-label")) == TEXT("Fading")
            && TaggedText(Main, TEXT("audio-stat-virtualized-label")) == TEXT("Virtualized")
            && TaggedText(Main, TEXT("audio-stat-concurrency")) == TEXT("0 / 0"));
    TestEqual(TEXT("production page switcher retains all six Audio pages"),
        DebuggerWindow->_PageSwitcher->GetNumWidgets(), 6);
    TestEqual(TEXT("production Audio shell preserves Tracks as its default page"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex(), 1);
    TestTrue(TEXT("authored shell exposes horizontal overflow reachability"),
        View->GetScroll(TEXT("audio-shell-scroll")).IsValid());
    const TSharedRef<SWidget> CrossfadeMain = CrossfadeView->GetRegion(TEXT("main"));
    TestTrue(TEXT("dedicated Crossfade page authors its ordinary presentation around the exact dual-series plot"),
        TaggedText(CrossfadeMain, TEXT("audio-crossfade-title")) == TEXT("Crossfade lane")
            && TaggedText(CrossfadeMain, TEXT("audio-crossfade-subtitle")) == TEXT("recent history of _CurrentVolume")
            && TaggedText(CrossfadeMain, TEXT("audio-crossfade-legend")) == TEXT("(nothing playing)")
            && ContainsWidget(CrossfadeMain, DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_CrossfadePagePlot->Get_Samples() == DebuggerWindow->_CrossfadeSeriesA
            && DebuggerWindow->_CrossfadePagePlot->Get_BandSamples() == DebuggerWindow->_CrossfadeSeriesB
            && DebuggerWindow->_CrossfadePagePlot->Get_BandFillOpacity() == 0.0f
            && DebuggerWindow->_CrossfadePagePlot->Get_DesiredSize() == FVector2D{320.0f, 220.0f});

    const TSharedPtr<SWidget> AuthoredValueWidget = FindTaggedWidget(Main, TEXT("audio-stat-concurrency"));
    const bool AuthoredValueIsText = AuthoredValueWidget.IsValid()
        && AuthoredValueWidget->GetTypeAsString() == TEXT("SCkFlexText");
    const float NormalValueFontSize = AuthoredValueIsText
        ? StaticCastSharedPtr<SCkFlexText>(AuthoredValueWidget)->GetFont().Size : 0.0f;
    const TSharedPtr<SWidget> LayeredWrap = FindTaggedWidget(Main, TEXT("audio-stat-concurrency-wrap"));
    const TSharedPtr<SWidget> RoundedCard = FindTaggedWidget(Main, TEXT("audio-stat-concurrency-card"));
    const float LayeredWrapHeight = LayeredWrap.IsValid() ? LayeredWrap->GetDesiredSize().Y : 0.0f;
    const bool RoundedCardIsBorder = RoundedCard.IsValid()
        && RoundedCard->GetTypeAsString() == TEXT("SBorder");
    const FSlateBrush* RoundedCardBrush = RoundedCardIsBorder
        ? StaticCastSharedPtr<SBorder>(RoundedCard)->GetBorderImage() : nullptr;
    TestTrue(TEXT("authored Audio card starts from native rounded-card geometry"),
        RoundedCardBrush != nullptr
            && RoundedCardBrush->DrawAs == ESlateBrushDrawType::RoundedBox
            && RoundedCardBrush->OutlineSettings.CornerRadii.X == CkStyle::RadiusL()
            && RoundedCardBrush->OutlineSettings.Width == CkStyle::RingWidth());

    const int64 RevisionBeforeGeometryChange = View->GetRevision();
    const int64 CrossfadeRevisionBeforeGeometryChange = CrossfadeView->GetRevision();
    StyleSettings->Selection.CornerStyle = ECkDebugAxis_CornerStyle::Sharp;
    StyleSettings->Selection.SurfaceElevation = ECkDebugAxis_SurfaceElevation::Flat;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const TSharedPtr<SWidget> FlatWrap = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-wrap"));
    const TSharedPtr<SWidget> SharpCard = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-card"));
    const bool SharpCardIsBorder = SharpCard.IsValid() && SharpCard->GetTypeAsString() == TEXT("SBorder");
    const FSlateBrush* SharpCardBrush = SharpCardIsBorder
        ? StaticCastSharedPtr<SBorder>(SharpCard)->GetBorderImage() : nullptr;
    TestTrue(TEXT("live corner and elevation axes republish authored Audio card geometry"),
        View->GetRevision() > RevisionBeforeGeometryChange
            && CrossfadeView->GetRevision() > CrossfadeRevisionBeforeGeometryChange
            && FlatWrap.IsValid() && FlatWrap->GetDesiredSize().Y < LayeredWrapHeight
            && SharpCardBrush != nullptr
            && SharpCardBrush->OutlineSettings.CornerRadii.X == 0.0f
            && SharpCardBrush->OutlineSettings.Width == CkStyle::RingWidth());

    const int64 RevisionBeforeTextChange = View->GetRevision();
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Large;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    const TSharedPtr<SWidget> RestyledValueWidget = FindTaggedWidget(
        View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency"));
    TestTrue(TEXT("live Style Lab revision republishes authored Audio card tokens without rebuilding the window"),
        AuthoredValueIsText
            && RestyledValueWidget.IsValid()
            && RestyledValueWidget->GetTypeAsString() == TEXT("SCkFlexText")
            && View->GetRevision() > RevisionBeforeTextChange
            && StaticCastSharedPtr<SCkFlexText>(RestyledValueWidget)->GetFont().Size > NormalValueFontSize);

    const TSharedPtr<SButton> CrossfadeTab = FindButtonWithText(
        DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Crossfade"));
    TestTrue(TEXT("physical nondefault tab selection routes through the production tab strip"),
        CrossfadeTab.IsValid() && Click(Slate, CrossfadeTab.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2
            && ContainsWidget(DebuggerWindow->_CrossfadePageHost.ToSharedRef(), CrossfadeView->GetRegion(TEXT("main"))));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Css;
    FString CrossfadeMarkup;
    FString CrossfadeCss;
    const FString Directory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"))
        : FString{};
    if (NOT TestTrue(TEXT("installed Audio shell resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.css")))
        && FFileHelper::LoadFileToString(CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")))
        && FFileHelper::LoadFileToString(CrossfadeCss, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.css")))))
    { return false; }

    const auto Revision = View->GetRevision();
    const FCkUiLoadResult Reloaded = View->TryReload(Markup, Css, TEXT("Audio compatible shell candidate"));
    TickSlate(Slate);
    if (NOT Reloaded.Succeeded) { AddError(FString::Join(Reloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible reload retains native boundaries, authored summary ownership and page state"),
        Reloaded.Succeeded && View->GetRevision() > Revision
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && NOT ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const int64 CrossfadeRevision = CrossfadeView->GetRevision();
    const TSharedPtr<TArray<float>> SeriesA = DebuggerWindow->_CrossfadeSeriesA;
    const TSharedPtr<TArray<float>> SeriesB = DebuggerWindow->_CrossfadeSeriesB;
    const FCkUiLoadResult CrossfadeReloaded = CrossfadeView->TryReload(
        CrossfadeMarkup, CrossfadeCss, TEXT("Audio compatible Crossfade candidate"));
    TickSlate(Slate);
    if (NOT CrossfadeReloaded.Succeeded) { AddError(FString::Join(CrossfadeReloaded.Errors, TEXT("\n"))); }
    TestTrue(TEXT("compatible Crossfade reload retains plot, series ownership and selected page"),
        CrossfadeReloaded.Succeeded && CrossfadeView->GetRevision() > CrossfadeRevision
            && DebuggerWindow->_CrossfadeSeriesA == SeriesA
            && DebuggerWindow->_CrossfadeSeriesB == SeriesB
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);
    const int64 CrossfadeRevisionBeforeReject = CrossfadeView->GetRevision();
    const FCkUiLoadResult CrossfadeRejected = CrossfadeView->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-crossfade-plot\"/></region></ui>"),
        TEXT(""), TEXT("Audio rejected Crossfade candidate"));
    TestFalse(TEXT("missing Crossfade plot is rejected atomically"), CrossfadeRejected.Succeeded);
    TestTrue(TEXT("rejected Crossfade reload keeps the accepted page and exact plot"),
        CrossfadeView->GetRevision() == CrossfadeRevisionBeforeReject
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    const auto RevisionBeforeReject = View->GetRevision();
    const FCkUiLoadResult Rejected = View->TryReload(
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>"),
        TEXT(""), TEXT("Audio rejected shell candidate"));
    TestFalse(TEXT("missing Audio native port is rejected atomically"), Rejected.Succeeded);
    TestTrue(TEXT("rejection reaches missing-binding admission"),
        FString::Join(Rejected.Errors, TEXT("\n")).Contains(TEXT("missing-audio-port")));
    TestTrue(TEXT("rejected reload leaves the committed shell and page state intact"),
        View->GetRevision() == RevisionBeforeReject
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef())
            && DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2);

    HostWindow->Resize(FVector2D{480.0f, 480.0f});
    TickSlate(Slate);
    TestTrue(TEXT("actual narrow Audio shell keeps horizontal overflow reachable"),
        View->GetScroll(TEXT("audio-shell-scroll"))->GetScrollOffsetOfEnd() > 0.0f);

    HostWindow->Resize(FVector2D{1100.0f, 240.0f});
    TickSlate(Slate);
    TestTrue(TEXT("short Audio shell lets its page body shrink without vertical clipping pressure"),
        DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y > 0.0f
            && DebuggerWindow->_PageSwitcher->GetCachedGeometry().GetLocalSize().Y < 320.0f);

    Slate.DestroyWindowImmediately(HostWindow.ToSharedRef());
    HostWindow.Reset();
    TickSlate(Slate);
    const TWeakPtr<FCkUiView> ReleasedView = View;
    const TWeakPtr<FCkUiView> ReleasedCrossfadeView = CrossfadeView;
    DebuggerWindow.Reset();
    View.Reset();
    CrossfadeView.Reset();
    TestFalse(TEXT("authored Audio view releases with its production window"), ReleasedView.IsValid());
    TestFalse(TEXT("authored Crossfade view releases with its production window"), ReleasedCrossfadeView.IsValid());

    const FString InvalidStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-audio-port\"/></region></ui>");
    const FString InvalidCrossfadeStartupMarkup =
        TEXT("<ui version=\"1\"><region name=\"main\"><native id=\"missing\" bind=\"missing-crossfade-plot\"/></region></ui>");
    bool MarkupRestored = false;
    bool CrossfadeMarkupRestored = false;
    ON_SCOPE_EXIT
    {
        if (NOT MarkupRestored)
        { FFileHelper::SaveStringToFile(Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html"))); }
        if (NOT CrossfadeMarkupRestored)
        { FFileHelper::SaveStringToFile(CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html"))); }
    };
    if (NOT TestTrue(TEXT("Audio fixture installs its valid-but-unbound startup candidate"),
        FFileHelper::SaveStringToFile(
            InvalidStartupMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")))
            && FFileHelper::SaveStringToFile(InvalidCrossfadeStartupMarkup,
                *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")))))
    { return false; }

    DebuggerWindow = SNew(SCkAudioDebuggerWindow);
    HostWindow = SNew(SWindow)
        .ClientSize(FVector2D{1100.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [DebuggerWindow.ToSharedRef()];
    Slate.AddWindow(HostWindow.ToSharedRef(), true);
    TickSlate(Slate);
    View = DebuggerWindow->_AuthoredShellView;
    TestTrue(TEXT("invalid startup resource mounts the complete native fallback"),
        View.IsValid() && NOT View->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeFallback
            && ContainsWidget(DebuggerWindow->_AuthoredShellHost.ToSharedRef(),
                DebuggerWindow->_PageSwitcher.ToSharedRef()));

    MarkupRestored = FFileHelper::SaveStringToFile(
        Markup, *FPaths::Combine(Directory, TEXT("AudioDebuggerShell.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid production resource"), MarkupRestored);
    StyleSettings->Selection.TextScale = ECkDebugAxis_TextScale::Small;
    StyleSettings->NotifyChanged();
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    TestTrue(TEXT("normal style-revision tick recovers the outer shell and admits the still-invalid Crossfade fallback"),
        View->GetLastResult().Succeeded && NOT DebuggerWindow->_UsingNativeFallback
            && DebuggerWindow->_AuthoredCrossfadeView.IsValid()
            && NOT DebuggerWindow->_AuthoredCrossfadeView->GetLastResult().Succeeded
            && DebuggerWindow->_UsingNativeCrossfadeFallback
            && ContainsWidget(DebuggerWindow->_CrossfadePageHost.ToSharedRef(), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_Tabs.ToSharedRef())
            && NOT ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_StatCards.ToSharedRef())
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency-label")) == TEXT("Active / max")
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_FilterRow.ToSharedRef())
            && ContainsWidget(View->GetRegion(TEXT("main")), DebuggerWindow->_PageSwitcher.ToSharedRef()));
    CrossfadeMarkupRestored = FFileHelper::SaveStringToFile(
        CrossfadeMarkup, *FPaths::Combine(Directory, TEXT("AudioDebuggerCrossfade.ui.html")));
    TestTrue(TEXT("Audio fixture restores the valid Crossfade resource"), CrossfadeMarkupRestored);
    DebuggerWindow->OnStyleRevisionChanged();
    TickSlate(Slate);
    CrossfadeView = DebuggerWindow->_AuthoredCrossfadeView;
    TestTrue(TEXT("bounded Crossfade polling recovers from native startup fallback with the exact retained plot"),
        CrossfadeView.IsValid() && CrossfadeView->GetLastResult().Succeeded
            && NOT DebuggerWindow->_UsingNativeCrossfadeFallback
            && ContainsWidget(CrossfadeView->GetRegion(TEXT("main")), DebuggerWindow->_CrossfadePagePlot.ToSharedRef())
            && TaggedText(CrossfadeView->GetRegion(TEXT("main")), TEXT("audio-crossfade-title")) == TEXT("Crossfade lane"));

    using namespace ck::registry_table;
    auto Registry = EnttRegistryType{};
    const auto RegistrySlot = Allocate(&Registry);
    ON_SCOPE_EXIT
    {
        DebuggerWindow->HandleSessionInvalidated();
        Free(RegistrySlot);
    };

    const auto DirectorEntity = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    auto TrackA = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    auto TrackB = FCk_Handle{FCk_Entity{Registry.create()}, RegistrySlot};
    TrackA.Add<ck::FFragment_AudioTrack_Params>();
    TrackA.Add<ck::FFragment_AudioTrack_Current>();
    TrackB.Add<ck::FFragment_AudioTrack_Params>();
    TrackB.Add<ck::FFragment_AudioTrack_Current>();

    auto Director = FCkAudioDebugger_DirectorInfo{};
    Director.DirectorEntity = DirectorEntity;
    Director.DirectorName = TEXT("Same director");
    Director.MaxConcurrentTracks = 4;

    auto& FixtureSnapshot = DebuggerWindow->_Collector._SnapshotOverrideForTests.Emplace();
    FixtureSnapshot.HasWorld = true;
    FixtureSnapshot.Directors.Add(Director);
    DebuggerWindow->_ObservedWorld = DebuggerWindow->DoGet_PieWorld();
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto EmptyDirectorSignature = DebuggerWindow->DoBuild_Signature();
    DebuggerWindow->DoRebuild_Structure();
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("an empty director participates in the structure signature and dedicated page"),
        NOT EmptyDirectorSignature.IsEmpty()
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.Num() == 1
            && DebuggerWindow->_DirectorPageSlots[0].ActiveText->GetText().ToString() == TEXT("0 / 4 active"));

    auto TrackInfoA = FCkAudioDebugger_TrackInfo{};
    TrackInfoA.TrackEntity = TrackA;
    TrackInfoA.TrackName = TEXT("Same track");
    TrackInfoA.State = ECk_AudioTrack_State::Playing;
    TrackInfoA.CurrentVolume = 0.5f;
    FixtureSnapshot.Directors[0].Tracks.Add(TrackInfoA);
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto TrackAStructureSignature = DebuggerWindow->DoBuild_Signature();
    const auto TrackAAllSignature = DebuggerWindow->DoBuild_AllTracksSignature();
    DebuggerWindow->DoRebuild_Structure();
    DebuggerWindow->DoUpdate_LiveValues();
    TestTrue(TEXT("both Audio pages receive the live director concurrency count"),
        DebuggerWindow->_DirectorSlots.Num() == 1
            && DebuggerWindow->_DirectorPageSlots.Num() == 1
            && DebuggerWindow->_DirectorSlots[0].ActiveText->GetText().ToString() == TEXT("1 / 4 active")
            && DebuggerWindow->_DirectorPageSlots[0].ActiveText->GetText().ToString() == TEXT("1 / 4 active")
            && TaggedText(View->GetRegion(TEXT("main")), TEXT("audio-stat-concurrency")) == TEXT("1 / 4"));

    const TSharedPtr<SButton> OverlayTab = FindButtonWithText(DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Overlay"));
    if (NOT TestTrue(TEXT("production Overlay tab is physically selectable"),
        OverlayTab.IsValid() && Click(Slate, OverlayTab.ToSharedRef())))
    { return false; }
    DebuggerWindow->Tick(DebuggerWindow->GetCachedGeometry(), FPlatformTime::Seconds(), 0.0f);
    const TSharedPtr<SCheckBox> HeldTrackAToggle = FindCheckBoxWithText(
        DebuggerWindow->_OverlayListBox.ToSharedRef(), TEXT("draw"));
    if (NOT TestTrue(TEXT("physical production overlay action targets the first same-name entity"),
        HeldTrackAToggle.IsValid() && Click(Slate, HeldTrackAToggle.ToSharedRef())
            && TrackA.Has<ck::FTag_AudioTrack_DebugDraw>()))
    { return false; }
    TrackA.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackA.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();
    HeldTrackAToggle->ToggleCheckedState();
    TestTrue(TEXT("held-control probe dispatches while its production generation is current"),
        TrackA.Has<ck::FTag_AudioTrack_DebugDraw>());
    TrackA.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackA.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();

    auto TrackInfoB = TrackInfoA;
    TrackInfoB.TrackEntity = TrackB;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    const auto TrackBStructureSignature = DebuggerWindow->DoBuild_Signature();
    const auto TrackBAllSignature = DebuggerWindow->DoBuild_AllTracksSignature();
    TestTrue(TEXT("same-label entity replacement changes both production structure signatures"),
        TrackBStructureSignature != TrackAStructureSignature
            && TrackBAllSignature != TrackAAllSignature);
    DebuggerWindow->Tick(DebuggerWindow->GetCachedGeometry(), FPlatformTime::Seconds(), 0.0f);
    HeldTrackAToggle->ToggleCheckedState();
    TestFalse(TEXT("held same-name predecessor action is revoked when the production list replaces it"),
        TrackA.Has<ck::FTag_AudioTrack_DebugDraw>());

    const TSharedPtr<SCheckBox> HeldTrackBToggle = FindCheckBoxWithText(
        DebuggerWindow->_OverlayListBox.ToSharedRef(), TEXT("draw"));
    if (NOT TestTrue(TEXT("replacement overlay action routes to the new same-name entity"),
        HeldTrackBToggle.IsValid() && Click(Slate, HeldTrackBToggle.ToSharedRef())
            && TrackB.Has<ck::FTag_AudioTrack_DebugDraw>()))
    { return false; }
    TrackB.Try_Remove<ck::FFragment_AudioTrack_Debug>();
    TrackB.Try_Remove<ck::FTag_AudioTrack_DebugDraw>();

    DebuggerWindow->DoRecord_VolumeHistory();
    DebuggerWindow->DoRecord_Events();
    const TSharedPtr<SButton> LiveCrossfadeTab = FindButtonWithText(DebuggerWindow->_Tabs.ToSharedRef(), TEXT("Crossfade"));
    if (NOT TestTrue(TEXT("production Crossfade tab remains physically selectable"),
        LiveCrossfadeTab.IsValid() && Click(Slate, LiveCrossfadeTab.ToSharedRef())))
    { return false; }
    DebuggerWindow->_VolumeHistory.Reset();
    TrackInfoA.TrackName = TEXT("Fade A");
    TrackInfoA.CurrentVolume = 0.25f;
    TrackInfoA.State = ECk_AudioTrack_State::FadingIn;
    TrackInfoB.TrackName = TEXT("Fade B");
    TrackInfoB.CurrentVolume = 0.75f;
    TrackInfoB.State = ECk_AudioTrack_State::FadingOut;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA, TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_VolumeHistory();
    TrackInfoA.CurrentVolume = 0.45f;
    TrackInfoB.CurrentVolume = 0.55f;
    FixtureSnapshot.Directors[0].Tracks = {TrackInfoA, TrackInfoB};
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->DoRecord_VolumeHistory();
    TestTrue(TEXT("production sampling drives both retained Crossfade series and authored legend"),
        DebuggerWindow->_PageSwitcher->GetActiveWidgetIndex() == 2
            && DebuggerWindow->_CrossfadeSeriesA->Num() == 2
            && DebuggerWindow->_CrossfadeSeriesB->Num() == 2
            && (*DebuggerWindow->_CrossfadeSeriesA)[0] == 0.75f
            && (*DebuggerWindow->_CrossfadeSeriesA)[1] == 0.55f
            && (*DebuggerWindow->_CrossfadeSeriesB)[0] == 0.25f
            && (*DebuggerWindow->_CrossfadeSeriesB)[1] == 0.45f
            && TaggedText(CrossfadeView->GetRegion(TEXT("main")), TEXT("audio-crossfade-legend"))
                == TEXT("Fade B   ·   Fade A"));
    const auto SessionFixture = FixtureSnapshot;
    UWorld* InvalidatedWorld = NewObject<UWorld>();
    UWorld* UnrelatedWorld = NewObject<UWorld>();
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(UnrelatedWorld);
    TestTrue(TEXT("unrelated world invalidation leaves the observed Audio session intact"),
        DebuggerWindow->_ObservedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_Collector.Get_Snapshot().HasWorld);
    ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Broadcast(InvalidatedWorld);
    HeldTrackBToggle->ToggleCheckedState();
    TestTrue(TEXT("matching world invalidation synchronously clears handle-backed Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld
            && DebuggerWindow->_DirectorSlots.IsEmpty()
            && DebuggerWindow->_DirectorPageSlots.IsEmpty()
            && DebuggerWindow->_TrackSlots.IsEmpty()
            && DebuggerWindow->_VolumeHistory.IsEmpty()
            && DebuggerWindow->_TrackWatch.IsEmpty()
            && DebuggerWindow->_OverlayListBox->GetChildren()->Num() == 0
            && DebuggerWindow->_SpatialSelectorBox->GetChildren()->Num() == 0
            && DebuggerWindow->_StatConcurrency->ToString() == TEXT("0 / 0"));
    TestFalse(TEXT("held production overlay action remains inert after world invalidation"),
        TrackB.Has<ck::FTag_AudioTrack_DebugDraw>());

    DebuggerWindow->_Collector._SnapshotOverrideForTests.Emplace(SessionFixture);
    DebuggerWindow->_Collector.Collect(nullptr);
    DebuggerWindow->_ObservedWorld = InvalidatedWorld;
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("session invalidation independently clears Audio state"),
        NOT DebuggerWindow->_Collector.Get_Snapshot().HasWorld
            && DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld);
    ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Broadcast();
    TestTrue(TEXT("repeated session invalidation preserves the blocked-world marker"),
        DebuggerWindow->_InvalidatedWorld.Get() == InvalidatedWorld);

    auto Module = FCkAudioDebuggerModule{};
    Module._DebuggerWindow = DebuggerWindow;
    Module._DebuggerTab = SNew(SDockTab);
    Module.HandleEnginePreExit();
    TestTrue(TEXT("module pre-exit releases Audio tab and window ownership without a close request"),
        NOT Module._DebuggerTab.IsValid() && NOT Module._DebuggerWindow.IsValid());
    return true;
}

#endif
