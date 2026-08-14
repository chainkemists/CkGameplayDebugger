#include "CkDebuggerWindow_Main.h"

#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Styling/AppStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Base.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Dashboard.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Overview.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Archetypes.h"
#include "CkEcsDebugger/Pages/CkDebuggerPage_Activity.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Markers/CkDebug_EntityMarkers.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_EntityList.h"
#include "CkEcsDebugger/Panels/CkDebuggerPanel_Inspector.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

// On-Screen Overlay controls (toolbar popover mirroring the overlay CVars + settings).
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "HAL/IConsoleManager.h"

const FName SCkDebuggerWindow_Main::WindowId = FName(TEXT("EcsDebugger"));

// ====================================================================================================================
// Typography. Every font here rides an attribute (Font_Static / Font_Lambda) instead of a
// construct-time FSlateFontInfo: TextScale bakes into the FSlateFontInfo, so a value-form call
// freezes the size of a widget that was built before the flip.
//
// Sizes come from CkStyle roles wherever a role matches what the site shipped with. The two
// literals below are deliberate outliers — no role sits at 7 or 11 and rounding them onto the
// nearest role would move Classic — so they stay literal INSIDE ScaledFont, which keeps them
// following TextScale even though they do not follow a role.
//
// Named, not anonymous: this module builds with unity on, and a merged TU collides file-local
// helpers by name.
// ====================================================================================================================

namespace ck_debugger_window_main
{
    auto Get_LabelFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH4()); }

    auto Get_BodyFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); }

    // Sub-micro: the toolbar's tiny popover heading and the tab-strip unseen-count badge.
    auto Get_TinyLabelFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", 7); }

    // The gear glyph is sized to the button, not to a text role.
    auto Get_GlyphFont() -> FSlateFontInfo
    { return ck::debug_axes::ScaledFont("Bold", 11); }
}

auto SCkDebuggerWindow_Main::Construct(const FArguments& InArgs) -> void
{
    Register_WithGate();

    SelectionModel = MakeShared<FCkDebuggerModel_EntitySelection>();
    WorldModel = MakeShared<FCkDebuggerModel_WorldContext>();
    FilterModel = MakeShared<FCkDebuggerModel_InspectorFilter>();

    // Shared viewport picker (CkDebuggerCommon), adapted to this window's models.
    // No TargetFilter — the ECS debugger previews every entity.
    ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    {
        auto PickerParams = FCkDebug_ViewportPicker::FParams{};
        PickerParams.Get_TargetWorld =
            [WeakWorld = TWeakPtr<FCkDebuggerModel_WorldContext>(WorldModel)]() -> UWorld*
            {
                const auto Pinned = WeakWorld.Pin();
                return Pinned.IsValid() ? Pinned->Get_SelectedWorld() : nullptr;
            };
        PickerParams.OnEntityPicked =
            [WeakSelection = TWeakPtr<FCkDebuggerModel_EntitySelection>(SelectionModel)](const FCk_Handle& InPicked)
            {
                if (const auto Pinned = WeakSelection.Pin())
                {
                    const auto NewSelection = TArray<FCk_Handle>{InPicked};
                    Pinned->Set_SelectedEntities(NewSelection);
                }

                // The entity tree applies model changes as ESelectInfo::Direct and deliberately never
                // re-broadcasts those, so a world-picked entity would stay invisible to Crowd/GOAP.
                // Broadcast here — this IS the user-driven selection.
                ck::DebugSelectionSync::Broadcast(InPicked, TEXT("EcsDebugger"));

                // Bring the debugger tab back to front — the game viewport took focus while
                // picking, so a successful pick would otherwise land in a background tab.
                ck::DebugNav::Goto_Entity(InPicked);
            };
        ViewportPicker->Construct(MoveTemp(PickerParams));
    }

    WorldChangedHandle = WorldModel->OnWorldChanged.AddSP(
        this, &SCkDebuggerWindow_Main::HandleWorldChanged);
    SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkDebuggerWindow_Main::HandleSessionInvalidated);

    // Refresh the command-bar badge strip when the filter selection changes.
    if (FilterModel.IsValid())
    {
        auto WeakSelf = TWeakPtr<SCkDebuggerWindow_Main>(SharedThis(this));
        FilterChangedHandle = FilterModel->OnFilterChanged.AddLambda(
        [WeakSelf]()
        {
            if (const auto Pinned = WeakSelf.Pin())
            {
                Pinned->Refresh_FilterBadgeStrip();
            }
        });
    }

    const auto AvailableWorlds = WorldModel->Get_AvailableWorlds();
    if (AvailableWorlds.Num() > 0)
    {
        WorldModel->Set_SelectedWorld(AvailableWorlds[0]);
    }

    Pages.Add(MakeShared<FCkDebuggerPage_Dashboard>());   // "Overview" (mockup dashboard)
    Pages.Add(MakeShared<FCkDebuggerPage_Overview>());     // "Graph" (hierarchy graph)
    Pages.Add(MakeShared<FCkDebuggerPage_Archetypes>());
    Pages.Add(MakeShared<FCkDebuggerPage_Activity>());
    Pages[0]->Set_IsActive(true);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkEcsDebugger"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("OverlayView"), FText::FromString(TEXT("Overlay view controls")), Build_MenuActions()),
            FCkDebug_CommandGroup::Context(TEXT("Target"), FText::FromString(TEXT("Entity target controls")), Build_TargetControls()),
            FCkDebug_CommandGroup::Context(TEXT("OverlaySettings"), FText::FromString(TEXT("Overlay settings")), Build_OverlaySettingsControls()),
            FCkDebug_CommandGroup::Context(TEXT("EntityFilter"), FText::FromString(TEXT("Entity filter controls")), Build_EntityFilterControls())
        })
        .ShowRefreshControls(true)
        .Content()
        [
            SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                .PhysicalSplitterHandleSize(3.0f)
                .HitDetectionSplitterHandleSize(5.0f)
                .Style(FAppStyle::Get(), "Splitter")

                + SSplitter::Slot()
                .Value(0.2f)
                .MinSize(200.0f)
                [
                    SNew(SBox)
                    .MaxDesiredWidth(500.0f)
                    [
                        Build_LeftSidebar()
                    ]
                ]

                + SSplitter::Slot()
                .Value(0.5f)
                .MinSize(400.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Border"))
                    .BorderBackgroundColor(CkStyle::Border())
                    .Padding(FMargin(1.0f, 0.0f))
                    [
                        SAssignNew(ContentAreaContainer, SBox)
                        [
                            Build_ContentArea()
                        ]
                    ]
                ]

                + SSplitter::Slot()
                .Value(0.3f)
                .MinSize(250.0f)
                [
                    SNew(SBox)
                    .MaxDesiredWidth(600.0f)
                    [
                        Build_InspectorPanel()
                    ]
                ]
            ]
        ]
        ]
    ];
}

