#include "CkDebuggerPage_Activity.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleDefaults.h"

// =====================================================================================================================

namespace ck_debugger_page_activity
{
    constexpr auto MaxEvents = 200;

    auto Get_RowPadding() -> FMargin
    {
        return ck::debug_axes::Apply_RowDensity(FMargin{FCkDebuggerStyle::Padding_Small, 1.0f});
    }

    // RowBanding, split the way the axis defines it: Zebra owns the row's fill, Hairline owns a
    // rule under the row. Asking for the band brush under Hairline would paint the separator
    // across the whole row instead of along its edge.
    auto Get_RowBandFill(int32 InRowIndex) -> const FSlateBrush*
    {
        return UCkDebuggerStyleSettings::Get_Selection().RowBanding == ECkDebugAxis_RowBanding::Zebra
            ? ck::debug_axes::Get_RowBandingBrush(InRowIndex)
            : FStyleDefaults::GetNoBrush();
    }

    auto Make_RowRule() -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .HeightOverride_Lambda([]() -> FOptionalSize
            {
                return FOptionalSize{ck::debug_axes::Get_RowBandingRuleThickness()};
            })
            .Visibility_Lambda([]()
            {
                return ck::debug_axes::Get_RowBandingRuleThickness() > 0.0f
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetFilledBrush())
                .BorderBackgroundColor(FSlateColor{CkStyle::Border()})
            ];
    }
}

FCkDebuggerPage_Activity::~FCkDebuggerPage_Activity()
{
    if (WorldModel.IsValid())
    {
        if (CacheDiffHandle.IsValid())
        { WorldModel->OnCacheDiff.Remove(CacheDiffHandle); }
        if (WorldChangedHandle.IsValid())
        { WorldModel->OnWorldChanged.Remove(WorldChangedHandle); }
    }
}

auto FCkDebuggerPage_Activity::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Activity"));
}

auto FCkDebuggerPage_Activity::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Activity::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Activity::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;

    if (InIsActive)
    {
        FeedDirty = true;
        UnseenCount = 0;   // the badge counts only what happened while looking away
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Activity::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    SelectionModel = InContext.SelectionModel;
    WorldModel = InContext.WorldModel;

    if (WorldModel.IsValid())
    {
        CacheDiffHandle = WorldModel->OnCacheDiff.AddRaw(this, &FCkDebuggerPage_Activity::HandleCacheDiff);
        WorldChangedHandle = WorldModel->OnWorldChanged.AddRaw(this, &FCkDebuggerPage_Activity::HandleWorldChanged);
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            ck::debug_axes::Make_SectionHeader(
                UCkDebuggerStyleSettings::Get_Selection(),
                FText::FromString(TEXT("Spawn / destroy feed (newest first)")),
                ECk_Tone::Neutral)
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(FCkDebuggerStyle::Padding_Medium, 0.0f)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FActivityEvent>>)
            .ListItemsSource(&Events)
            // Lambda (not method binding): pages are plain classes, not TSharedFromThis;
            // the page owns the list, so a raw `this` capture cannot dangle.
            .OnGenerateRow_Lambda([this](TSharedPtr<FActivityEvent> InEvent, const TSharedRef<STableViewBase>& InOwnerTable)
            {
                return OnGenerateRow(InEvent, InOwnerTable);
            })
            .OnSelectionChanged_Lambda([this](TSharedPtr<FActivityEvent> InEvent, ESelectInfo::Type InSelectInfo)
            {
                if (InSelectInfo == ESelectInfo::Direct || NOT InEvent.IsValid())
                { return; }

                if (InEvent->IsSpawn && SelectionModel.IsValid() && ck::IsValid(InEvent->Entity))
                { SelectionModel->Set_SelectedEntities({ InEvent->Entity }); }
            })
            .SelectionMode(ESelectionMode::Single)
        ];
}

auto FCkDebuggerPage_Activity::Tick(float InDeltaTime) -> void
{
    if (NOT IsActivePage || NOT FeedDirty)
    { return; }

    FeedDirty = false;

    if (ListView.IsValid())
    { ListView->RequestListRefresh(); }
}

// =====================================================================================================================

