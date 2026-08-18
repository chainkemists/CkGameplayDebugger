#include "CkDebuggerPage_Dashboard.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyleSelection.h"
#include "CkEcsDebugger/Widgets/SCkEcsDebugger_Sparkline.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// =====================================================================================================================

namespace ck_debugger_page_dashboard
{
    constexpr auto SampleWindow = 60;
    constexpr auto MaxCards = 12;
    constexpr auto MaxSingletons = 8;
    constexpr auto CardColumns = 3;
    constexpr auto FamilyBarMaxWidth = 240.0f;

    static auto PushSample(TArray<float>& InOutSamples, float InValue) -> void
    {
        InOutSamples.Add(InValue);
        if (InOutSamples.Num() > SampleWindow)
        { InOutSamples.RemoveAt(0); }
    }

    static auto Get_WorldLabel(const UWorld* InWorld) -> FString
    {
        if (InWorld == nullptr)
        { return TEXT("no world"); }

        switch (InWorld->GetNetMode())
        {
            case NM_ListenServer:    return TEXT("ListenServer");
            case NM_DedicatedServer: return TEXT("DedicatedServer");
            case NM_Client:          return TEXT("Client");
            default:                 return TEXT("Standalone");
        }
    }

    static auto MakeSectionHeader(const TCHAR* InLabel, const FText& InToolTip = FText::GetEmpty())
        -> TSharedRef<SWidget>
    {
        return ck::debug_axes::Make_SectionHeader(
            UCkDebuggerStyleSettings::Get_Selection(), FText::FromString(InLabel), ECk_Tone::Neutral, InToolTip);
    }

    // Icon glyphs on this page are smaller than the axis' own 12/16/20 scale, so IconSize moves
    // them by its offset. SCkDebug_Icon::Size is a plain argument, not an attribute — the value is
    // read when the card/row is built, which is every DoRefresh that changes the presented set.
    static auto IconSquare(float InBase) -> TOptional<FVector2D>
    {
        const auto Size = ck::debug_axes::Apply_IconSize(InBase);
        return FVector2D{Size, Size};
    }

    // True while SCkDebug_Icon paints no backdrop of its own, which is when a site that wants a
    // well has to draw one itself. Reads the live selection, so it is safe to call from an
    // attribute lambda every paint.
    static auto Is_IconWellLocal() -> bool
    {
        return UCkDebuggerStyleSettings::Get_Selection().IconTreatment
            == ECkDebugAxis_IconTreatment::Plain;
    }
}

// =====================================================================================================================

auto FCkDebuggerPage_Dashboard::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Overview"));
}

auto FCkDebuggerPage_Dashboard::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Dashboard::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Dashboard::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;

    if (InIsActive)
    { TimeSinceRefresh = MAX_flt; }
}

// =====================================================================================================================