SCkDebuggerWindow_Main::~SCkDebuggerWindow_Main()
{
    if (WorldModel.IsValid() && WorldChangedHandle.IsValid())
    { WorldModel->OnWorldChanged.Remove(WorldChangedHandle); }

    if (SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(SessionInvalidatedHandle); }
}

auto SCkDebuggerWindow_Main::HandleWorldChanged(UWorld*) -> void
{
    // Order is load-bearing: stop picker reads first, then broadcast an empty
    // selection so inspectors/graphs deactivate, then release tree-owned copies.
    if (ViewportPicker.IsValid())
    { ViewportPicker->Deactivate(); }

    if (SelectionModel.IsValid())
    { SelectionModel->Reset_ForWorldChange(); }

    if (EntityListPanel.IsValid())
    { EntityListPanel->Reset_ForWorldChange(); }
}

auto SCkDebuggerWindow_Main::HandleSessionInvalidated() -> void
{
    if (WorldModel.IsValid())
    {
        if (WorldModel->Get_SelectedWorld() != nullptr)
        {
            WorldModel->Set_SelectedWorld(nullptr);
            return;
        }

        WorldModel->Reset_ForWorldChange();
        return;
    }

    HandleWorldChanged(nullptr);
}

auto SCkDebuggerWindow_Main::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    // MUST be the base, not SCompoundWidget: the base's Tick is what polls the Layer-B style
    // revision and fires OnStyleRevisionChanged. Calling the grandparent compiles and runs, and
    // silently kills live-apply for every structural axis in this window.
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    // Page ticks drive graph rebuilds — gate them behind the user's refresh
    // settings. Viewport-picker ticks stay ungated so input handling keeps
    // working even when the panel is paused.
    if (FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    {
        for (const auto& Page : Pages)
        {
            if (Page.IsValid() && Page->IsActive())
            {
                Page->Tick(InDeltaTime);
            }
        }
    }

    if (ViewportPicker.IsValid() && ViewportPicker->IsActive())
    {
        ViewportPicker->Tick(InDeltaTime);
    }
}

// ====================================================================================================================
// Style live-apply — the structural half.
//
// Everything visual in this window is attribute-bound and needs nothing here. What this drives is
// the four surfaces that COMPOSE at build time and cannot be expressed as an attribute:
//
    //   1. the command-bar active-filter badge strip (its widget per badge is chosen by BadgeStyle),
//   2. the inspector panel (row set, section headers, EntityIdStyle name composition),
//   3. the entity list's quick-access section (rows composed per pinned entity),
//   4. the active page, via the page interface's own hook.
//
// Deliberately NOT RebuildContentArea(): Build_ContentArea re-invokes the active page's
// Build_Content, which is not idempotent for every page (Overview roots a fresh UEdGraph and
// re-binds its model delegates each call). The page hook is the safe route.
//
// The entity tree is absent on purpose — it owns its own revision poll inside its refresh
// interval, where it can re-present without losing node pointer identity.
// ====================================================================================================================

