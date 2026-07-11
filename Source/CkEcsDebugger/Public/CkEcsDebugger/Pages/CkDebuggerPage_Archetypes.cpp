#include "CkDebuggerPage_Archetypes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkEcs/Archetype/CkArchetype_Registry.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

// =====================================================================================================================

FCkDebuggerPage_Archetypes::~FCkDebuggerPage_Archetypes() = default;

auto FCkDebuggerPage_Archetypes::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Archetypes"));
}

auto FCkDebuggerPage_Archetypes::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Archetypes::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Archetypes::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;

    // Force a rebuild on next active tick so the page is fresh when switched to.
    if (InIsActive)
    { TimeSinceRebuild = MAX_flt; }
}

// =====================================================================================================================

auto FCkDebuggerPage_Archetypes::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    WorldModel = InContext.WorldModel;
    RequestEntityFilter = InContext.RequestEntityFilter;

    const auto Content =
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SAssignNew(HeroText, STextBlock)
            .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
            .Text(FText::FromString(TEXT("No world observed")))
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(FCkDebuggerStyle::Padding_Medium, 0.0f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SAssignNew(CardsBox, SWrapBox)
                .UseAllottedSize(true)
            ]
        ];

    RebuildCards();
    return Content;
}

auto FCkDebuggerPage_Archetypes::Tick(float InDeltaTime) -> void
{
    if (NOT IsActivePage)
    { return; }

    // 1 Hz rebuild while visible — the aggregation walks every cached entity, so it
    // stays off the per-frame path; a hidden tab does no work at all (spec §5 gate).
    TimeSinceRebuild += InDeltaTime;
    if (TimeSinceRebuild < 1.0f)
    { return; }

    TimeSinceRebuild = 0.0f;
    RebuildCards();
}

auto FCkDebuggerPage_Archetypes::RebuildCards() -> void
{
    if (NOT CardsBox.IsValid() || NOT WorldModel.IsValid())
    { return; }

    const auto& Entities = WorldModel->Get_CachedEntities();

    // Aggregate by registry-first archetype key (spec §3.3 consumption).
    auto Buckets = TMap<FString, FArchetypeBucket>{};

    for (const auto& Entity : Entities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        const auto Bits = ck::debug_feature_flags::Get_Flags(Entity.Get_RegistryView(), Entity.Get_Entity());
        const auto Registered = ck::archetype_registry::TryGet_BestMatchName(Entity);

        auto Key = FString{};
        auto DisplayName = FString{};
        auto FilterToken = FString{};

        if (Registered.IsNone())
        {
            const auto CleanName = ck::DebugNameClean::Get_CleanName(
                UCk_Utils_Handle_UE::Get_DebugName(Entity).ToString());
            Key = ck::ecs_debugger_query::Get_InferredArchetypeKey(CleanName, Bits);

            if (NOT Key.Split(TEXT("#"), &DisplayName, nullptr))
            { DisplayName = Key; }
            FilterToken = DisplayName;
        }
        else
        {
            Key = Registered.ToString();
            DisplayName = Key;
            FilterToken = Key;
        }

        auto& Bucket = Buckets.FindOrAdd(Key);
        if (Bucket.Count == 0)
        {
            Bucket.Key = Key;
            Bucket.DisplayName = DisplayName;
            Bucket.FilterToken = FilterToken;
            Bucket.IsRegistered = NOT Registered.IsNone();
            Bucket.SignatureBits = Bits;

            if (Bucket.IsRegistered)
            {
                if (const auto Descriptor = ck::archetype_registry::Find(Registered); Descriptor.IsSet())
                {
                    // IconSvgPath resolution is game-plugin territory; v1 uses the id as
                    // a style icon name when it happens to match, else the Cube fallback.
                    Bucket.IconName = FName{FPaths::GetBaseFilename(Descriptor->Get_IconSvgPath())};
                }
            }
        }
        ++Bucket.Count;
    }

    auto Sorted = TArray<FArchetypeBucket>{};
    Buckets.GenerateValueArray(Sorted);
    Sorted.Sort([](const FArchetypeBucket& A, const FArchetypeBucket& B)
    {
        // Registered game archetypes first, then by population. Key tie-break keeps the
        // order deterministic — Sort is unstable, and a shuffling order would defeat the
        // no-reslot fast path below.
        if (A.IsRegistered != B.IsRegistered)
        { return A.IsRegistered; }
        if (A.Count != B.Count)
        { return A.Count > B.Count; }
        return A.Key < B.Key;
    });

    // Skip single-instance inferred buckets past the first screenful — they are noise
    // at scale; the tree remains the place to browse one-offs.
    auto Presented = TArray<const FArchetypeBucket*>{};
    for (const auto& Bucket : Sorted)
    {
        if (NOT Bucket.IsRegistered && Bucket.Count <= 1 && Presented.Num() >= 40)
        { continue; }
        Presented.Add(&Bucket);
    }

    if (HeroText.IsValid())
    {
        HeroText->SetText(FText::FromString(
            FString::Printf(TEXT("%d entities · %d archetypes"), Entities.Num(), Buckets.Num())));
    }

    auto NewKeys = TArray<FString>{};
    NewKeys.Reserve(Presented.Num());
    for (const auto* Bucket : Presented)
    { NewKeys.Add(Bucket->Key); }

    if (NewKeys == PresentedKeys)
    {
        // Same cards in the same order — update counts in place. No ClearChildren, no
        // widget churn, no flicker.
        for (const auto* Bucket : Presented)
        {
            auto* Entry = CardCache.Find(Bucket->Key);
            if (Entry != nullptr && Entry->LastCount != Bucket->Count && Entry->CountText.IsValid())
            {
                Entry->CountText->SetText(FText::AsNumber(Bucket->Count));
                Entry->LastCount = Bucket->Count;
            }
        }
        return;
    }

    // Key set (or order) changed: re-slot, reusing surviving card widgets by key.
    for (auto It = CardCache.CreateIterator(); It; ++It)
    {
        if (NOT NewKeys.Contains(It->Key))
        { It.RemoveCurrent(); }
    }

    CardsBox->ClearChildren();
    for (const auto* Bucket : Presented)
    {
        auto& Entry = CardCache.FindOrAdd(Bucket->Key);
        if (NOT Entry.CardWidget.IsValid())
        { DoCreateCard(*Bucket, Entry); }

        if (Entry.LastCount != Bucket->Count && Entry.CountText.IsValid())
        {
            Entry.CountText->SetText(FText::AsNumber(Bucket->Count));
            Entry.LastCount = Bucket->Count;
        }

        CardsBox->AddSlot()
        .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small)
        [
            Entry.CardWidget.ToSharedRef()
        ];
    }

    PresentedKeys = MoveTemp(NewKeys);
}

