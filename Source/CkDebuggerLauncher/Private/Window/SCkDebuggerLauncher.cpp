#include "SCkDebuggerLauncher.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "Styles/CkDebuggerLauncherStyle.h"
#include "Window/CkDebuggerLauncherFilter.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerLauncher"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debugger_launcher
{
    auto Get_SeparatorThickness()
        -> float
    {
        return ck::debug_axes::Get_SeparatorThickness(UCkDebuggerStyleSettings::Get_Selection());
    }

    // RowDensity on the rail's tool buttons. Slot padding is a Slate attribute, so the axis moves
    // buttons that were built on the last registry change.
    auto Get_ToolButtonPadding()
        -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 2.0f});
    }

    // The two rail text styles bake their font at style-set creation, so TextScale can only reach
    // them as a per-widget Font override. Sizes mirror CkDebuggerLauncherStyle exactly: the section
    // caps are a deliberate 7pt outlier (no CkStyle role goes below FontSizeMicro's 8), the tool
    // label is the FontSizeSmall role.
    auto Get_SectionFont()
        -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Bold", 7);
    }

    auto Get_ToolLabelFont()
        -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall());
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebuggerLauncher::Construct(const FArguments& InArgs) -> void
{
    _OnToolSelected = InArgs._OnToolSelected;
    _SelectedToolId = InArgs._SelectedToolId;

    _RegistryChangedHandle = FCkDebuggerToolRegistry::Get().Get_OnChanged().AddSP(
        this,
        &SCkDebuggerLauncher::RebuildTools);

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerLauncherStyle::Get_BackgroundBrush())
        .Padding(FMargin{4.0f})
        [
            SNew(SBox)
            .MinDesiredWidth(52.0f)
            [
                SNew(SVerticalBox)

                // The suite entry sits ABOVE everything and dresses differently from the tool
                // buttons on purpose: it opens the one-window host, not a 23rd tool. Hidden in
                // embedded mode -- inside the suite this button would open the window it is in.
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin{0.0f, 0.0f, 0.0f, 6.0f})
                [
                    SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(FSlateColor{CkStyle::Accent().CopyWithNewOpacity(0.30f)})
                    .Padding(FMargin{2.0f})
                    .Visibility(Get_IsEmbedded() ? EVisibility::Collapsed : EVisibility::Visible)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .ToolTipText(LOCTEXT("OpenSuiteTooltip",
                            "Open the CK Debugger Suite - every tool in one window, this rail on its left"))
                        .IsEnabled_Lambda([]
                        {
                            return FGlobalTabmanager::Get()->HasTabSpawner(
                                ck::debugger_tabs::SuiteTabId);
                        })
                        .OnClicked_Lambda([]
                        {
                            FGlobalTabmanager::Get()->TryInvokeTab(
                                FTabId{ck::debugger_tabs::SuiteTabId});
                            return FReply::Handled();
                        })
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SImage)
                                .Image(FCkIconStyle::Get_Brush(ECk_Icon::Catalog, ECk_Icon_BrushSize::Size_16x16))
                                .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin{6.0f, 2.0f, 0.0f, 2.0f})
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("OpenSuite", "Debugger Suite"))
                                .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                                .ColorAndOpacity(FSlateColor{CkStyle::Accent()})
                                .Visibility(this, &SCkDebuggerLauncher::Get_LabelVisibility)
                            ]
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin{0.0f, 0.0f, 0.0f, 4.0f})
                [
                    SAssignNew(_SearchBar, SCkDebug_SearchBar)
                    .HintText(LOCTEXT("SearchHint", "Find a debugger..."))
                    .Visibility(this, &SCkDebuggerLauncher::Get_SearchVisibility)
                    .OnSearchTextChanged(FCkDebug_OnSearchTextChanged::CreateSP(
                        this, &SCkDebuggerLauncher::Handle_SearchTextChanged))
                    .OnSearchTextCommitted(FCkDebug_OnSearchTextCommitted::CreateSP(
                        this, &SCkDebuggerLauncher::Handle_SearchTextCommitted))
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    .Orientation(Orient_Vertical)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(_ToolList, SVerticalBox)
                    ]
                ]
            ]
        ]
    ];

    // Seed the baseline so the first tick can only report a change the user actually made.
    if (const auto* Settings = UCkDebuggerStyleSettings::Get())
    { _LastSeenStyleRevision = Settings->Get_Revision(); }

    RebuildTools();
}