auto SCkDebuggerWindow_Main::OnStyleRevisionChanged() -> void
{
    Refresh_FilterBadgeStrip();

    if (InspectorPanel.IsValid())
    { InspectorPanel->Request_RebuildInspectors(); }

    if (EntityListPanel.IsValid())
    { EntityListPanel->Notify_StyleRevisionChanged(); }

    for (const auto& Page : Pages)
    {
        if (Page.IsValid())
        { Page->OnStyleRevisionChanged(); }
    }
}

auto SCkDebuggerWindow_Main::Build_MenuActions() -> TSharedRef<SWidget>
{
    const auto Make_OverlayAction = [](
        FName InId,
        FName InIconId,
        const TCHAR* InLabel,
        const TCHAR* InCVarName,
        const TCHAR* InTooltip) -> FCkDebug_IconToggleAction
    {
        const auto CVarName = FString{InCVarName};
        return FCkDebug_IconToggleAction{
            InId,
            InIconId,
            FText::FromString(InLabel),
            FText::FromString(InTooltip),
            TAttribute<bool>::CreateLambda([CVarName]() -> bool
            {
                const auto* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
                return CVar != nullptr && CVar->GetInt() != 0;
            }),
            FOnCkDebug_IconToggleChanged::CreateLambda([CVarName](bool InIsOn)
            {
                if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName))
                { CVar->Set(InIsOn ? 1 : 0, ECVF_SetByConsole); }
            }),
            TAttribute<bool>::CreateLambda([CVarName]() -> bool
            {
                return IConsoleManager::Get().FindConsoleVariable(*CVarName) != nullptr;
            })};
    };

    const auto IconActions = TArray<FCkDebug_IconToggleAction>{
        Make_OverlayAction(
            TEXT("EcsOverlayEnabled"), TEXT("World"), TEXT("Overlay enabled"), TEXT("ck.DebugOverlay"),
            TEXT("Master toggle for the on-screen entity debug overlay (ck.DebugOverlay).\n"
                 "Available once a PIE session has started.")),
        Make_OverlayAction(
            TEXT("EcsNearPlates"), TEXT("Grid"), TEXT("Near plates"), TEXT("ck.DebugOverlay.NearPlates"),
            TEXT("Ultra-condensed name and feature-badge plates for candidates close to the camera\n"
                 "(ck.DebugOverlay.NearPlates).")),
        Make_OverlayAction(
            TEXT("EcsSyncHovered"), TEXT("Target"), TEXT("Sync hovered entity"),
            ck::DebugSelectionSync::Get_OverlayFocusSyncCVarName(),
            TEXT("Continuously sync the on-screen overlay focus into every already-open compatible debugger.\n"
                 "This never opens or foregrounds a debugger tab.")),
        Make_OverlayAction(
            TEXT("EcsFullDepth"), TEXT("Tree"), TEXT("Full depth for focus"),
            ck::DebugMarkers::Get_FocusFullDepthCVarName(),
            TEXT("Show the complete lifetime subtree for the current hovered, locked, or pinned entity.\n"
                 "Unrelated entities still obey Max Depth."))};
    // The picker's own options (Ignore Self, Meshes First, radii) live in the
    // shared SCkDebug_ViewportPickerControls gear popover in the toolbar —
    // contextual picker filters don't belong in the window chrome.

    return SNew(SCkDebug_IconToolbar)
        .Actions(IconActions);
}

auto SCkDebuggerWindow_Main::Build_TargetControls() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_ViewportPickerControls)
        .Picker(ViewportPicker)
        .ExtraSettingsContent()
        [
            Build_PickerExtraSettings()
        ];
}

