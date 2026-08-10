#include "CkDebuggerModel_InspectorFilter.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    /**
     * Curated badge color map for well-known inspectors. Reuses semantic colors that already
     * live in FCkDebuggerStyle so the palette stays consistent with the rest of the debugger.
     * Inspectors not listed here fall back to the shared categorical ramp.
     */
    static auto Get_CuratedBadgeColor(
        FName InID) -> TOptional<FLinearColor>
    {
        // Lazily build a map on first call. Static initialization order with FCkDebuggerStyle's
        // FLinearColor constants is fine because these are compile-time constexpr in the .cpp.
        static const auto Curated = []() -> TMap<FName, FLinearColor>
        {
            auto Map = TMap<FName, FLinearColor>{};
            Map.Add(TEXT("FCkInspector_Transform"),         CkStyle::Transform());
            Map.Add(TEXT("FCkInspector_Network"),           CkStyle::Network());
            Map.Add(TEXT("FCkInspector_Relationships"),     CkStyle::Relationship());
            Map.Add(TEXT("FCkInspector_TagSet"),            CkStyle::Value_Tag());
            Map.Add(TEXT("FCkInspector_Inventories"),       CkStyle::Value_Object());
            Map.Add(TEXT("FCkInspector_AnimPlans"),         CkStyle::Value_Math());
            Map.Add(TEXT("FCkInspector_FloatAttributes"),   CkStyle::Attribute());
            Map.Add(TEXT("FCkInspector_IntegerAttributes"), CkStyle::Attribute());
            Map.Add(TEXT("FCkInspector_ByteAttributes"),    CkStyle::Attribute());
            Map.Add(TEXT("FCkInspector_Variables"),         CkStyle::Reference());
            Map.Add(TEXT("FCkInspector_Probes"),            CkStyle::PickMarker_Default());
            Map.Add(TEXT("FCkInspector_ProbeTraces"),       CkStyle::PickMarker_Default());
            Map.Add(TEXT("FCkInspector_EntityCollections"), CkStyle::Value_Enum());
            Map.Add(TEXT("FCkInspector_EntityInfo"),        CkStyle::EntityId());
            Map.Add(TEXT("FCkInspector_DynamicFragments"),  CkStyle::Value_String());
            Map.Add(TEXT("FCkInspector_IsmProxy"),          CkStyle::Value_Numeric());
            Map.Add(TEXT("FCkInspector_Objective"),         CkStyle::State_Config());
            Map.Add(TEXT("FCkInspector_ObjectiveOwner"),    CkStyle::State_Config());
            Map.Add(TEXT("FCkInspector_InteractionResolver"), CkStyle::Value_Bool_True());
            Map.Add(TEXT("FCkInspector_InteractTarget"),    CkStyle::Value_Bool_True());
            Map.Add(TEXT("FCkInspector_Timer"),             CkStyle::Value_Numeric());
            Map.Add(TEXT("FCkInspector_ActorRelay"),        CkStyle::Value_Object());
            return Map;
        }();

        if (const auto* Found = Curated.Find(InID))
        { return *Found; }

        return TOptional<FLinearColor>{};
    }

    /** Deterministic fallback bucket color for an inspector with no curated role. */
    static auto Get_FallbackBadgeColor(
        FName InID) -> FLinearColor
    {
        return ck::debug_axes::Get_CategoricalColor(InID);
    }

    static auto Resolve_BadgeColor(
        FName InID) -> FLinearColor
    {
        if (const auto Curated = Get_CuratedBadgeColor(InID); Curated.IsSet())
        { return Curated.GetValue(); }

        return Get_FallbackBadgeColor(InID);
    }
}

// =====================================================================================================================

