#include "CkDebuggerModel_InspectorFilter.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    /**
     * Curated badge color map for well-known inspectors. Reuses semantic colors that already
     * live in FCkDebuggerStyle so the palette stays consistent with the rest of the debugger.
     * Inspectors not listed here fall back to a deterministic hash into FallbackPalette.
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

    /** Deterministic fallback palette — six visually distinct colors. */
    static auto Get_FallbackBadgeColor(
        FName InID) -> FLinearColor
    {
        static const auto Palette = TArray<FLinearColor>{
            FLinearColor(0.85f, 0.45f, 0.20f),  // Orange
            FLinearColor(0.30f, 0.75f, 0.40f),  // Green
            FLinearColor(0.65f, 0.45f, 0.80f),  // Purple
            FLinearColor(0.85f, 0.75f, 0.25f),  // Yellow
            FLinearColor(0.40f, 0.80f, 0.80f),  // Cyan
            FLinearColor(0.85f, 0.50f, 0.65f),  // Pink
        };

        const auto Hash = GetTypeHash(InID);
        return Palette[Hash % Palette.Num()];
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
        _AllEntries.Add(FInspectorEntry{
            Entry.ID,
            Entry.DisplayName,
            Resolve_BadgeColor(Entry.ID)
        });
    }

    LoadExclusionsFromSettings();
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

    auto& Registry = FCkDebuggerInspectorRegistry::Get();

    if (_MatchMode == ECk_InspectorFilter_MatchMode::All)
    {
        for (const auto& ID : _SelectedIDs)
        {
            if (NOT Registry.Test(ID, InEntity))
            { return false; }
        }
        return true;
    }

    // Any (OR)
    for (const auto& ID : _SelectedIDs)
    {
        if (Registry.Test(ID, InEntity))
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
    auto& Registry = FCkDebuggerInspectorRegistry::Get();
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity).ToString();

    for (const auto& ID : _ExcludedIDs)
    {
        const auto Token = ID.ToString();
        if (Token.IsEmpty())
        { continue; }

        for (const auto& Entry : _AllEntries)
        {
            if (Entry.ID.ToString().Contains(Token) && Registry.Test(Entry.ID, InEntity))
            { return true; }
        }

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
    { _ExcludedIDs = New; }

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
    DoBroadcast() -> void
{
    OnFilterChanged.Broadcast();
}