auto FCkDebuggerPage_Activity::HandleCacheDiff(
    const TArray<FCk_Handle>& InAdded,
    const TArray<FCk_Handle>& InRemoved) -> void
{
    const auto Now = FDateTime::Now();

    // Removed handles are dying — read nothing off them beyond identity. Their display
    // name is best-effort (the tree's cache is gone by the time we hear about it).
    for (const auto& Removed : InRemoved)
    {
        // The canonical "ID|Version(Raw)" string every EntityRef pill shows, composed through the
        // EntityIdStyle axis rather than hand-formatted — a destroyed entity has no name left to
        // read, so the name-bearing compositions correctly degrade to the id alone.
        auto Event = MakeShared<FActivityEvent>();
        Event->IsSpawn = false;
        Event->DisplayName = ck::debug_axes::Make_EntityIdText(
            UCkDebuggerStyleSettings::Get_Selection(),
            FString{},
            ck::Format_UE(TEXT("{}"), Removed.Get_Entity())).ToString();
        Event->Timestamp = Now;
        Events.Insert(Event, 0);
    }

    for (const auto& Added : InAdded)
    {
        if (ck::Is_NOT_Valid(Added))
        { continue; }

        auto Event = MakeShared<FActivityEvent>();
        Event->IsSpawn = true;
        Event->Entity = Added;
        Event->DisplayName = ck::DebugNameClean::Get_CleanName(
            UCk_Utils_Handle_UE::Get_DebugName(Added).ToString());
        Event->Timestamp = Now;
        Events.Insert(Event, 0);
    }

    if (Events.Num() > ck_debugger_page_activity::MaxEvents)
    { Events.SetNum(ck_debugger_page_activity::MaxEvents); }

    if (NOT IsActivePage)
    { UnseenCount += InAdded.Num() + InRemoved.Num(); }

    FeedDirty = true;
}

auto FCkDebuggerPage_Activity::HandleWorldChanged(UWorld* InWorld) -> void
{
    // Old-world events hold dying handles — drop everything on a world switch.
    Events.Reset();
    FeedDirty = true;
}

auto FCkDebuggerPage_Activity::OnGenerateRow(
    TSharedPtr<FActivityEvent> InEvent,
    const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    const auto IsSpawn = InEvent.IsValid() && InEvent->IsSpawn;
    const auto Label = InEvent.IsValid()
        ? FString::Printf(TEXT("%s  %s  %s"),
            *InEvent->Timestamp.ToString(TEXT("%H:%M:%S")),
            IsSpawn ? TEXT("+") : TEXT("-"),
            *InEvent->DisplayName)
        : FString{};

    // RowBanding needs a row ORDINAL, and this feed's source array is the ordering (newest first),
    // so the item's index in it is the stable band key — it shifts with the feed exactly as the
    // stripe should.
    const auto RowIndex = Events.IndexOfByKey(InEvent);

    return SNew(STableRow<TSharedPtr<FActivityEvent>>, InOwnerTable)
        .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugger.TableView.Row"))
        .Padding(TAttribute<FMargin>::CreateStatic(&ck_debugger_page_activity::Get_RowPadding))
        .ShowSelection(true)
        [
            SNew(SVerticalBox)

            // Zebra paints an alternating full-bleed fill; Off paints nothing (a no-brush border
            // with zero padding contributes neither paint nor geometry, so Classic is unchanged).
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage_Lambda([RowIndex]() -> const FSlateBrush*
                {
                    return ck_debugger_page_activity::Get_RowBandFill(RowIndex);
                })
                .Padding(0.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
                        .ColorAndOpacity(IsSpawn ? FSlateColor{CkStyle::Ok()} : FSlateColor{CkStyle::Err()})
                        .DesiredSizeOverride_Lambda([]() -> TOptional<FVector2D>
                        {
                            const auto Size = ck::debug_axes::Apply_IconSize(6.0f);
                            return FVector2D{Size, Size};
                        })
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(Label))
                        .ColorAndOpacity(IsSpawn ? CkStyle::Text() : CkStyle::TextDim())
                    ]
                ]
            ]

            // Hairline banding draws a rule under the row instead of filling it; zero thickness
            // (Off / Zebra, or SeparatorWeight None) collapses the box and its slot together.
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ck_debugger_page_activity::Make_RowRule()
            ]
        ];
}