auto SCkDebuggerWindow_Main::Build_OverlaySettingsControls() -> TSharedRef<SWidget>
{
    return SAssignNew(OverlayAnchor, SMenuAnchor)
                .Placement(MenuPlacement_BelowAnchor)
                .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
                {
                    return Build_OverlayPopover();
                })
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity_Lambda([]() -> FLinearColor
                    {
                        const auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.DebugOverlay"));
                        return (CVar != nullptr && CVar->GetInt() != 0)
                            ? CkStyle::Selection()
                            : CkStyle::Bg2();
                    })
                    .ToolTipText(FText::FromString(TEXT(
                        "On-screen entity overlay layout controls\n"
                        "(depth, plate anchor/width, diamond scale).")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (OverlayAnchor.IsValid())
                        {
                            OverlayAnchor->SetIsOpen(NOT OverlayAnchor->IsOpen());
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Overlay")))
                        .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                ];
}

auto SCkDebuggerWindow_Main::Build_EntityFilterControls() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(FilterAnchor, SMenuAnchor)
                .Placement(MenuPlacement_BelowAnchor)
                .OnGetMenuContent_Lambda([this]() -> TSharedRef<SWidget>
                {
                    return Build_FilterPopover();
                })
                [
                    SNew(SButton)
                    .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
                    {
                        return FilterModel.IsValid() && FilterModel->Get_HasActiveFilter()
                            ? CkStyle::Selection()
                            : CkStyle::Bg2();
                    })
                    .ForegroundColor_Lambda([this]() -> FSlateColor
                    {
                        return FilterModel.IsValid() && FilterModel->Get_HasActiveFilter()
                            ? FSlateColor(CkStyle::TextStrong())
                            : FSlateColor(CkStyle::TextDim());
                    })
                    .ToolTipText(FText::FromString(TEXT(
                        "Filter entities by which inspectors apply to them.\n"
                        "Non-matching entities are dimmed but remain selectable.")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (FilterAnchor.IsValid())
                        {
                            FilterAnchor->SetIsOpen(NOT FilterAnchor->IsOpen());
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() -> FText
                        {
                            if (NOT FilterModel.IsValid() || NOT FilterModel->Get_HasActiveFilter())
                            { return FText::FromString(TEXT("Filter")); }

                            const auto Count = FilterModel->Get_NumSelected();
                            const auto CountStr = Count > 9
                                ? FString(TEXT("9+"))
                                : ck::Format_UE(TEXT("{}"), Count);
                            return FText::FromString(ck::Format_UE(TEXT("Filter ({})"), CountStr));
                        })
                        .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(FilterBadgeStrip, SHorizontalBox)
            ];
}

auto SCkDebuggerWindow_Main::Build_PickerExtraSettings() -> TSharedRef<SWidget>
{
    // ECS-specific rows appended below the shared picker rows in the picker's
    // gear popover (SCkDebug_ViewportPickerControls).
    return SNew(SVerticalBox)

        // ---- Overlay attribute filter (focus-card volume control) ----
        // Persisted on UCk_DebugOverlay_Settings; the overlay's attribute providers
        // consult it per row, so edits apply live during PIE.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 2.0f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("OVERLAY ATTRIBUTES")))
            .Font_Static(&ck_debugger_window_main::Get_TinyLabelFont)
            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
        [
            SNew(SBox)
            .WidthOverride(240.0f)
            [
                SNew(SEditableTextBox)
                .HintText(FText::FromString(TEXT("patterns, comma-separated")))
                .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                .ToolTipText(FText::FromString(TEXT(
                    "Case-insensitive SUBSTRING patterns matched against attribute names\n"
                    "on the overlay focus card ('Health' catches 'Attr.Health.Max').\n"
                    "Empty = show all. Mode below flips between show-only and hide.")))
                .Text_Lambda([]() -> FText
                {
                    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
                    return FText::FromString(Settings != nullptr
                        ? FString::Join(Settings->AttributeFilterPatterns, TEXT(", "))
                        : FString{});
                })
                .OnTextCommitted_Lambda([](const FText& InText, ETextCommit::Type)
                {
                    auto* Settings = GetMutableDefault<UCk_DebugOverlay_Settings>();
                    if (Settings == nullptr)
                    { return; }

                    auto Patterns = TArray<FString>{};
                    InText.ToString().ParseIntoArray(Patterns, TEXT(","));
                    for (auto& Pattern : Patterns)
                    { Pattern.TrimStartAndEndInline(); }
                    Patterns.RemoveAll([](const FString& InPattern) { return InPattern.IsEmpty(); });

                    Settings->AttributeFilterPatterns = MoveTemp(Patterns);
                    Settings->SaveConfig();
                })
            ]
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SCkDebug_IconToggle)
            .IconId(TEXT("Attribute"))
            .Label(FText::FromString(TEXT("Exclude listed")))
            .ToolTip(FText::FromString(TEXT(
                "Unchecked: show ONLY attributes matching a pattern.\n"
                "Checked: show all EXCEPT matching (deny-list mode).")))
            .IsOn_Lambda([]() -> bool
            {
                const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
                return Settings != nullptr && Settings->bAttributeFilterIsExclusion;
            })
            .OnStateChanged_Lambda([](bool InIsOn)
            {
                auto* Settings = GetMutableDefault<UCk_DebugOverlay_Settings>();
                if (Settings == nullptr)
                { return; }

                Settings->bAttributeFilterIsExclusion = InIsOn;
                Settings->SaveConfig();
            })
            .ShowLabel(true)
        ];
}

// ====================================================================================================================
// On-Screen Overlay popover — mirrors the overlay's CVars + tunable UDeveloperSettings.
// CVar edits go through IConsoleManager (no hard coupling to the subsystem); settings
// edits mutate the UCk_DebugOverlay_Settings CDO, which the overlay reads live every
// tick. CDO edits are session-only — persist via Project Settings.
// Gameplay-tag-driven layout data intentionally stays in Project Settings.
// ====================================================================================================================

// Reopens the file-local namespace declared at the top of this file.
namespace ck_debugger_window_main
{
    // The popover's uniform row gap. Bound, not computed: SBoxPanel slot padding is a
    // TAttribute, so RowDensity lands on the already-built popover.
    auto Get_PopoverRowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(
            FMargin{0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small});
    }

    auto Get_OverlayCVar(const TCHAR* InName) -> IConsoleVariable*
    {
        return IConsoleManager::Get().FindConsoleVariable(InName);
    }

    auto Make_OverlayPopoverLabel(const TCHAR* InLabel) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(110.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(InLabel))
                .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
            ];
    }

    auto Make_OverlaySettingSpin(
        const TCHAR* InLabel,
        float InMin, float InMax, float InDelta,
        TFunction<float()> InGet,
        TFunction<void(float)> InSet,
        const TCHAR* InTooltip) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                Make_OverlayPopoverLabel(InLabel)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(120.0f)
                [
                    SNew(SSpinBox<float>)
                    .MinValue(InMin)
                    .MaxValue(InMax)
                    .Delta(InDelta)
                    .Value_Lambda(MoveTemp(InGet))
                    .OnValueChanged_Lambda(MoveTemp(InSet))
                    .ToolTipText(FText::FromString(InTooltip))
                ]
            ];
    }
}