FCkDebuggerModel_InspectorFilter::FCkDebuggerModel_InspectorFilter()
{
    const auto Metadata = FCkDebuggerInspectorRegistry::Get().Get_AllMetadata();

    _AllEntries.Reserve(Metadata.Num());
    for (const auto& Entry : Metadata)
    {
        // Flag ids are registered at module startup (before any model exists), so the
        // bit index is stable for the model's lifetime. An id that never registered
        // resolves to INDEX_NONE and keeps the instantiation path.
        const auto FeatureFlagBit = Entry.FeatureFlagId.IsNone()
            ? int32{INDEX_NONE}
            : ck::debug_feature_flags::Get_BitIndex(Entry.FeatureFlagId);

        _AllEntries.Add(FInspectorEntry{
            Entry.ID,
            Entry.DisplayName,
            Resolve_BadgeColor(Entry.ID),
            Entry.FeatureFlagId,
            FeatureFlagBit
        });
    }

    LoadExclusionsFromSettings();
    DoRebuildExclusionMatches();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Get_AllEntries() const -> const TArray<FInspectorEntry>&
{
    return _AllEntries;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Find_Entry(
        FName InID) const -> const FInspectorEntry*
{
    return _AllEntries.FindByPredicate([&InID](const FInspectorEntry& InEntry)
    {
        return InEntry.ID == InID;
    });
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Get_SelectedIDs() const -> const TSet<FName>&
{
    return _SelectedIDs;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Get_HasActiveFilter() const -> bool
{
    return _SelectedIDs.Num() > 0;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Get_NumSelected() const -> int32
{
    return _SelectedIDs.Num();
}

auto
    FCkDebuggerModel_InspectorFilter::
    Get_IsSelected(
        FName InID) const -> bool
{
    return _SelectedIDs.Contains(InID);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebuggerModel_InspectorFilter::
    Toggle_Selection(
        FName InID) -> void
{
    if (_SelectedIDs.Contains(InID))
    {
        _SelectedIDs.Remove(InID);
    }
    else
    {
        _SelectedIDs.Add(InID);
    }

    DoBroadcast();
}

auto
    FCkDebuggerModel_InspectorFilter::
    Set_Selection(
        FName InID,
        bool  InSelected) -> void
{
    const auto AlreadySelected = _SelectedIDs.Contains(InID);
    if (AlreadySelected == InSelected)
    { return; }

    if (InSelected)
    {
        _SelectedIDs.Add(InID);
    }
    else
    {
        _SelectedIDs.Remove(InID);
    }

    DoBroadcast();
}

auto
    FCkDebuggerModel_InspectorFilter::
    Clear_Selection() -> void
{
    if (_SelectedIDs.IsEmpty())
    { return; }

    _SelectedIDs.Reset();
    DoBroadcast();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Get_MatchMode() const -> ECk_InspectorFilter_MatchMode
{
    return _MatchMode;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Set_MatchMode(
        ECk_InspectorFilter_MatchMode InMode) -> void
{
    if (_MatchMode == InMode)
    { return; }

    _MatchMode = InMode;
    DoBroadcast();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Get_BadgeStyle() const -> ECk_InspectorFilter_BadgeStyle
{
    return _BadgeStyle;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Set_BadgeStyle(
        ECk_InspectorFilter_BadgeStyle InStyle) -> void
{
    if (_BadgeStyle == InStyle)
    { return; }

    _BadgeStyle = InStyle;
    DoBroadcast();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Test_Entity(
        const FCk_Handle& InEntity) const -> bool
{
    // Empty selection = filter off → everything passes.
    if (_SelectedIDs.IsEmpty())
    { return true; }

    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    const auto Flags = DoGet_FlagsIfEnabled(InEntity);
    auto& Registry = FCkDebuggerInspectorRegistry::Get();

    // Parity-wired inspectors answer from the flag cache — O(1) per entity — instead of
    // instantiating a fresh inspector per Test (redesign spec §5 hot spot).
    const auto TestOne = [&](FName InID) -> bool
    {
        if (Flags.IsSet())
        {
            if (const auto* Entry = Find_Entry(InID);
                Entry != nullptr && Entry->FeatureFlagBit != INDEX_NONE)
            { return (Flags.GetValue() & (uint64{1} << Entry->FeatureFlagBit)) != 0; }
        }

        return Registry.Test(InID, InEntity);
    };

    if (_MatchMode == ECk_InspectorFilter_MatchMode::All)
    {
        for (const auto& ID : _SelectedIDs)
        {
            if (NOT TestOne(ID))
            { return false; }
        }
        return true;
    }

    // Any (OR)
    for (const auto& ID : _SelectedIDs)
    {
        if (TestOne(ID))
        { return true; }
    }
    return false;
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Test_Entity_IsExcluded(
        const FCk_Handle& InEntity) const -> bool
{
    if (_ExcludedIDs.IsEmpty())
    { return false; }

    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    // Each excluded entry is a SUBSTRING token, matched two ways:
    //   1. Against inspector IDs ("Transform" matches "FCkInspector_Transform")
    //      — entities that inspector can inspect are hidden.
    //   2. Against the entity's debug name ("Ck_CueRelay" matches
    //      "Ck_CueRelay_UE_3") — name-pattern exclusion, which is what users
    //      reach for first. Exact inspector-ID entries keep working via (1).
    // Token → entry matching is precomputed in DoRebuildExclusionMatches; only the
    // per-entity tests run here.
    const auto Flags = DoGet_FlagsIfEnabled(InEntity);
    auto& Registry = FCkDebuggerInspectorRegistry::Get();

    for (const auto EntryIndex : _ExclusionMatchedEntries)
    {
        const auto& Entry = _AllEntries[EntryIndex];

        if (Flags.IsSet() && Entry.FeatureFlagBit != INDEX_NONE)
        {
            if ((Flags.GetValue() & (uint64{1} << Entry.FeatureFlagBit)) != 0)
            { return true; }

            continue;
        }

        if (Registry.Test(Entry.ID, InEntity))
        { return true; }
    }

    if (_ExclusionNameTokens.IsEmpty())
    { return false; }

    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity).ToString();

    for (const auto& Token : _ExclusionNameTokens)
    {
        if (DebugName.Contains(Token))
        { return true; }
    }
    return false;
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    Get_ExcludedIDs() const -> const TSet<FName>&
{
    return _ExcludedIDs;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Get_HasActiveExclusion() const -> bool
{
    return _ExcludedIDs.Num() > 0;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Get_IsExcluded(
        FName InID) const -> bool
{
    return _ExcludedIDs.Contains(InID);
}

auto
    FCkDebuggerModel_InspectorFilter::
    Toggle_Exclusion(
        FName InID) -> void
{
    if (_ExcludedIDs.Contains(InID))
    {
        _ExcludedIDs.Remove(InID);
    }
    else
    {
        _ExcludedIDs.Add(InID);
    }

    DoRebuildExclusionMatches();
    SaveExclusionsToSettings();
    DoBroadcast();
}

auto
    FCkDebuggerModel_InspectorFilter::
    Set_Exclusion(
        FName InID,
        bool  InExcluded) -> void
{
    const auto AlreadyExcluded = _ExcludedIDs.Contains(InID);
    if (AlreadyExcluded == InExcluded)
    { return; }

    if (InExcluded)
    {
        _ExcludedIDs.Add(InID);
    }
    else
    {
        _ExcludedIDs.Remove(InID);
    }

    DoRebuildExclusionMatches();
    SaveExclusionsToSettings();
    DoBroadcast();
}

auto
    FCkDebuggerModel_InspectorFilter::
    Clear_Exclusions() -> void
{
    if (_ExcludedIDs.IsEmpty())
    { return; }

    _ExcludedIDs.Reset();
    DoRebuildExclusionMatches();
    SaveExclusionsToSettings();
    DoBroadcast();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    LoadExclusionsFromSettings() -> void
{
    const auto* Settings = UCkEcsDebuggerSettings::Get();
    if (Settings == nullptr)
    { return; }

    _ExcludedIDs = Settings->DefaultExcludedInspectorIDs;
}

auto
    FCkDebuggerModel_InspectorFilter::
    Sync_ExclusionsFromSettings() -> bool
{
    const auto* Settings = UCkEcsDebuggerSettings::Get();
    if (Settings == nullptr)
    { return false; }

    const auto& New = Settings->DefaultExcludedInspectorIDs;

    auto Changed = New.Num() != _ExcludedIDs.Num();
    if (NOT Changed)
    {
        for (const auto& ID : New)
        {
            if (NOT _ExcludedIDs.Contains(ID))
            { Changed = true; break; }
        }
    }

    if (Changed)
    {
        _ExcludedIDs = New;
        DoRebuildExclusionMatches();
    }

    return Changed;
}

auto
    FCkDebuggerModel_InspectorFilter::
    SaveExclusionsToSettings() const -> void
{
    auto* Settings = GetMutableDefault<UCkEcsDebuggerSettings>();
    if (Settings == nullptr)
    { return; }

    Settings->DefaultExcludedInspectorIDs = _ExcludedIDs;
    Settings->SaveConfig();
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    DoRebuildExclusionMatches() -> void
{
    _ExclusionMatchedEntries.Reset();
    _ExclusionNameTokens.Reset();

    for (const auto& ID : _ExcludedIDs)
    {
        const auto Token = ID.ToString();
        if (Token.IsEmpty())
        { continue; }

        _ExclusionNameTokens.Add(Token);

        for (auto EntryIndex = 0; EntryIndex < _AllEntries.Num(); ++EntryIndex)
        {
            if (_AllEntries[EntryIndex].ID.ToString().Contains(Token))
            { _ExclusionMatchedEntries.AddUnique(EntryIndex); }
        }
    }
}

auto
    FCkDebuggerModel_InspectorFilter::
    DoGet_FlagsIfEnabled(
        const FCk_Handle& InEntity) -> TOptional<uint64>
{
    const auto& RegistryView = InEntity.Get_RegistryView();

    if (NOT ck::debug_feature_flags::Get_IsEnabled(RegistryView))
    { return {}; }

    return ck::debug_feature_flags::Get_Flags(RegistryView, InEntity.Get_Entity());
}

// =====================================================================================================================

auto
    FCkDebuggerModel_InspectorFilter::
    DoBroadcast() -> void
{
    OnFilterChanged.Broadcast();
}
