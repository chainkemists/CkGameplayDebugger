#include "CkDebuggerModel_InspectorFilter.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

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
            Map.Add(TEXT("FCkInspector_Transform"),         FCkDebuggerStyle::Color_Transform);
            Map.Add(TEXT("FCkInspector_Network"),           FCkDebuggerStyle::Color_Network);
            Map.Add(TEXT("FCkInspector_Relationships"),     FCkDebuggerStyle::Color_Relationship);
            Map.Add(TEXT("FCkInspector_TagSet"),            FCkDebuggerStyle::Color_Value_Tag);
            Map.Add(TEXT("FCkInspector_Inventories"),       FCkDebuggerStyle::Color_Value_Object);
            Map.Add(TEXT("FCkInspector_AnimPlans"),         FCkDebuggerStyle::Color_Value_Math);
            Map.Add(TEXT("FCkInspector_FloatAttributes"),   FCkDebuggerStyle::Color_Attribute);
            Map.Add(TEXT("FCkInspector_IntegerAttributes"), FCkDebuggerStyle::Color_Attribute);
            Map.Add(TEXT("FCkInspector_ByteAttributes"),    FCkDebuggerStyle::Color_Attribute);
            Map.Add(TEXT("FCkInspector_Variables"),         FCkDebuggerStyle::Color_Reference);
            Map.Add(TEXT("FCkInspector_Probes"),            FCkDebuggerStyle::Color_PickMarker_Default);
            Map.Add(TEXT("FCkInspector_ProbeTraces"),       FCkDebuggerStyle::Color_PickMarker_Default);
            Map.Add(TEXT("FCkInspector_EntityCollections"), FCkDebuggerStyle::Color_Value_Enum);
            Map.Add(TEXT("FCkInspector_EntityInfo"),        FCkDebuggerStyle::Color_Entity_ID);
            Map.Add(TEXT("FCkInspector_DynamicFragments"),  FCkDebuggerStyle::Color_Value_String);
            Map.Add(TEXT("FCkInspector_IsmProxy"),          FCkDebuggerStyle::Color_Value_Numeric);
            Map.Add(TEXT("FCkInspector_Objective"),         FCkDebuggerStyle::Color_State_Config);
            Map.Add(TEXT("FCkInspector_ObjectiveOwner"),    FCkDebuggerStyle::Color_State_Config);
            Map.Add(TEXT("FCkInspector_InteractionResolver"), FCkDebuggerStyle::Color_Value_Bool_True);
            Map.Add(TEXT("FCkInspector_InteractTarget"),    FCkDebuggerStyle::Color_Value_Bool_True);
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
    DoBroadcast() -> void
{
    OnFilterChanged.Broadcast();
}