auto SCkDebuggerWindow_Main::Build_OverlayPopover() -> TSharedRef<SWidget>
{
    const auto RowPad = TAttribute<FMargin>::CreateStatic(
        &ck_debugger_window_main::Get_PopoverRowPadding);

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SVerticalBox)

            // ---- Max depth (CVar; -1 = unlimited) ----
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    ck_debugger_window_main::Make_OverlayPopoverLabel(TEXT("Max Depth:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<int32>)
                        .MinValue(-1)
                        .MaxValue(16)
                        .Delta(1)
                        .Value_Lambda([]() -> int32
                        {
                            const auto* CVar = ck_debugger_window_main::Get_OverlayCVar(ck::DebugMarkers::Get_MaxDepthCVarName());
                            return CVar != nullptr ? CVar->GetInt() : 0;
                        })
                        .OnValueChanged_Lambda([](int32 InValue)
                        {
                            if (auto* CVar = ck_debugger_window_main::Get_OverlayCVar(ck::DebugMarkers::Get_MaxDepthCVarName()))
                            {
                                CVar->Set(InValue, ECVF_SetByConsole);
                            }
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Max hierarchy depth for the entity-marker preview — shared by the\n"
                            "on-screen overlay AND the ECS picker (ck.Debug.EntityMarkers.MaxDepth).\n"
                            "-1 = unlimited, 0 = top-level entities only, N = up to N levels deep.")))
                    ]
                ]
            ]

            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, FCkDebuggerStyle::Padding_Small))
            [
                ck::debug_axes::Make_AxisSeparator()
            ]

            // ---- Settings-backed controls (live; session-only — persist via Project Settings) ----
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    ck_debugger_window_main::Make_OverlayPopoverLabel(TEXT("Plate Anchor:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        // Cycle button (SEnumComboBox lives in editor-only EditorWidgets,
                        // which an UncookedOnly module can't depend on unconditionally).
                        SNew(SButton)
                        .ButtonColorAndOpacity(CkStyle::Bg2())
                        .HAlign(HAlign_Center)
                        .OnClicked_Lambda([]() -> FReply
                        {
                            auto* MutableSettings = GetMutableDefault<UCk_DebugOverlay_Settings>();
                            const auto Next =
                                (static_cast<int32>(MutableSettings->PlateAnchor) + 1) %
                                (static_cast<int32>(ECk_DebugOverlay_PlateAnchor::BottomRight) + 1);
                            MutableSettings->PlateAnchor = static_cast<ECk_DebugOverlay_PlateAnchor>(Next);
                            return FReply::Handled();
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "Viewport corner/edge the focus plate is anchored to — click to cycle.\n"
                            "Session-only here — persist via Project Settings → Ck On-Screen Debugger.")))
                        [
                            SNew(STextBlock)
                            .Text_Lambda([]() -> FText
                            {
                                const auto Anchor = GetDefault<UCk_DebugOverlay_Settings>()->PlateAnchor;
                                return FText::FromString(
                                    StaticEnum<ECk_DebugOverlay_PlateAnchor>()->GetNameStringByValue(
                                        static_cast<int64>(Anchor)));
                            })
                            .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                            .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Plate Width:"), 240.0f, 1600.0f, 10.0f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->PlateWidth; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->PlateWidth = InValue; },
                    TEXT("Width (Slate units) of the focus plate. Applied live."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Diamond Scale:"), 0.2f, 5.0f, 0.1f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->DiamondScale; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->DiamondScale = InValue; },
                    TEXT("Uniform scale of the in-world diamond markers. Applied live."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Plate Font:"), 0.5f, 2.0f, 0.05f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->PlateFontScale; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->PlateFontScale = InValue; },
                    TEXT("Font-size multiplier for the main overlay plate's text. Applied live."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    ck_debugger_window_main::Make_OverlayPopoverLabel(TEXT("SM Name Depth:"))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(120.0f)
                    [
                        SNew(SSpinBox<int32>)
                        .MinValue(0)
                        .MaxValue(8)
                        .Delta(1)
                        .Value_Lambda([]() -> int32
                        {
                            return GetDefault<UCk_DebugOverlay_Settings>()->SmStateNameDepth;
                        })
                        .OnValueChanged_Lambda([](int32 InValue)
                        {
                            GetMutableDefault<UCk_DebugOverlay_Settings>()->SmStateNameDepth = InValue;
                        })
                        .ToolTipText(FText::FromString(TEXT(
                            "State-machine state-name depth (same rule as the SM debugger):\n"
                            "show the last N underscore-segments of the state class name. 0 = full name.")))
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Near Dist:"), 100.0f, 5000.0f, 50.0f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->NearDist; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->NearDist = InValue; },
                    TEXT("Distance (cm) below which pills are full-size and near plates appear."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Far Dist:"), 500.0f, 20000.0f, 100.0f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->FarDist; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->FarDist = InValue; },
                    TEXT("Distance (cm) at which pills reach their minimum scale."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Min Scale:"), 0.1f, 1.0f, 0.05f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->MinScale; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->MinScale = InValue; },
                    TEXT("Minimum pill scale applied at Far Dist and beyond."))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(RowPad)
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Fade Start:"), 500.0f, 20000.0f, 100.0f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->FadeStartDist; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->FadeStartDist = InValue; },
                    TEXT("Distance (cm) at which pill opacity starts fading."))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                ck_debugger_window_main::Make_OverlaySettingSpin(TEXT("Max Dist:"), 1000.0f, 50000.0f, 100.0f,
                    []() { return GetDefault<UCk_DebugOverlay_Settings>()->MaxDist; },
                    [](float InValue) { GetMutableDefault<UCk_DebugOverlay_Settings>()->MaxDist = InValue; },
                    TEXT("Hard cull distance (cm): pills/plates beyond this are not shown."))
            ]
        ];
}

auto SCkDebuggerWindow_Main::Build_FilterPopover() -> TSharedRef<SWidget>
{
    if (NOT FilterModel.IsValid())
    { return SNullWidget::NullWidget; }

    auto InspectorList = SNew(SVerticalBox);

    for (const auto& Entry : FilterModel->Get_AllEntries())
    {
        const auto EntryID    = Entry.ID;
        const auto BadgeColor = Entry.BadgeColor;
        const auto DisplayName = Entry.DisplayName;

        InspectorList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 1.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SBox)
                .WidthOverride(14.0f)
                .HeightOverride(14.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                    .BorderBackgroundColor(BadgeColor)
                    .Padding(0.0f)
                    [
                        SNullWidget::NullWidget
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_ToggleSurface)
                .IsOn_Lambda([this, EntryID]() -> bool
                {
                    return FilterModel.IsValid() && FilterModel->Get_IsSelected(EntryID);
                })
                .OnStateChanged_Lambda([this, EntryID](bool InIsOn)
                {
                    if (FilterModel.IsValid())
                    {
                        FilterModel->Set_Selection(EntryID, InIsOn);
                    }
                })
                .AccessibleText(DisplayName)
                [
                    SNew(STextBlock)
                    .Text(DisplayName)
                    .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                    .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
            [
                SNew(SCkDebug_ToggleSurface)
                .ToolTipText(FText::FromString(TEXT(
                    "Hide — when checked, entities matching this inspector are removed from the\n"
                    "entity tree entirely (not just dimmed). Persists across editor sessions via\n"
                    "Project Settings → CkGameplayDebugger → Ck ECS Debugger.")))
                .AccessibleText(FText::FromString(TEXT("Hide")))
                .IsOn_Lambda([this, EntryID]() -> bool
                {
                    return FilterModel.IsValid() && FilterModel->Get_IsExcluded(EntryID);
                })
                .OnStateChanged_Lambda([this, EntryID](bool InIsOn)
                {
                    if (FilterModel.IsValid())
                    {
                        FilterModel->Set_Exclusion(EntryID, InIsOn);
                    }
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Hide")))
                    .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                ]
            ]
        ];
    }

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Medium"))
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SBox)
            .WidthOverride(280.0f)
            .MaxDesiredHeight(500.0f)
            [
                SNew(SVerticalBox)

                // ---- Header row: Match mode + Clear all ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Match:")))
                        .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SSegmentedControl<ECk_InspectorFilter_MatchMode>)
                        .Value_Lambda([this]() -> ECk_InspectorFilter_MatchMode
                        {
                            return FilterModel.IsValid()
                                ? FilterModel->Get_MatchMode()
                                : ECk_InspectorFilter_MatchMode::All;
                        })
                        .OnValueChanged_Lambda([this](ECk_InspectorFilter_MatchMode InMatchMode)
                        {
                            if (FilterModel.IsValid())
                            { FilterModel->Set_MatchMode(InMatchMode); }
                        })
                        + SSegmentedControl<ECk_InspectorFilter_MatchMode>::Slot(ECk_InspectorFilter_MatchMode::All)
                            .Text(FText::FromString(TEXT("All")))
                        + SSegmentedControl<ECk_InspectorFilter_MatchMode>::Slot(ECk_InspectorFilter_MatchMode::Any)
                            .Text(FText::FromString(TEXT("Any")))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Right)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                        [
                            SNew(SButton)
                            .ButtonColorAndOpacity(CkStyle::Bg2())
                            .ToolTipText(FText::FromString(TEXT(
                                "Un-hide every inspector — clears the persistent exclusion list.")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (FilterModel.IsValid())
                                {
                                    FilterModel->Clear_Exclusions();
                                }
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Clear hidden")))
                                .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                            .ButtonColorAndOpacity(CkStyle::Bg2())
                            .ToolTipText(FText::FromString(TEXT("Remove every active filter.")))
                            .OnClicked_Lambda([this]() -> FReply
                            {
                                if (FilterModel.IsValid())
                                {
                                    FilterModel->Clear_Selection();
                                }
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Clear all")))
                                .Font_Static(&ck_debugger_window_main::Get_BodyFont)
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                        ]
                    ]
                ]

                // ---- Header row 2: Badge style ----
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Badges:")))
                        .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SSegmentedControl<ECk_InspectorFilter_BadgeStyle>)
                        .Value_Lambda([this]() -> ECk_InspectorFilter_BadgeStyle
                        {
                            return FilterModel.IsValid()
                                ? FilterModel->Get_BadgeStyle()
                                : ECk_InspectorFilter_BadgeStyle::ColorAndText;
                        })
                        .OnValueChanged_Lambda([this](ECk_InspectorFilter_BadgeStyle InNewValue)
                        {
                            if (FilterModel.IsValid())
                            {
                                FilterModel->Set_BadgeStyle(InNewValue);
                            }
                        })
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::ColorOnly)
                            .Text(FText::FromString(TEXT("Color")))
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::TextOnly)
                            .Text(FText::FromString(TEXT("Text")))
                        + SSegmentedControl<ECk_InspectorFilter_BadgeStyle>::Slot(ECk_InspectorFilter_BadgeStyle::ColorAndText)
                            .Text(FText::FromString(TEXT("Both")))
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small))
                [
                    ck::debug_axes::Make_AxisSeparator()
                ]

                // ---- Scrollable inspector list ----
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Vertical)
                    + SScrollBox::Slot()
                    [
                        InspectorList
                    ]
                ]
            ]
        ];
}

