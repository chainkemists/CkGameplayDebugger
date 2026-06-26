#include "CkDebugOverlay_Present.h"

#include "CkEntityDebugOverlay/History/CkDebugOverlay_History.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Resolve.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_FocusCard.h"   // Get_ProviderColor
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"          // Get_LeafName / Get_ProviderAbbrev

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

// ====================================================================================================================

namespace
{
    // Ultra-condensed multi-line plates for candidates within NearDist of the camera.
    // (Owned here now that Build_WorldTags lives in this TU; the overlay popover still
    // reads it by name via IConsoleManager.)
    TAutoConsoleVariable<int32> CVar_DebugOverlay_NearPlates(
        TEXT("ck.DebugOverlay.NearPlates"),
        1,
        TEXT("1 = show ultra-condensed multi-line plates for candidates within NearDist; 0 = single-line pills only."),
        ECVF_Cheat);

    // Returns true if InProviderTag is in the always-on set (force-included regardless
    // of the active layout, as long as CanProvide). Currently: StateMachine.
    auto Is_AlwaysOnProvider(const FGameplayTag& InProviderTag) -> bool
    {
        // Leaf-name compare avoids a hard dep on the SM provider's tag declaration here.
        return ck_debugoverlay::Get_LeafName(InProviderTag) == TEXT("StateMachine");
    }
}

// ====================================================================================================================

auto
    ck_debugoverlay::
    Build_EntityModel(
        const FCk_Handle&                                    InFocusEntity,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        FCk_DebugOverlay_History*                            InHistory,
        double                                               InNow)
    -> FCk_DebugOverlay_EntityModel
{
    auto OutModel   = FCk_DebugOverlay_EntityModel{};
    OutModel.Entity = InFocusEntity;

    if (ck::Is_NOT_Valid(InFocusEntity))
    { return OutModel; }

    // Header is rendered by SCkDebug_EntityRef in the FocusCard; OutModel.Header kept as
    // a fallback / for callers that may read it.
    {
        const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InFocusEntity);
        const auto CleanName = ck::DebugNameClean::Get_CleanName(DebugName.ToString());
        const auto EntityNum = static_cast<int32>(InFocusEntity.Get_Entity().Get_EntityNumber());
        OutModel.Header = FText::FromString(
            FString::Printf(TEXT("%s [%d]"), *CleanName, EntityNum));
    }

    const auto EntityId = static_cast<uint32>(InFocusEntity.Get_Entity().Get_EntityNumber());

    // Track which provider tags we've already emitted so always-on providers don't double-add.
    auto EmittedProviderTags = FGameplayTagContainer{};

    // ---- Layout-driven providers ----
    for (const auto& Provider : InProviders)
    {
        if (NOT Provider || NOT Provider->CanProvide(InFocusEntity))
        { continue; }

        const auto& ProviderTag   = Provider->Get_ProviderTag();
        const auto  EnabledFields = ck_debugoverlay::Resolve_EnabledFields(
            InLayout, ProviderTag, Provider->Get_FieldTags());

        if (EnabledFields.IsEmpty())
        { continue; }

        auto EntryFilter = FGameplayTagQuery{};
        for (const auto& Entry : InLayout.Entries)
        {
            if (Entry.ProviderTag == ProviderTag)
            {
                EntryFilter = Entry.EntryFilter;
                break;
            }
        }

        auto Config          = FCk_DebugOverlay_ProviderConfig{};
        Config.EnabledFields = EnabledFields;
        Config.EntryFilter   = EntryFilter;

        auto Section          = FCk_DebugOverlay_Section{};
        Section.ProviderTag   = ProviderTag;
        Section.SortPriority  = Provider->Get_SortPriority();

        Provider->Collect(InFocusEntity, Config, Section);

        for (const auto& Row : Section.Rows)
        {
            const auto Key = FCk_DebugOverlay_HistoryKey{ EntityId, Row.FieldTag };
            if (InHistory) { InHistory->Observe(Key, Row.Value.ToString(), InNow); }
        }

        EmittedProviderTags.AddTag(ProviderTag);
        OutModel.Sections.Add(MoveTemp(Section));
    }

    // ---- Always-on providers (force-include if CanProvide and not already emitted) ----
    for (const auto& Provider : InProviders)
    {
        if (NOT Provider)
        { continue; }

        const auto& ProviderTag = Provider->Get_ProviderTag();

        if (NOT Is_AlwaysOnProvider(ProviderTag))
        { continue; }

        if (EmittedProviderTags.HasTagExact(ProviderTag))
        { continue; }

        if (NOT Provider->CanProvide(InFocusEntity))
        { continue; }

        auto EnabledFields = FGameplayTagContainer{};
        for (const auto& FieldDesc : Provider->Get_FieldTags())
        {
            if (FieldDesc.DefaultEnabled)
            {
                EnabledFields.AddTag(FieldDesc.Tag);
            }
        }

        if (EnabledFields.IsEmpty())
        { continue; }

        auto Config          = FCk_DebugOverlay_ProviderConfig{};
        Config.EnabledFields = EnabledFields;

        auto Section          = FCk_DebugOverlay_Section{};
        Section.ProviderTag   = ProviderTag;
        Section.SortPriority  = Provider->Get_SortPriority();

        Provider->Collect(InFocusEntity, Config, Section);

        for (const auto& Row : Section.Rows)
        {
            const auto Key = FCk_DebugOverlay_HistoryKey{ EntityId, Row.FieldTag };
            if (InHistory) { InHistory->Observe(Key, Row.Value.ToString(), InNow); }
        }

        OutModel.Sections.Add(MoveTemp(Section));
    }

    return OutModel;
}

