#include "CkDebuggerPage_Dashboard.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/SCkEcsDebugger_Sparkline.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SCheckBox.h"
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

    static auto MakeSectionHeader(const TCHAR* InLabel) -> TSharedRef<SWidget>
    {
        return SNew(STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
            .Text(FText::FromString(InLabel))
            .ColorAndOpacity(CkStyle::TextMute());
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
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
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

        // ---- Singletons ----------------------------------------------------------------
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
                    ck_debugger_page_dashboard::MakeSectionHeader(TEXT("SINGLETONS"))
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
            .Padding(0.0f, 1.0f)
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

    // ---- Singletons ------------------------------------------------------------------
    auto Singletons = TArray<const ck::ecs_debugger_aggregation::FArchetypeBucket*>{};
    for (const auto& Bucket : Buckets)
    {
        if (Bucket.Count != 1)
        { continue; }
        if (Singletons.Num() >= dashboard::MaxSingletons)
        { break; }
        Singletons.Add(&Bucket);
    }

    auto NewSingletonKeys = TArray<FString>{};
    for (const auto* Bucket : Singletons)
    { NewSingletonKeys.Add(Bucket->Key); }

    if (NewSingletonKeys != PresentedSingletonKeys && SingletonsBox.IsValid())
    {
        SingletonsBox->ClearChildren();

        for (const auto* Bucket : Singletons)
        {
            const auto* IconBrush = FCkDebuggerStyle::Get_IconBrush(Bucket->IconName);
            if (IconBrush == nullptr)
            { IconBrush = FCkDebuggerStyle::Get_IconBrush(TEXT("Cube")); }

            const auto Representative = Bucket->Representative;

            SingletonsBox->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 1.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SImage)
                    .Image(IconBrush)
                    .ColorAndOpacity(Bucket->IconColor)
                    .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
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
        PresentedSingletonKeys = MoveTemp(NewSingletonKeys);
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
    const auto* IconBrush = FCkDebuggerStyle::Get_IconBrush(InBucket.IconName);
    if (IconBrush == nullptr)
    { IconBrush = FCkDebuggerStyle::Get_IconBrush(TEXT("Cube")); }

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
        const auto* Brush = Visual != nullptr ? FCkDebuggerStyle::Get_IconBrush(Visual->IconName) : nullptr;
        if (Brush == nullptr)
        { continue; }

        Badges->AddSlot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 2.0f, 0.0f)
        [
            SNew(SImage)
            .Image(Brush)
            .ColorAndOpacity(Visual->Color)
            .DesiredSizeOverride(FVector2D(11.0f, 11.0f))
        ];
        ++BadgeCount;
    }

    OutEntry.LastCount = InBucket.Count;
    OutEntry.Samples = MakeShared<TArray<float>>();
    OutEntry.CountText =
        SNew(STextBlock)
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        .Text(FText::AsNumber(InBucket.Count))
        .ColorAndOpacity(CkStyle::TextStrong());

    const auto FilterToken = InBucket.FilterToken;
    const auto TokenLower = FilterToken.ToLower();

    OutEntry.CardWidget =
        SNew(SCheckBox)
        .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CkDebugger.CardToggle"))
        .ToolTipText(FText::FromString(FString::Printf(
            TEXT("Toggle arch:%s in the entity tree filter."), *FilterToken)))
        .IsChecked_Lambda([this, TokenLower]()
        {
            return ActiveArchTokens.Contains(TokenLower)
                ? ECheckBoxState::Checked
                : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([this, FilterToken](ECheckBoxState InState)
        {
            if (NOT RequestEntityFilter)
            { return; }

            RequestEntityFilter(ck::ecs_debugger_query::Toggle_ArchToken(
                GetEntityFilter ? GetEntityFilter() : FString{},
                FilterToken, InState == ECheckBoxState::Checked));
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
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Card.IconWell"))
                    .BorderBackgroundColor(FSlateColor{AccentColor.CopyWithNewOpacity(0.15f)})
                    .Padding(FMargin{4.0f})
                    [
                        SNew(SImage)
                        .Image(IconBrush)
                        .ColorAndOpacity(AccentColor)
                        .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
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
