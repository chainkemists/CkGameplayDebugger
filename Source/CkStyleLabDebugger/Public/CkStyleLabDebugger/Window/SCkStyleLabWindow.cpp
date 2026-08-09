#include "CkStyleLabDebugger/Window/SCkStyleLabWindow.h"

#include "CkStyleLabDebugger/Widgets/SCkStyleLab_ControlsPane.h"
#include "CkStyleLabDebugger/Widgets/SCkStyleLab_SamplePane.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_style_lab_window
{
    constexpr auto ControlsPaneWidth = 340.0f;
}

// --------------------------------------------------------------------------------------------------------------------

const FName SCkStyleLabWindow::WindowId = FName(TEXT("StyleLabDebugger"));

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

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(Get_WindowId())
            .ToolTabId(TEXT("CkStyleLabDebugger"))
            .DisplayName(Get_WindowDisplayName())
            .StatusText(TAttribute<FText>::CreateSP(this, &SCkStyleLabWindow::Get_StatusText))
            .MenuActionsContent()
            [
                Build_MenuActions()
            ]
            .Content()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceM)
                    [
                        SNew(SScrollBox)

                        + SScrollBox::Slot()
                            [
                                SAssignNew(_SamplePane, SCkStyleLab_SamplePane)
                            ]
                    ]

                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, CkStyle::SpaceM, CkStyle::SpaceM, CkStyle::SpaceM)
                    [
                        SNew(SBox)
                            .WidthOverride(ck_style_lab_window::ControlsPaneWidth)
                            [
                                SNew(SScrollBox)

                                + SScrollBox::Slot()
                                    [
                                        SAssignNew(_ControlsPane, SCkStyleLab_ControlsPane)
                                            .OnSelectionChanged(FOnCkStyleLab_SelectionChanged::CreateSP(
                                                this, &SCkStyleLabWindow::OnSampleSelectionChanged))
                                    ]
                            ]
                    ]
            ]
    ];
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
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

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

    if (_SamplePane.IsValid())
    { _SamplePane->RequestRebuild(); }
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
                TEXT("Palette"),
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
    return _SamplePane.IsValid() && _SamplePane->Get_ShowAllTones();
}

auto
    SCkStyleLabWindow::
    Set_ShowAllTones(
        bool InShowAllTones)
    -> void
{
    if (NOT _SamplePane.IsValid())
    { return; }

    _SamplePane->Set_ShowAllTones(InShowAllTones);
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
    OnSampleSelectionChanged()
    -> void
{
    if (const auto* Settings = UCkDebuggerStyleSettings::Get())
    { _LastSeenRevision = Settings->Get_Revision(); }

    if (_SamplePane.IsValid())
    { _SamplePane->RequestRebuild(); }
}

// ====================================================================================================================