auto FCkDebuggerPage_Dashboard::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    // The main window builds a fresh content tree whenever this page is reselected. These caches contain widgets
    // slotted into the previous tree, so retaining them would make the unchanged-key fast path leave the new
    // containers empty.
    FamilyCache.Reset();
    PresentedFamilies.Reset();
    CardCache.Reset();
    PresentedCardKeys.Reset();
    PresentedSingletonKeys.Reset();
    PresentedSingletonOverflow = 0;

    WorldModel = InContext.WorldModel;
    RequestEntityFilter = InContext.RequestEntityFilter;
    GetEntityFilter = InContext.GetEntityFilter;

    TotalSamples = MakeShared<TArray<float>>();

    const auto Content =
        SNew(SScrollBox)

        // ---- Hero: live count + total sparkline --------------------------------------
        + SScrollBox::Slot()
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(HeroCountText, STextBlock)
                        // Deliberate outlier: the hero readout is a display number, twice the
                        // largest CkStyle role (FontSizeH2 = 12), so it keeps its own size and
                        // only borrows the TextScale axis from ScaledFont.
                        .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 24); })
                        .Text(FText::FromString(TEXT("0")))
                        .ColorAndOpacity(CkStyle::TextStrong())
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(HeroSubText, STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("no world observed")))
                        .ColorAndOpacity(CkStyle::TextDim())
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkEcsDebugger_Sparkline)
                    .Samples(TotalSamples)
                    .Color(CkStyle::Selection())
                    .DesiredSize(FVector2D(220.0f, 44.0f))
                ]
            ]
        ]

        // ---- Population by family ----------------------------------------------------
        + SScrollBox::Slot()
        .Padding(FCkDebuggerStyle::Padding_Medium, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small)
                [
                    ck_debugger_page_dashboard::MakeSectionHeader(TEXT("POPULATION BY FAMILY"))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(FamilyBox, SVerticalBox)
                ]
            ]
        ]

        // ---- Archetype cards ----------------------------------------------------------
        + SScrollBox::Slot()
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SAssignNew(CardsGrid, SUniformGridPanel)
            .SlotPadding(FMargin{0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small})
        ]

        // ---- Unique archetypes -----------------------------------------------------------
        + SScrollBox::Slot()
        .Padding(FCkDebuggerStyle::Padding_Medium, 0.0f, FCkDebuggerStyle::Padding_Medium, FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Panel.Border"))
            .Padding(FCkDebuggerStyle::Padding_Medium)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, FCkDebuggerStyle::Padding_Small)
                [
                    // Renamed from "SINGLETONS": the predicate below is Count==1, a population
                    // reading, and every reader took the old label for an ECS registry singleton.
                    ck_debugger_page_dashboard::MakeSectionHeader(
                        TEXT("UNIQUE ARCHETYPES"),
                        FText::FromString(TEXT("Archetypes with exactly one live entity in this world right now — a population count, not a registry singleton. Click the pill to open the instance.")))
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(SingletonsBox, SVerticalBox)
                ]
            ]
        ];

    DoRefresh();
    return Content;
}

auto FCkDebuggerPage_Dashboard::Tick(float InDeltaTime) -> void
{
    if (NOT IsActivePage)
    { return; }

    RefreshActiveArchTokens();

    TimeSinceRefresh += InDeltaTime;
    if (TimeSinceRefresh < 1.0f)
    { return; }

    TimeSinceRefresh = 0.0f;
    DoRefresh();
}

auto FCkDebuggerPage_Dashboard::RefreshActiveArchTokens() -> void
{
    const auto FilterText = GetEntityFilter ? GetEntityFilter() : FString{};
    if (LastSeenFilterText.IsSet() && LastSeenFilterText.GetValue() == FilterText)
    { return; }

    LastSeenFilterText = FilterText;
    ActiveArchTokens = ck::ecs_debugger_query::Get_ArchTokens(FilterText);
}

// =====================================================================================================================