auto SCkDebuggerLauncher::Poll_StyleRevision() -> void
{
    const auto* Settings = UCkDebuggerStyleSettings::Get();

    if (Settings == nullptr)
    { return; }

    const auto Revision = Settings->Get_Revision();

    if (Revision == _LastSeenStyleRevision)
    { return; }

    _LastSeenStyleRevision = Revision;

    // The rail's construct-baked reads (row density on the button slots, the separator between
    // category groups) live inside RebuildTools — the same entry point the registry uses.
    RebuildTools();
}

SCkDebuggerLauncher::~SCkDebuggerLauncher()
{
    if (_RegistryChangedHandle.IsValid())
    {
        FCkDebuggerToolRegistry::Get().Get_OnChanged().Remove(_RegistryChangedHandle);
        _RegistryChangedHandle.Reset();
    }
}

auto SCkDebuggerLauncher::Tick(
    const FGeometry& InAllottedGeometry,
    double InCurrentTime,
    float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    Poll_StyleRevision();

    const auto Width = InAllottedGeometry.GetLocalSize().X;
    auto ShowLabels = _ShowLabels;

    if (NOT ShowLabels && Width >= 180.0f) { ShowLabels = true; }
    else if (ShowLabels && Width <= 150.0f) { ShowLabels = false; }

    if (ShowLabels != _ShowLabels)
    {
        _ShowLabels = ShowLabels;
        Invalidate(EInvalidateWidgetReason::Layout);

        // The search box collapses with the labels — a rail this narrow has nowhere to put it. A
        // filter the user can no longer see or clear would read as "tools went missing", so the
        // query dies with the field that owns it.
        if (NOT _ShowLabels)
        { Clear_Search(); }
    }
}

auto SCkDebuggerLauncher::OnKeyDown(
    const FGeometry& InAllottedGeometry,
    const FKeyEvent& InKeyEvent) -> FReply
{
    // Escape reaches here only when the text box left it unhandled (nothing selected, nothing
    // changed since focus). The changed case arrives through Handle_SearchTextCommitted instead.
    if (InKeyEvent.GetKey() == EKeys::Escape && NOT _SearchQuery.IsEmpty())
    {
        Clear_Search();
        return FReply::Handled();
    }

    return SCompoundWidget::OnKeyDown(InAllottedGeometry, InKeyEvent);
}

auto SCkDebuggerLauncher::RebuildTools() -> void
{
    // Registry-changed broadcasts also arrive during UnloadModulesAtShutdown —
    // every feature debugger unregisters its descriptor from ShutdownModule,
    // AFTER Slate itself has shut down. Constructing widgets then (SButton's
    // ToolTipText goes through FSlateApplicationBase::Get()) fires the
    // IsInitialized ensure on exit. Nothing to rebuild for — the app is dying.
    if (NOT FSlateApplication::IsInitialized())
    { return; }

    if (NOT _ToolList.IsValid())
    { return; }

    _ToolList->ClearChildren();
    _VisibleToolIds.Reset();

    const auto AllTools = FCkDebuggerToolRegistry::Get().Get_Tools();
    const auto Tools = ck::debugger_launcher::Filter_Tools(_SearchQuery, AllTools);

    if (Tools.IsEmpty())
    {
        _ToolList->AddSlot()
        .AutoHeight()
        .Padding(FMargin{4.0f})
        [
            SNew(STextBlock)
            .Text(AllTools.IsEmpty()
                ? LOCTEXT("NoTools", "No debugger tools registered")
                : LOCTEXT("NoMatchingTools", "No debugger matches that search"))
            .AutoWrapText(true)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    // The filter preserves registry order, so a category's surviving tools stay contiguous and an
    // emptied category never emits a header.
    auto CurrentCategory = ECkDebuggerToolCategory::Invalid;
    for (const auto& Tool : Tools)
    {
        _VisibleToolIds.Add(Tool.Get_TabId());

        if (Tool.Get_Category() != CurrentCategory)
        {
            const auto AddSeparator = CurrentCategory != ECkDebuggerToolCategory::Invalid;
            CurrentCategory = Tool.Get_Category();

            _ToolList->AddSlot()
            .AutoHeight()
            [
                Build_CategoryHeader(CurrentCategory, AddSeparator)
            ];
        }

        _ToolList->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debugger_launcher::Get_ToolButtonPadding))
        [
            Build_ToolButton(Tool)
        ];
    }
}