auto SCkDebuggerWindow_Main::Refresh_FilterBadgeStrip() -> void
{
    if (NOT FilterBadgeStrip.IsValid())
    { return; }

    FilterBadgeStrip->ClearChildren();

    if (NOT FilterModel.IsValid())
    { return; }

    const auto& AllEntries = FilterModel->Get_AllEntries();
    const auto BadgeStyle  = FilterModel->Get_BadgeStyle();

    for (const auto& Entry : AllEntries)
    {
        if (NOT FilterModel->Get_IsSelected(Entry.ID))
        { continue; }

        const auto EntryID    = Entry.ID;
        const auto BadgeColor = Entry.BadgeColor;
        const auto Label      = Entry.DisplayName.ToString();

        TSharedPtr<SWidget> BadgeContent;
        switch (BadgeStyle)
        {
            case ECk_InspectorFilter_BadgeStyle::ColorOnly:
            {
                BadgeContent = SNew(SBox)
                    .WidthOverride(14.0f)
                    .HeightOverride(14.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                        .BorderBackgroundColor(BadgeColor)
                        .Padding(0.0f)
                        [
                            SNullWidget::NullWidget
                        ]
                    ];
                break;
            }
            case ECk_InspectorFilter_BadgeStyle::TextOnly:
            {
                BadgeContent = SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                    .ColorAndOpacity(FSlateColor(BadgeColor));
                break;
            }
            case ECk_InspectorFilter_BadgeStyle::ColorAndText:
            default:
            {
                BadgeContent = SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                    .BorderBackgroundColor(BadgeColor)
                    .Padding(FMargin(6.0f, 1.0f))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .Font_Static(&ck_debugger_window_main::Get_LabelFont)
                        .ColorAndOpacity(FSlateColor(CkStyle::TextStrong()))
                    ];
                break;
            }
        }

        FilterBadgeStrip->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f))
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(0.0f)
                .ToolTipText(FText::Format(
                    FText::FromString(TEXT("{0}  (click to remove)")),
                    Entry.DisplayName))
                .OnClicked_Lambda([this, EntryID]() -> FReply
                {
                    if (FilterModel.IsValid())
                    {
                        FilterModel->Toggle_Selection(EntryID);
                    }
                    return FReply::Handled();
                })
                [
                    BadgeContent.ToSharedRef()
                ]
            ];
    }
}