auto FCkDebuggerPage_Dashboard::DoRefresh() -> void
{
    namespace dashboard = ck_debugger_page_dashboard;

    if (NOT WorldModel.IsValid() || NOT CardsGrid.IsValid())
    { return; }

    const auto& Entities = WorldModel->Get_CachedEntities();
    const auto Buckets = ck::ecs_debugger_aggregation::Aggregate(Entities);

    // ---- Hero ------------------------------------------------------------------------
    if (HeroCountText.IsValid())
    { HeroCountText->SetText(FText::AsNumber(Entities.Num())); }

    if (HeroSubText.IsValid())
    {
        HeroSubText->SetText(FText::FromString(FString::Printf(TEXT("entities in %s · live"),
            *dashboard::Get_WorldLabel(WorldModel->Get_SelectedWorld()))));
    }

    if (TotalSamples.IsValid())
    { dashboard::PushSample(*TotalSamples, static_cast<float>(Entities.Num())); }

    // ---- Population by family ----------------------------------------------------------
    auto FamilySums = TMap<FName, int32>{};
    for (const auto& Bucket : Buckets)
    { FamilySums.FindOrAdd(Bucket.Family) += Bucket.Count; }

    auto SortedFamilies = FamilySums.Array();
    SortedFamilies.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
    {
        if (A.Value != B.Value)
        { return A.Value > B.Value; }
        return A.Key.LexicalLess(B.Key);
    });

    const auto MaxFamilyCount = SortedFamilies.IsEmpty() ? 1 : FMath::Max(1, SortedFamilies[0].Value);

    auto NewFamilies = TArray<FName>{};
    for (const auto& [Family, Count] : SortedFamilies)
    { NewFamilies.Add(Family); }

    if (NewFamilies != PresentedFamilies && FamilyBox.IsValid())
    {
        for (auto It = FamilyCache.CreateIterator(); It; ++It)
        {
            if (NOT NewFamilies.Contains(It->Key))
            { It.RemoveCurrent(); }
        }

        FamilyBox->ClearChildren();
        for (const auto& Family : NewFamilies)
        {
            auto& Entry = FamilyCache.FindOrAdd(Family);
            if (NOT Entry.RowWidget.IsValid())
            { DoCreateFamilyRow(Family, Entry); }

            FamilyBox->AddSlot()
            .AutoHeight()
            .Padding(ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                Entry.RowWidget.ToSharedRef()
            ];
        }
        PresentedFamilies = MoveTemp(NewFamilies);
    }

    for (const auto& [Family, Count] : SortedFamilies)
    {
        auto* Entry = FamilyCache.Find(Family);
        if (Entry == nullptr)
        { continue; }

        if (Entry->BarBox.IsValid())
        {
            Entry->BarBox->SetWidthOverride(FMath::Max(4.0f,
                dashboard::FamilyBarMaxWidth * Count / MaxFamilyCount));
        }
        if (Entry->CountText.IsValid() && Entry->LastCount != Count)
        {
            Entry->CountText->SetText(FText::AsNumber(Count));
            Entry->LastCount = Count;
        }
    }

    // ---- Archetype cards ----------------------------------------------------------------
    auto PresentedBuckets = TArray<const ck::ecs_debugger_aggregation::FArchetypeBucket*>{};
    for (const auto& Bucket : Buckets)
    {
        if (PresentedBuckets.Num() >= dashboard::MaxCards)
        { break; }
        PresentedBuckets.Add(&Bucket);
    }

    auto NewCardKeys = TArray<FString>{};
    for (const auto* Bucket : PresentedBuckets)
    { NewCardKeys.Add(Bucket->Key); }

    if (NewCardKeys != PresentedCardKeys)
    {
        for (auto It = CardCache.CreateIterator(); It; ++It)
        {
            if (NOT NewCardKeys.Contains(It->Key))
            { It.RemoveCurrent(); }
        }

        CardsGrid->ClearChildren();
        auto SlotIndex = 0;
        for (const auto* Bucket : PresentedBuckets)
        {
            auto& Entry = CardCache.FindOrAdd(Bucket->Key);
            if (NOT Entry.CardWidget.IsValid())
            { DoCreateCard(*Bucket, Entry); }

            CardsGrid->AddSlot(SlotIndex % dashboard::CardColumns, SlotIndex / dashboard::CardColumns)
            [
                Entry.CardWidget.ToSharedRef()
            ];
            ++SlotIndex;
        }
        PresentedCardKeys = MoveTemp(NewCardKeys);
    }

    for (const auto* Bucket : PresentedBuckets)
    {
        auto* Entry = CardCache.Find(Bucket->Key);
        if (Entry == nullptr)
        { continue; }

        if (Entry->Samples.IsValid())
        { dashboard::PushSample(*Entry->Samples, static_cast<float>(Bucket->Count)); }

        if (Entry->CountText.IsValid() && Entry->LastCount != Bucket->Count)
        {
            Entry->CountText->SetText(FText::AsNumber(Bucket->Count));
            Entry->LastCount = Bucket->Count;
        }
    }

    // ---- Unique archetypes -------------------------------------------------------------
    // "Count == 1" is a POPULATION reading, not an ECS singleton — the section header's tooltip
    // says so. The whole qualifying set is counted before the cap is applied, so the overflow
    // indicator below can state what the cap hid instead of dropping it silently.
    auto Singletons = TArray<const ck::ecs_debugger_aggregation::FArchetypeBucket*>{};
    auto SingletonTotal = 0;
    for (const auto& Bucket : Buckets)
    {
        if (Bucket.Count != 1)
        { continue; }

        ++SingletonTotal;
        if (Singletons.Num() >= dashboard::MaxSingletons)
        { continue; }

        Singletons.Add(&Bucket);
    }

    const auto SingletonOverflow = SingletonTotal - Singletons.Num();

    auto NewSingletonKeys = TArray<FString>{};
    for (const auto* Bucket : Singletons)
    { NewSingletonKeys.Add(Bucket->Key); }

    if ((NewSingletonKeys != PresentedSingletonKeys || SingletonOverflow != PresentedSingletonOverflow)
        && SingletonsBox.IsValid())
    {
        SingletonsBox->ClearChildren();

        for (const auto* Bucket : Singletons)
        {
            const auto* IconBrush = Bucket->Get_IconBrush();
            if (IconBrush == nullptr)
            { IconBrush = FCkIconStyle::Get_Brush(ECk_Icon::Entity, ECk_Icon_BrushSize::Size_16x16); }

            const auto Representative = Bucket->Representative;

            SingletonsBox->AddSlot()
            .AutoHeight()
            .Padding(ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SCkDebug_Icon)
                    .Brush(IconBrush)
                    .Meaning(FText::FromString(Bucket->DisplayName))
                    .ColorAndOpacity(Bucket->IconColor)
                    .Size(ck_debugger_page_dashboard::IconSquare(14.0f))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(FText::FromString(Bucket->DisplayName))
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_EntityRef)
                    .Entity_Lambda([Representative]() { return Representative; })
                ]
            ];
        }

        // Same contract as the inspector's diff cap (CkDebuggerPanel_Inspector.cpp:642): the cap is
        // stated, never silent, so "eight rows" can't be mistaken for "eight unique archetypes".
        if (SingletonOverflow > 0)
        {
            SingletonsBox->AddSlot()
            .AutoHeight()
            .Padding(ck::debug_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Muted"))
                .Text(FText::FromString(ck::Format_UE(TEXT("+{} more"), SingletonOverflow)))
                .ToolTipText(FText::FromString(ck::Format_UE(
                    TEXT("{} archetypes have exactly one live entity; only the first {} are listed."),
                    SingletonTotal, ck_debugger_page_dashboard::MaxSingletons)))
            ];
        }

        PresentedSingletonKeys = MoveTemp(NewSingletonKeys);
        PresentedSingletonOverflow = SingletonOverflow;
    }
}