auto FCkDebuggerPage_Archetypes::DoCreateCard(
    const FArchetypeBucket& InBucket,
    FCardCacheEntry& OutEntry) -> void
{
    const auto* IconBrush = FCkDebuggerStyle::Get_IconBrush(InBucket.IconName);
    if (IconBrush == nullptr)
    { IconBrush = FCkDebuggerStyle::Get_IconBrush(TEXT("Cube")); }

    const auto AccentColor = InBucket.IsRegistered ? CkStyle::Selection() : CkStyle::TextDim();

    // Header row: tinted icon well + name (+ GAME pill for registered archetypes).
    auto HeaderRow = SNew(SHorizontalBox);

    HeaderRow->AddSlot()
    .AutoWidth()
    .VAlign(VAlign_Center)
    .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
    [
        SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Card.IconWell"))
        .BorderBackgroundColor(FSlateColor{AccentColor.CopyWithNewOpacity(0.15f)})
        .Padding(FMargin{5.0f})
        [
            SNew(SImage)
            .Image(IconBrush)
            .ColorAndOpacity(AccentColor)
            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
        ]
    ];

    HeaderRow->AddSlot()
    .FillWidth(1.0f)
    .VAlign(VAlign_Center)
    [
        SNew(STextBlock)
        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold"))
        .Text(FText::FromString(InBucket.DisplayName))
        .ColorAndOpacity(CkStyle::Text())
    ];

    if (InBucket.IsRegistered)
    {
        HeaderRow->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Badge.Rounded"))
            .BorderBackgroundColor(FSlateColor{CkStyle::Selection().CopyWithNewOpacity(0.18f)})
            .Padding(FMargin{4.0f, 1.0f})
            [
                SNew(STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                .Text(FText::FromString(TEXT("GAME")))
                .ColorAndOpacity(CkStyle::Selection())
            ]
        ];
    }

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
    OutEntry.CountText =
        SNew(STextBlock)
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
        .Text(FText::AsNumber(InBucket.Count))
        .ColorAndOpacity(CkStyle::TextStrong());

    const auto FilterToken = InBucket.FilterToken;

    OutEntry.CardWidget =
        SNew(SButton)
        .ButtonStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugger.Card"))
        .ContentPadding(FMargin{FCkDebuggerStyle::Padding_Medium})
        .ToolTipText(FText::FromString(FString::Printf(TEXT("Filter the entity tree to arch:%s"), *FilterToken)))
        .OnClicked_Lambda([this, FilterToken]() -> FReply
        {
            if (RequestEntityFilter)
            { RequestEntityFilter(FString::Printf(TEXT("arch:%s"), *FilterToken)); }
            return FReply::Handled();
        })
        [
            SNew(SBox)
            .MinDesiredWidth(170.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    HeaderRow
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Bottom)
                    [
                        OutEntry.CountText.ToSharedRef()
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Bottom)
                    .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 2.0f)
                    [
                        SNew(STextBlock)
                        .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                        .Text(FText::FromString(TEXT("instances")))
                        .ColorAndOpacity(CkStyle::TextMute())
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f)
                [
                    Badges
                ]
            ]
        ];
}
