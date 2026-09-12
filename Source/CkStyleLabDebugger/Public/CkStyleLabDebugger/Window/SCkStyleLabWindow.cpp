#include "CkStyleLabDebugger/Window/SCkStyleLabWindow.h"

#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SNullWidget.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkStyleLabWindow::WindowId = FName(TEXT("StyleLabDebugger"));

namespace ck_style_lab_window
{
    auto ShellStyleTokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--style-lab-shell-surface"), TEXT("#") + CkStyle::BgRoot().ToFColorSRGB().ToHex()}};
    }
}

// ====================================================================================================================

auto
    SCkStyleLabWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    if (const auto* Settings = UCkDebuggerStyleSettings::Get())
    { _LastSeenRevision = Settings->Get_Revision(); }

    _ControlsPane = SNew(SCkStyleLab_ControlsPane)
        .OnSelectionChanged(FOnCkStyleLab_SelectionChanged::CreateSP(this, &SCkStyleLabWindow::OnSelectionChanged));
    _AuthoredShellHost = SNew(SBox);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(Get_WindowId())
            .ToolTabId(TEXT("CkStyleLabDebugger"))
            .StatusText(TAttribute<FText>::CreateSP(this, &SCkStyleLabWindow::Get_StatusText))
            .CommandGroups({
                FCkDebug_CommandGroup::Primary(TEXT("StylePreview"), FText::FromString(TEXT("Style preview controls")), Build_MenuActions())
            })
            .Content()
            [_AuthoredShellHost.ToSharedRef()]
    ];

    Build_AuthoredShell();
}

SCkStyleLabWindow::~SCkStyleLabWindow()
{
    if (_AuthoredShellHost.IsValid())
    { _AuthoredShellHost->SetContent(SNullWidget::NullWidget); }

    _AuthoredShellView.Reset();
    _ControlsPane.Reset();
    _AuthoredShellHost.Reset();
}

auto SCkStyleLabWindow::Build_NativeShellFallback() -> TSharedRef<SWidget>
{
    return SNew(SScrollBox)
        + SScrollBox::Slot().Padding(CkStyle::SpaceM)
            [_ControlsPane.ToSharedRef()];
}

auto SCkStyleLabWindow::Build_AuthoredShell() -> void
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        _AuthoredShellHost->SetContent(Build_NativeShellFallback());
        return;
    }

    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("style-lab-shell-controls"), _ControlsPane.ToSharedRef());
    const TSharedRef<FCkUiView> View = FCkUiView::Create(MoveTemp(NativeBindings), {},
        ck_style_lab_window::ShellStyleTokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()), {}, Registry);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("StyleLabShell.ui.html")),
        FPaths::Combine(Directory, TEXT("StyleLabShell.ui.css")));
    _AuthoredShellView = View;
    View->PollFiles();
    _AuthoredShellHost->SetContent(View->GetLastResult().Succeeded ? Main : Build_NativeShellFallback());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLabWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double           InCurrentTime,
        float            InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (_AuthoredShellView.IsValid())
    {
        const bool WasAccepted = _AuthoredShellView->GetLastResult().Succeeded;
        _AuthoredShellView->PollFiles(ck_style_lab_window::ShellStyleTokens());
        if (!WasAccepted && _AuthoredShellView->GetLastResult().Succeeded)
        { _AuthoredShellHost->SetContent(_AuthoredShellView->GetRegion(TEXT("main"))); }
    }

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (Settings == nullptr)
    { return; }

    // Structure is rebuilt only when the selection actually moved — an edit made in Editor
    // Preferences reaches the sample here, an in-Lab edit already rebuilt on the click.
    const auto Revision = Settings->Get_Revision();

    if (Revision == _LastSeenRevision)
    { return; }

    _LastSeenRevision = Revision;

    if (_ControlsPane.IsValid())
    { _ControlsPane->RequestPreviewRebuilds(); }
}

// ====================================================================================================================

auto
    SCkStyleLabWindow::
    Build_MenuActions()
    -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_IconToolbar)
        .Actions({
            FCkDebug_IconToggleAction{
                TEXT("StyleLabShowAllTones"),
                ECk_Icon::TextureAsset,
                FText::FromString(TEXT("All Tones")),
                FText::FromString(TEXT("Show one sample chip per semantic tone instead of the three the real surfaces use most.")),
                TAttribute<bool>::CreateSP(this, &SCkStyleLabWindow::Get_ShowAllTones),
                FOnCkDebug_IconToggleChanged::CreateSP(this, &SCkStyleLabWindow::Set_ShowAllTones)}
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLabWindow::
    Get_ShowAllTones() const
    -> bool
{
    return _ControlsPane.IsValid() && _ControlsPane->Get_ShowAllTones();
}

auto
    SCkStyleLabWindow::
    Set_ShowAllTones(
        bool InShowAllTones)
    -> void
{
    if (NOT _ControlsPane.IsValid())
    { return; }

    _ControlsPane->Set_ShowAllTones(InShowAllTones);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLabWindow::
    Get_StatusText() const
    -> FText
{
    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (Settings == nullptr)
    { return FText::GetEmpty(); }

    return FText::FromString(ck::Format_UE(
        TEXT("Profile: {}  |  Axis schema v{}  |  Changes apply live to every open debugger"),
        Settings->ActiveProfileName,
        Settings->SchemaVersion));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkStyleLabWindow::
    OnSelectionChanged()
    -> void
{
    if (const auto* Settings = UCkDebuggerStyleSettings::Get())
    { _LastSeenRevision = Settings->Get_Revision(); }
}

// ====================================================================================================================