// ====================================================================================================================

auto
    ck_debugoverlay::
    Build_WorldTags(
        const TArray<FCk_Handle>&                            InHandles,
        const TArray<FCandidate>&                            InCandidates,
        const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders,
        const FCk_DebugOverlay_Layout&                       InLayout,
        APlayerController*                                   InPC,
        bool                                                 InIsEjected)
    -> TArray<FCk_DebugOverlay_WorldTagInfo>
{
    auto WorldTags = TArray<FCk_DebugOverlay_WorldTagInfo>{};

    // Tag positions come from PC-based screen projection, which reflects the frozen
    // player camera while ejected — skip then (the focus card still renders).
    if (ck::Is_NOT_Valid(InPC) || InIsEjected)
    { return WorldTags; }

    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();

    const auto MaxDist       = Settings ? Settings->MaxDist       : 5000.0f;
    const auto NearDist      = Settings ? Settings->NearDist      : 600.0f;
    const auto FarDist       = Settings ? Settings->FarDist       : 4000.0f;
    const auto MinScale      = Settings ? Settings->MinScale      : 0.5f;
    const auto FadeStartDist = Settings ? Settings->FadeStartDist : 3000.0f;
    const auto MaxNameChars  = Settings ? Settings->MaxWorldTagNameChars : 24;

    auto CamLoc = FVector::ZeroVector;
    {
        auto CamRot = FRotator::ZeroRotator;
        InPC->GetPlayerViewPoint(CamLoc, CamRot);
    }

    const auto NearPlatesEnabled = CVar_DebugOverlay_NearPlates.GetValueOnGameThread() != 0;

    for (auto CandIdx = 0; CandIdx < InCandidates.Num(); ++CandIdx)
    {
        if (NOT InCandidates[CandIdx].bIsOnScreen)
        { continue; }

        const auto& Handle = InHandles[CandIdx];

        const auto Dist = static_cast<float>(
            FVector::Dist(CamLoc, InCandidates[CandIdx].WorldLocation));
        if (Dist > MaxDist)
        { continue; }

        // Build compact token + feature badges (what the entity HAS, layout-independent).
        auto TokenParts = TArray<FString>{};
        auto Badges     = TArray<FCk_DebugOverlay_WorldTagBadge>{};
        for (const auto& Provider : InProviders)
        {
            if (NOT Provider || NOT Provider->CanProvide(Handle))
            { continue; }

            const auto& ProviderTag = Provider->Get_ProviderTag();

            // World tags survey feature presence; skip only EntityInfo (it IS the name/
            // header — a pill would be redundant). Transform now emits a "T" pill so every
            // feature an entity carries is visible.
            const auto ProviderLeaf = ck_debugoverlay::Get_LeafName(ProviderTag);
            if (ProviderLeaf == TEXT("EntityInfo"))
            { continue; }

            Badges.Add(FCk_DebugOverlay_WorldTagBadge{
                ck_debugoverlay::Get_ProviderAbbrev(ProviderLeaf),
                SCkDebugOverlay_FocusCard::Get_ProviderColor(ProviderTag) });

            const auto EnabledFields = ck_debugoverlay::Resolve_EnabledFields(
                InLayout, ProviderTag, Provider->Get_FieldTags());
            if (EnabledFields.IsEmpty())
            { continue; }

            auto EntryFilter = FGameplayTagQuery{};
            for (const auto& Entry : InLayout.Entries)
            {
                if (Entry.ProviderTag == ProviderTag) { EntryFilter = Entry.EntryFilter; break; }
            }

            auto Config          = FCk_DebugOverlay_ProviderConfig{};
            Config.EnabledFields = EnabledFields;
            Config.EntryFilter   = EntryFilter;

            const auto Token = Provider->Get_CompactToken(Handle, Config);
            if (NOT Token.IsEmpty())
            {
                TokenParts.Add(Token);
            }
        }

        const auto IsNearPlate = NearPlatesEnabled && Dist <= NearDist;

        const auto DebugName   = UCk_Utils_Handle_UE::Get_DebugName(Handle);
        const auto HasRealName = DebugName.IsNone() == false;

        // Far pills require behavioral tokens (identity-only pills are spam at range).
        // Near plates show when the entity has feature badges OR an explicit name.
        if (IsNearPlate)
        {
            if (Badges.IsEmpty() && NOT HasRealName)
            { continue; }
        }
        else if (TokenParts.IsEmpty())
        { continue; }

        auto ScreenPos = FVector2D{};
        if (NOT UGameplayStatics::ProjectWorldToScreen(
            InPC, InCandidates[CandIdx].WorldLocation, ScreenPos,
            /*bPlayerViewportRelative=*/false))
        { continue; }

        const auto Scale = FMath::GetMappedRangeValueClamped(
            FVector2D{ NearDist, FarDist },
            FVector2D{ 1.0f, MinScale },
            Dist);
        const auto Opacity = FMath::GetMappedRangeValueClamped(
            FVector2D{ FadeStartDist, MaxDist },
            FVector2D{ 1.0f, 0.15f },
            Dist);

        auto TagInfo      = FCk_DebugOverlay_WorldTagInfo{};
        TagInfo.ScreenPos = ScreenPos;
        TagInfo.Scale     = Scale;
        TagInfo.Opacity   = Opacity;

        if (IsNearPlate)
        {
            // Always show the entity number (very important for identification), matching
            // the focus-card "Name [id]" format. The name is truncated to MaxNameChars
            // (with an ellipsis) so long names don't balloon the plate; the [id] is never
            // truncated.
            const auto EntityNum = static_cast<uint32>(Handle.Get_Entity().Get_EntityNumber());

            auto Header = FString{};
            if (HasRealName)
            {
                auto Name = ck::DebugNameClean::Get_CleanName(DebugName.ToString());
                if (MaxNameChars > 0 && Name.Len() > MaxNameChars)
                { Name = Name.Left(MaxNameChars) + TEXT("…"); }
                Header = FString::Printf(TEXT("%s [%u]"), *Name, EntityNum);
            }
            else
            {
                Header = FString::Printf(TEXT("[%u]"), EntityNum);
            }

            TagInfo.bIsPlate = true;
            TagInfo.Header   = FText::FromString(Header);
            TagInfo.Badges   = MoveTemp(Badges);
        }
        else
        {
            TagInfo.Text = FText::FromString(FString::Join(TokenParts, TEXT(" | ")));
        }

        WorldTags.Add(MoveTemp(TagInfo));

        // Hard cap to avoid clutter in dense scenes (e.g. crowds).
        if (WorldTags.Num() >= 16)
        { break; }
    }

    // Fan out every co-located cluster (item 8): plates sharing a screen cell are splayed
    // horizontally around their shared point and each badged "[i/N]" so overlapping entities
    // can be told apart at a glance. Singletons stay anchored directly above their marker.
    // This replaces the old "stack upward" de-overlap, which drifted dense clusters far from
    // their markers (the misaligned column of pills). Non-interactive — the co-located cycle
    // key (double-Alt) remains the selector.
    {
        constexpr auto CellW       = 64.0f;
        constexpr auto CellH       = 48.0f;
        constexpr auto FanSpacingX = 92.0f; // px between fanned plates
        constexpr auto FanRiseY    = 14.0f; // px lift per column so it reads as a shallow fan

        auto CellMembers = TMap<FIntPoint, TArray<int32>>{};
        for (auto T = 0; T < WorldTags.Num(); ++T)
        {
            const auto Cell = FIntPoint{
                FMath::RoundToInt32(WorldTags[T].ScreenPos.X / CellW),
                FMath::RoundToInt32(WorldTags[T].ScreenPos.Y / CellH) };
            CellMembers.FindOrAdd(Cell).Add(T);
        }

        for (auto& Pair : CellMembers)
        {
            auto& Members = Pair.Value;
            if (Members.Num() <= 1)
            { continue; }

            // Stable left -> right order for the [i/N] indices.
            Members.Sort([&WorldTags](int32 InA, int32 InB)
            { return WorldTags[InA].ScreenPos.X < WorldTags[InB].ScreenPos.X; });

            const auto Count = Members.Num();

            auto CentroidX = 0.0f;
            auto TopY      = WorldTags[Members[0]].ScreenPos.Y;
            for (const auto T : Members)
            {
                CentroidX += WorldTags[T].ScreenPos.X;
                TopY = FMath::Min(TopY, WorldTags[T].ScreenPos.Y);
            }
            CentroidX /= static_cast<float>(Count);

            for (auto i = 0; i < Count; ++i)
            {
                const auto T    = Members[i];
                const auto Slot = static_cast<float>(i) - (Count - 1) * 0.5f;

                WorldTags[T].ScreenPos.X = CentroidX + Slot * FanSpacingX;
                WorldTags[T].ScreenPos.Y = TopY - FMath::Abs(Slot) * FanRiseY;

                const auto Badge = FString::Printf(TEXT("[%d/%d] "), i + 1, Count);
                if (WorldTags[T].bIsPlate)
                { WorldTags[T].Header = FText::FromString(Badge + WorldTags[T].Header.ToString()); }
                else
                { WorldTags[T].Text = FText::FromString(Badge + WorldTags[T].Text.ToString()); }
            }
        }
    }

    return WorldTags;
}

// ====================================================================================================================

auto
    ck_debugoverlay::
    Resolve_Layout(
        const UCk_DebugOverlay_Settings* InSettings,
        int32                            InIndex)
    -> const FCk_DebugOverlay_Layout*
{
    if (InSettings == nullptr || InSettings->Layouts.IsEmpty())
    { return nullptr; }

    const auto ClampedIdx = FMath::Clamp(InIndex, 0, InSettings->Layouts.Num() - 1);
    return &InSettings->Layouts[ClampedIdx];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck_debugoverlay::
    Get_StartingLayoutIndex(
        const UCk_DebugOverlay_Settings* InSettings)
    -> int32
{
    if (InSettings == nullptr)
    { return 0; }

    const auto& StartingTag = InSettings->StartingLayout;
    for (auto Idx = 0; Idx < InSettings->Layouts.Num(); ++Idx)
    {
        if (InSettings->Layouts[Idx].LayoutTag == StartingTag)
        { return Idx; }
    }
    return 0;
}

// ====================================================================================================================
