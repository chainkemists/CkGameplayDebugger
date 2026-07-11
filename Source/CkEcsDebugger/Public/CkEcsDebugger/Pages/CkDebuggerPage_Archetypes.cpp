#include "CkDebuggerPage_Archetypes.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkEcs/Archetype/CkArchetype_Registry.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
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
    GetEntityFilter = InContext.GetEntityFilter;

    // Column-density chips (2..6) — radio semantics against the persisted setting.
    auto ColumnChips = SNew(SHorizontalBox);
    for (auto Columns = 2; Columns <= 6; ++Columns)
    {
        ColumnChips->AddSlot()
        .AutoWidth()
        .Padding(2.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SCheckBox)
            .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CkDebugger.ToggleChip"))
            .ToolTipText(FText::FromString(FString::Printf(TEXT("%d card columns"), Columns)))
            .IsChecked_Lambda([Columns]()
            {
                return Get_GridColumns() == Columns ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, Columns](ECheckBoxState)
            {
                Set_GridColumns(Columns);
            })
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::AsNumber(Columns))
            ]
        ];
    }

    const auto Content =
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FCkDebuggerStyle::Padding_Medium)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SAssignNew(HeroText, STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Header"))
                .Text(FText::FromString(TEXT("No world observed")))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(TEXT("Columns")))
                .ColorAndOpacity(CkStyle::TextMute())
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                ColumnChips
            ]
        ]

        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(FCkDebuggerStyle::Padding_Medium, 0.0f)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot()
            [
                SAssignNew(CardsBox, SUniformGridPanel)
                .SlotPadding(FMargin{0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, FCkDebuggerStyle::Padding_Small})
            ]
        ];

    RebuildCards();
    return Content;
}

auto FCkDebuggerPage_Archetypes::Tick(float InDeltaTime) -> void
{
    if (NOT IsActivePage)
    { return; }

    // Cheap per-frame: re-derive card checked states only when the filter text changed.
    RefreshActiveArchTokens();

    // 1 Hz rebuild while visible — the aggregation walks every cached entity, so it
    // stays off the per-frame path; a hidden tab does no work at all (spec §5 gate).
    TimeSinceRebuild += InDeltaTime;
    if (TimeSinceRebuild < 1.0f)
    { return; }

    TimeSinceRebuild = 0.0f;
    RebuildCards();
}

auto FCkDebuggerPage_Archetypes::RefreshActiveArchTokens() -> void
{
    const auto FilterText = GetEntityFilter ? GetEntityFilter() : FString{};
    if (LastSeenFilterText.IsSet() && LastSeenFilterText.GetValue() == FilterText)
    { return; }

    LastSeenFilterText = FilterText;
    ActiveArchTokens.Reset();

    for (const auto& Term : ck::ecs_debugger_query::Parse(FilterText).Terms)
    {
        if (Term.Type == ck::ecs_debugger_query::ETermType::Arch)
        { ActiveArchTokens.Add(Term.Value); }   // Parse lowercases values
    }
}