auto FCkDebuggerPage_Dashboard::DoCreateFamilyRow(FName InFamily, FFamilyEntry& OutEntry) -> void
{
    OutEntry.LastCount = -1;
    OutEntry.CountText =
        SNew(STextBlock)
        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
        .Text(FText::FromString(TEXT("0")))
        .ColorAndOpacity(CkStyle::Text());

    OutEntry.RowWidget =
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(110.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromName(InFamily))
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
        [
            SAssignNew(OutEntry.BarBox, SBox)
            .WidthOverride(4.0f)
            .HeightOverride(7.0f)
            [
                SNew(SBorder)
                .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
                .BorderBackgroundColor(FSlateColor{CkStyle::Selection()})
                .Padding(0.0f)
                [
                    SNullWidget::NullWidget
                ]
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            OutEntry.CountText.ToSharedRef()
        ];
}

auto FCkDebuggerPage_Dashboard::DoCreateCard(
    const ck::ecs_debugger_aggregation::FArchetypeBucket& InBucket,
    FCardEntry& OutEntry) -> void
{
    const auto* IconBrush = InBucket.Get_IconBrush();
    if (IconBrush == nullptr)
    { IconBrush = FCkIconStyle::Get_Brush(ECk_Icon::Entity, ECk_Icon_BrushSize::Size_16x16); }

    const auto AccentColor = InBucket.IconColor;

    // Signature badges from the representative bits.
    auto Badges = SNew(SHorizontalBox);
    auto BadgeCount = 0;
    for (const auto& [FeatureId, Bit] : ck::ecs_debugger_feature_visuals::Get_BadgeFeatures())
    {
        if ((InBucket.SignatureBits & (uint64{1} << Bit)) == 0)
        { continue; }
        if (BadgeCount >= ck::ecs_debugger_feature_visuals::MaxBadges)
        { break; }

        const auto* Visual = ck::ecs_debugger_feature_visuals::Get_FeatureVisuals().Find(FeatureId);
        const auto* Brush = Visual != nullptr ? FCkIconStyle::Get_Brush(Visual->Icon, ECk_Icon_BrushSize::Size_16x16) : nullptr;
        if (Brush == nullptr)
        { continue; }

        Badges->AddSlot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 2.0f, 0.0f)
        [
            SNew(SCkDebug_Icon)
            .Brush(Brush)
            .Meaning(FText::FromName(FeatureId))
            .ColorAndOpacity(Visual->Color)
            .Size(ck_debugger_page_dashboard::IconSquare(11.0f))
        ];
        ++BadgeCount;
    }

    OutEntry.LastCount = InBucket.Count;
    OutEntry.Samples = MakeShared<TArray<float>>();
    OutEntry.CountText =
        SNew(STextBlock)
        // Deliberate outlier, same reasoning as the hero count: the card's population number is
        // sized above every CkStyle role on purpose and only adopts the TextScale axis.
        .Font_Lambda([]() { return ck::debug_axes::ScaledFont("Bold", 14); })
        .Text(FText::AsNumber(InBucket.Count))
        .ColorAndOpacity(CkStyle::TextStrong());

    const auto FilterToken = InBucket.FilterToken;
    const auto TokenLower = FilterToken.ToLower();

    OutEntry.CardWidget =
        SNew(SCkDebug_ToggleSurface)
        .ToolTipText(FText::FromString(FString::Printf(
            TEXT("Toggle arch:%s in the entity tree filter."), *FilterToken)))
        .AccessibleText(FText::FromString(FString::Printf(TEXT("Archetype filter %s"), *FilterToken)))
        .IsOn_Lambda([this, TokenLower]() -> bool
        {
            return ActiveArchTokens.Contains(TokenLower);
        })
        .OnStateChanged_Lambda([this, FilterToken](bool InIsOn)
        {
            if (NOT RequestEntityFilter)
            { return; }

            RequestEntityFilter(ck::ecs_debugger_query::Toggle_ArchToken(
                GetEntityFilter ? GetEntityFilter() : FString{},
                FilterToken, InIsOn));
            LastSeenFilterText.Reset();
        })
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    // The card's icon well predates the IconTreatment axis and must survive Classic
                    // (whose treatment is Plain, i.e. SCkDebug_Icon draws no backdrop of its own).
                    // SCkDebug_Icon exposes no per-site treatment override, so the well stays local —
                    // but only while the axis draws nothing. Under Well/Ring the widget's own backdrop
                    // takes over and this border collapses to a no-brush, zero-padding shell, so the
                    // two never stack. All three values are lambda-bound: the flip is live (R1).
                    SNew(SBorder)
                    .BorderImage_Lambda([]() -> const FSlateBrush*
                    {
                        return ck_debugger_page_dashboard::Is_IconWellLocal()
                            ? FCkDebuggerStyle::Get_IconWellBrush()
                            : FStyleDefaults::GetNoBrush();
                    })
                    .BorderBackgroundColor_Lambda([AccentColor]() -> FSlateColor
                    {
                        return FSlateColor{ck_debugger_page_dashboard::Is_IconWellLocal()
                            ? CkStyle::OverlayOf(AccentColor, CkStyle::IconWellAlpha())
                            : FLinearColor::Transparent};
                    })
                    .Padding_Lambda([]() -> FMargin
                    {
                        return ck_debugger_page_dashboard::Is_IconWellLocal()
                            ? FMargin{CkStyle::SpaceS}
                            : FMargin{0.0f};
                    })
                    [
                        SNew(SCkDebug_Icon)
                        .Brush(IconBrush)
                        .Meaning(FText::FromString(InBucket.DisplayName))
                        .ColorAndOpacity(AccentColor)
                        .Accent(AccentColor)
                        .Size(ck_debugger_page_dashboard::IconSquare(14.0f))
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
                    .Text(FText::FromString(InBucket.DisplayName))
                    .ColorAndOpacity(CkStyle::Text())
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                [
                    OutEntry.CountText.ToSharedRef()
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                Badges
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Muted"))
                .Text(FText::FromName(InBucket.Family))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
            [
                SNew(SCkEcsDebugger_Sparkline)
                .Samples(OutEntry.Samples)
                .Color(AccentColor)
                .DesiredSize(FVector2D(120.0f, 18.0f))
            ]
        ];
}