auto SCkDebuggerLauncher::Build_ToolButton(const FCkDebuggerToolDescriptor& InTool) -> TSharedRef<SWidget>
{
    const auto TabId = InTool.Get_TabId();
    auto IconBrush = FCkIconStyle::Get_Brush(InTool.Get_IconId(), ECk_Icon_BrushSize::Size_24x24);

    CK_ENSURE_IF_NOT(IconBrush != nullptr,
        TEXT("Debugger launcher tool [{}] references missing icon [{}]"),
        TabId,
        InTool.Get_IconId())
    { IconBrush = FAppStyle::GetBrush(TEXT("Icons.Warning")); }

    const auto Tooltip = FText::Format(
        LOCTEXT("ToolTooltipFormat", "{0}\n{1}"),
        InTool.Get_DisplayName(),
        InTool.Get_Tooltip());

    const auto WeakPanel = TWeakPtr<SCkDebuggerLauncher>{SharedThis(this)};

    return SNew(SButton)
        .ButtonStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("CkDebuggerLauncher.ToolButton")))
        .ContentPadding(FMargin{5.0f})
        .HAlign(HAlign_Fill)
        .ToolTipText(Tooltip)
        .IsEnabled_Lambda([TabId]()
        {
            return FGlobalTabmanager::Get()->HasTabSpawner(TabId);
        })
        .OnClicked_Lambda([TabId, WeakPanel]()
        {
            if (const auto Panel = WeakPanel.Pin();
                Panel.IsValid())
            { Panel->Activate_Tool(TabId); }

            return FReply::Handled();
        })
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(3.0f)
                .HeightOverride(24.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerLauncherStyle::Get().GetBrush(TEXT("CkDebuggerLauncher.ActiveMarker")))
                    .Visibility_Lambda([TabId, WeakPanel]()
                    {
                        // An embedded tool has no live global tab to find — the host's selection IS
                        // its "open" state, so the marker has to read both sources.
                        const auto Panel = WeakPanel.Pin();

                        if (Panel.IsValid() && Panel->Get_IsEmbedded() && Panel->Get_SelectedToolId() == TabId)
                        { return EVisibility::Visible; }

                        return FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{TabId}).IsValid()
                            ? EVisibility::Visible
                            : EVisibility::Hidden;
                    })
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .Padding(FMargin{7.0f, 0.0f})
            [
                SNew(SBox)
                .WidthOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck::debug_axes::Apply_IconSize(28.0f)};
                })
                .HeightOverride_Lambda([]() -> FOptionalSize
                {
                    return FOptionalSize{ck::debug_axes::Apply_IconSize(28.0f)};
                })
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(SImage)
                    .Image(IconBrush)
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FMargin{4.0f, 0.0f, 8.0f, 0.0f})
            [
                SNew(STextBlock)
                .Text(InTool.Get_DisplayName())
                .TextStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("CkDebuggerLauncher.ToolText")))
                .Font_Static(&ck_debugger_launcher::Get_ToolLabelFont)
                .Visibility_Lambda([WeakPanel]()
                {
                    const auto Panel = WeakPanel.Pin();
                    return Panel.IsValid() ? Panel->Get_LabelVisibility() : EVisibility::Collapsed;
                })
            ]
        ];
}