auto FCkDebuggerPage_Archetypes::Toggle_ArchFilterToken(const FString& InToken, bool InEnable) -> void
{
    if (NOT RequestEntityFilter)
    { return; }

    const auto NeedsQuotes = InToken.Contains(TEXT(" "));
    const auto QuotedForm = FString::Printf(TEXT("arch:\"%s\""), *InToken);
    const auto BareForm = FString::Printf(TEXT("arch:%s"), *InToken);

    auto NewFilter = GetEntityFilter ? GetEntityFilter() : FString{};

    // Remove an existing occurrence — quoted form is unambiguous; the bare form only
    // when it ends at a word boundary (so arch:Timer never clips arch:TimerX).
    NewFilter = NewFilter.Replace(*QuotedForm, TEXT(""), ESearchCase::IgnoreCase);
    if (NOT NeedsQuotes)
    {
        auto SearchFrom = 0;
        while (true)
        {
            const auto FoundIndex = NewFilter.Find(BareForm, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
            if (FoundIndex == INDEX_NONE)
            { break; }

            const auto EndIndex = FoundIndex + BareForm.Len();
            if (EndIndex >= NewFilter.Len() || FChar::IsWhitespace(NewFilter[EndIndex]))
            {
                NewFilter.RemoveAt(FoundIndex, BareForm.Len());
                continue;
            }
            SearchFrom = EndIndex;
        }
    }

    if (InEnable)
    {
        NewFilter.TrimStartAndEndInline();
        NewFilter += NewFilter.IsEmpty() ? FString{} : FString{TEXT(" ")};
        NewFilter += NeedsQuotes ? QuotedForm : BareForm;
    }

    while (NewFilter.Contains(TEXT("  ")))
    { NewFilter = NewFilter.Replace(TEXT("  "), TEXT(" ")); }
    NewFilter.TrimStartAndEndInline();

    RequestEntityFilter(NewFilter);
    LastSeenFilterText.Reset();   // force checked-state re-derive on the next tick
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
                    // a style icon name when it happens to match, else the fallback below.
                    const auto BespokeIcon = FName{FPaths::GetBaseFilename(Descriptor->Get_IconSvgPath())};
                    if (FCkDebuggerStyle::Get_IconBrush(BespokeIcon) != nullptr)
                    { Bucket.IconName = BespokeIcon; }
                }
            }

            // Inferred buckets prefer the dominant-feature glyph (semantic): a Timer
            // swarm reads as timers. Registered archetypes skip this — each named
            // archetype deserves its own identity from the general pool below.
            if (Bucket.IconName.IsNone() && NOT Bucket.IsRegistered)
            {
                for (const auto& [FeatureId, Bit] : ck::ecs_debugger_feature_visuals::Get_BadgeFeatures())
                {
                    if ((Bits & (uint64{1} << Bit)) == 0)
                    { continue; }

                    if (const auto* Visual = ck::ecs_debugger_feature_visuals::Get_FeatureVisuals().Find(FeatureId))
                    {
                        Bucket.IconName = Visual->IconName;
                        Bucket.IconColor = Visual->Color;
                        break;
                    }
                }
            }

            // General-pool assignment: stable hash of the archetype key picks a glyph
            // from Resources/Icons/General — distinct identity instead of anonymous
            // cubes. Cube survives only if the pool is empty.
            if (Bucket.IconName.IsNone())
            {
                const auto& Pool = FCkDebuggerStyle::Get_GeneralIconPool();
                if (Pool.Num() > 0)
                { Bucket.IconName = Pool[FCrc::StrCrc32(*Key) % static_cast<uint32>(Pool.Num())]; }
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

    const auto Columns = FMath::Clamp(Get_GridColumns(), 1, 8);

    if (NewKeys == PresentedKeys && LastSlottedColumns == Columns)
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

    // Key set (or order or density) changed: re-slot, reusing surviving card widgets by key.
    for (auto It = CardCache.CreateIterator(); It; ++It)
    {
        if (NOT NewKeys.Contains(It->Key))
        { It.RemoveCurrent(); }
    }

    CardsBox->ClearChildren();
    auto SlotIndex = 0;
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

        CardsBox->AddSlot(SlotIndex % Columns, SlotIndex / Columns)
        [
            Entry.CardWidget.ToSharedRef()
        ];
        ++SlotIndex;
    }

    PresentedKeys = MoveTemp(NewKeys);
    LastSlottedColumns = Columns;
}

auto FCkDebuggerPage_Archetypes::Get_GridColumns() -> int32
{
    return FMath::Clamp(UCkEcsDebuggerSettings::Get()->ArchetypeGridColumns, 1, 8);
}

auto FCkDebuggerPage_Archetypes::Set_GridColumns(int32 InColumns) -> void
{
    if (auto* Settings = GetMutableDefault<UCkEcsDebuggerSettings>())
    {
        Settings->ArchetypeGridColumns = FMath::Clamp(InColumns, 1, 8);
        Settings->SaveConfig();
    }

    // Re-slot immediately so the click lands this frame instead of on the 1 Hz tick.
    RebuildCards();
}

auto FCkDebuggerPage_Archetypes::DoCreateCard(
    const FArchetypeBucket& InBucket,
    FCardCacheEntry& OutEntry) -> void
{
    const auto* IconBrush = FCkDebuggerStyle::Get_IconBrush(InBucket.IconName);
    if (IconBrush == nullptr)
    { IconBrush = FCkDebuggerStyle::Get_IconBrush(TEXT("Cube")); }

    const auto AccentColor = InBucket.IconColor.Get(
        InBucket.IsRegistered ? CkStyle::Selection() : CkStyle::TextDim());

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
        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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
    const auto TokenLower = FilterToken.ToLower();

    OutEntry.CardWidget =
        SNew(SCheckBox)
        .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CkDebugger.CardToggle"))
        .ToolTipText(FText::FromString(FString::Printf(
            TEXT("Toggle arch:%s in the entity tree filter. Toggle several cards to see them together."), *FilterToken)))
        .IsChecked_Lambda([this, TokenLower]()
        {
            return ActiveArchTokens.Contains(TokenLower)
                ? ECheckBoxState::Checked
                : ECheckBoxState::Unchecked;
        })
        .OnCheckStateChanged_Lambda([this, FilterToken](ECheckBoxState InState)
        {
            Toggle_ArchFilterToken(FilterToken, InState == ECheckBoxState::Checked);
        })
        [
            // Width comes from the uniform grid cell (panel width / column count).
            SNew(SBox)
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