auto SCkDebuggerWindow_Main::Build_LeftSidebar() -> TSharedRef<SWidget>
{
    return SAssignNew(EntityListPanel, SCkDebuggerPanel_EntityList, SelectionModel, WorldModel, FilterModel);
}

auto SCkDebuggerWindow_Main::Build_ContentArea() -> TSharedRef<SWidget>
{
    // Tab row
    auto TabRow = SNew(SHorizontalBox);

    for (auto i = 0; i < Pages.Num(); ++i)
    {
        if (NOT Pages[i].IsValid())
        { continue; }

        auto PageIndex = i;
        auto IsActive = (i == ActivePageIndex);

        // Weak page ref: the unseen badge polls live so events arriving while another
        // tab is active surface without a tab-strip rebuild.
        const auto WeakPage = TWeakPtr<ICkDebuggerPage_Base>(Pages[i]);

        TabRow->AddSlot()
            .AutoWidth()
            .Padding(1.0f, 0.0f)
            [
                SNew(SButton)
                    .ButtonColorAndOpacity(IsActive
                        ? CkStyle::Selection()
                        : CkStyle::Bg2())
                    .ForegroundColor(IsActive
                        ? CkStyle::TextStrong()
                        : CkStyle::TextDim())
                    .OnClicked_Lambda([this, PageIndex]()
                    {
                        OnPageSelected(PageIndex);
                        return FReply::Handled();
                    })
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(Pages[i]->Get_PageName())
                                .Font_Lambda([IsActive]() -> FSlateFontInfo
                                {
                                    return ck::debug_axes::ScaledFont(
                                        IsActive ? "Bold" : "Regular", CkStyle::FontSizeH4());
                                })
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(3.0f, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                            .BorderBackgroundColor(FSlateColor{CkStyle::Err()})
                            .Padding(FMargin{3.0f, 0.0f})
                            .Visibility_Lambda([WeakPage]()
                            {
                                const auto Pinned = WeakPage.Pin();
                                return Pinned.IsValid() && Pinned->Get_BadgeCount() > 0
                                    ? EVisibility::Visible
                                    : EVisibility::Collapsed;
                            })
                            [
                                SNew(STextBlock)
                                .Font_Static(&ck_debugger_window_main::Get_TinyLabelFont)
                                .ColorAndOpacity(FLinearColor::White)
                                .Text_Lambda([WeakPage]()
                                {
                                    const auto Pinned = WeakPage.Pin();
                                    return FText::AsNumber(Pinned.IsValid() ? Pinned->Get_BadgeCount() : 0);
                                })
                            ]
                        ]
                    ]
            ];
    }

    // Page content
    TSharedRef<SWidget> PageContent = SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
            .Text(FText::FromString(TEXT("No Page Selected")))
            .ColorAndOpacity(CkStyle::TextMute())
        ];

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        auto Context = FCkDebuggerPageContext
        {
            SelectionModel,
            WorldModel,
            FilterModel,
            // Archetype-card click-through → the entity list's Filter bar.
            [WeakSelf = TWeakPtr<SCkDebuggerWindow_Main>(SharedThis(this))](const FString& InFilter)
            {
                const auto Pinned = WeakSelf.Pin();
                if (Pinned.IsValid() && Pinned->EntityListPanel.IsValid())
                { Pinned->EntityListPanel->Set_FilterText(InFilter); }
            },
            // Filter-bar readback — cards derive their toggled state from the live text.
            [WeakSelf = TWeakPtr<SCkDebuggerWindow_Main>(SharedThis(this))]() -> FString
            {
                const auto Pinned = WeakSelf.Pin();
                return Pinned.IsValid() && Pinned->EntityListPanel.IsValid()
                    ? Pinned->EntityListPanel->Get_FilterText()
                    : FString{};
            }
        };

        PageContent = Pages[ActivePageIndex]->Build_Content(Context);
    }

    return SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
        .Padding(0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    TabRow
                ]
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(FCkDebuggerStyle::Padding_Small)
                [
                    PageContent
                ]
        ];
}