auto SCkDebuggerLauncher::Build_CategoryHeader(
    ECkDebuggerToolCategory InCategory,
    bool InAddSeparator) -> TSharedRef<SWidget>
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin{2.0f, InAddSeparator ? 6.0f : 2.0f, 2.0f, 3.0f})
        [
            // SSeparator::Thickness is a construction-time argument with no setter, and the rail
            // only rebuilds when the registry changes, so the axis is carried by an SBox override.
            SNew(SBox)
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck_debugger_launcher::Get_SeparatorThickness()};
            })
            .Visibility_Lambda([InAddSeparator]()
            {
                return InAddSeparator && ck_debugger_launcher::Get_SeparatorThickness() > 0.0f
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SSeparator)
                .SeparatorImage(FCkDebuggerLauncherStyle::Get_SeparatorBrush())
                .Thickness(1.0f)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(FMargin{2.0f, 0.0f, 2.0f, 2.0f})
        [
            SNew(STextBlock)
            .Text(Get_CategoryDisplayName(InCategory))
            .TextStyle(&FCkDebuggerLauncherStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("CkDebuggerLauncher.SectionText")))
            .Font_Static(&ck_debugger_launcher::Get_SectionFont)
        ];
}

auto SCkDebuggerLauncher::Get_LabelVisibility() const -> EVisibility
{
    return _ShowLabels ? EVisibility::Visible : EVisibility::Collapsed;
}

auto SCkDebuggerLauncher::Get_SearchVisibility() const -> EVisibility
{
    return _ShowLabels ? EVisibility::Visible : EVisibility::Collapsed;
}

auto SCkDebuggerLauncher::Get_IsEmbedded() const -> bool
{
    return _OnToolSelected.IsBound();
}

auto SCkDebuggerLauncher::Get_SelectedToolId() const -> FName
{
    return _SelectedToolId.IsSet() ? _SelectedToolId.Get() : NAME_None;
}

auto SCkDebuggerLauncher::Activate_Tool(FName InTabId) -> void
{
    if (InTabId.IsNone())
    { return; }

    if (Get_IsEmbedded())
    {
        // The host decides what "open" means for this tool — embed it, focus its global tab, or
        // fall back to invoking it. The rail must not reach past the host to the tab manager.
        _OnToolSelected.Execute(InTabId);
        return;
    }

    if (NOT FGlobalTabmanager::Get()->HasTabSpawner(InTabId))
    { return; }

    ck::debugger_tabs::Invoke_DebuggerTab(InTabId);
}

auto SCkDebuggerLauncher::Handle_SearchTextChanged(const FString& InText) -> void
{
    if (_SearchQuery == InText)
    { return; }

    _SearchQuery = InText;
    RebuildTools();
}

auto SCkDebuggerLauncher::Handle_SearchTextCommitted(
    const FString& InText,
    ETextCommit::Type InCommitType) -> void
{
    // Escape reverts the box to whatever it held when it took focus; the rail's contract is that
    // Escape clears, so reverting is not enough.
    if (InCommitType == ETextCommit::OnCleared)
    {
        Clear_Search();
        return;
    }

    if (InCommitType != ETextCommit::OnEnter)
    { return; }

    // The search bar fires the changed pass before this one, so the list below is already filtered
    // by InText.
    if (_VisibleToolIds.IsEmpty())
    { return; }

    Activate_Tool(_VisibleToolIds[0]);
}

auto SCkDebuggerLauncher::Clear_Search() -> void
{
    if (_SearchBar.IsValid())
    {
        // Fires Handle_SearchTextChanged with an empty string, which rebuilds the rail.
        _SearchBar->Clear_SearchText();
        return;
    }

    Handle_SearchTextChanged(FString{});
}

auto SCkDebuggerLauncher::Get_CategoryDisplayName(ECkDebuggerToolCategory InCategory) -> FText
{
    switch (InCategory)
    {
        case ECkDebuggerToolCategory::Core: return LOCTEXT("CategoryCore", "CORE");
        case ECkDebuggerToolCategory::Ai: return LOCTEXT("CategoryAi", "AI");
        case ECkDebuggerToolCategory::Systems: return LOCTEXT("CategorySystems", "SYSTEMS");
        case ECkDebuggerToolCategory::Interface: return LOCTEXT("CategoryInterface", "INTERFACE");
        case ECkDebuggerToolCategory::Tools: return LOCTEXT("CategoryTools", "TOOLS");
        case ECkDebuggerToolCategory::Invalid: return FText::GetEmpty();
    }

    return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