auto SCkDebuggerWindow_Main::Build_InspectorPanel() -> TSharedRef<SWidget>
{
    return SAssignNew(InspectorPanel, SCkDebuggerPanel_Inspector, SelectionModel);
}

auto SCkDebuggerWindow_Main::OnPageSelected(int32 InPageIndex) -> void
{
    if (NOT Pages.IsValidIndex(InPageIndex))
    { return; }

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        Pages[ActivePageIndex]->Set_IsActive(false);
    }

    ActivePageIndex = InPageIndex;

    if (Pages.IsValidIndex(ActivePageIndex) && Pages[ActivePageIndex].IsValid())
    {
        Pages[ActivePageIndex]->Set_IsActive(true);
    }

    RebuildContentArea();
}

auto SCkDebuggerWindow_Main::RebuildContentArea() -> void
{
    if (ContentAreaContainer.IsValid())
    {
        ContentAreaContainer->SetContent(Build_ContentArea());
    }
}

auto SCkDebuggerWindow_Main::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (SelectionModel.IsValid())
    {
        if (MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
        {
            if (SelectionModel->NavigateBack())
            {
                return FReply::Handled();
            }
        }

        if (MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
        {
            if (SelectionModel->NavigateForward())
            {
                return FReply::Handled();
            }
        }
    }

    return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

auto SCkDebuggerWindow_Main::Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection>
{
    return SelectionModel;
}

auto SCkDebuggerWindow_Main::Get_WorldModel() const -> TSharedPtr<FCkDebuggerModel_WorldContext>
{
    return WorldModel;
}

auto SCkDebuggerWindow_Main::Get_ViewportPicker() const -> TSharedPtr<FCkDebug_ViewportPicker>
{
    return ViewportPicker;
}

auto SCkDebuggerWindow_Main::Get_FilterModel() const -> TSharedPtr<FCkDebuggerModel_InspectorFilter>
{
    return FilterModel;
}
