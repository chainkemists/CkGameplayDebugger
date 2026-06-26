#include "CkDebugOverlay_Provider_Timer.h"

#include "NativeGameplayTags.h"
#include "CkCore/Validation/CkIsValid.h"

// Timer utils — mirroring CkInspector_Timer.cpp includes
#include "CkTimer/CkTimer_Utils.h"

#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

// --------------------------------------------------------------------------------------------------------------------
// Native tags
// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Timer,
    "Ck.OnScreenDebugger.Provider.Timer")

// A single "catch-all" field tag for the timer count summary row.
UE_DEFINE_GAMEPLAY_TAG(TAG_Ck_OnScreenDebugger_Provider_Timer_Value,
    "Ck.OnScreenDebugger.Provider.Timer.Value")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    FGameplayTag ProviderTag()    { return TAG_Ck_OnScreenDebugger_Provider_Timer; }
    FGameplayTag FieldTag_Value() { return TAG_Ck_OnScreenDebugger_Provider_Timer_Value; }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Timer::Get_ProviderTag() const -> FGameplayTag
{
    return ProviderTag();
}

auto FCk_DebugOverlay_Provider_Timer::Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc>
{
    return {
        { FieldTag_Value(), true },
    };
}

// --------------------------------------------------------------------------------------------------------------------
// CanProvide — mirrors FCkInspector_Timer::CanInspect, but uses Has_Any so the provider
// fires for entities that *own* one or more timers (not just bare timer entities).
// Has_Any checks FFragment_RecordOfTimers on the entity.
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Timer::CanProvide(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity)) { return false; }
    return UCk_Utils_Timer_UE::Has_Any(Entity);
}

// --------------------------------------------------------------------------------------------------------------------
// Collect
//
// Iterates all owned timers via ForEach_Timer and emits one summary row:
//   "[N timers]  Name State"  — listing up to 3 timers inline, then "+N more".
//
// BATCH-VERIFY:
//   UCk_Utils_Timer_UE::Has_Any(Entity)                           — confirmed from CkTimer_Utils.h line 85
//   UCk_Utils_Timer_UE::ForEach_Timer(Entity, TFunction<...>)    — confirmed from CkTimer_Utils.h line 170
//   UCk_Utils_Timer_UE::Get_Name(FCk_Handle_Timer)               — confirmed from CkTimer_Utils.h line 127
//   UCk_Utils_Timer_UE::Get_CurrentState(FCk_Handle_Timer)       — confirmed from CkTimer_Utils.h line 134
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Timer::Collect(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& Cfg,
    FCk_DebugOverlay_Section&              Out) -> void
{
    Out.ProviderTag  = Get_ProviderTag();
    Out.SortPriority = Get_SortPriority();

    if (NOT Cfg.EnabledFields.HasTagExact(FieldTag_Value()))
    { return; }

    // Gather up to 3 timer names + state for inline display.
    constexpr int32 MaxInline = 3;
    TArray<FString> TimerStrings;
    int32 TotalCount = 0;

    UCk_Utils_Timer_UE::ForEach_Timer(Entity,
        [&TimerStrings, &TotalCount](FCk_Handle_Timer InTimer)
        {
            ++TotalCount;
            if (TimerStrings.Num() < MaxInline)
            {
                const auto NameTag = UCk_Utils_Timer_UE::Get_Name(InTimer);
                const auto Name    = NameTag.IsValid()
                    ? NameTag.GetTagName().ToString()
                    : FString(TEXT("Unnamed"));
                const auto State   = UCk_Utils_Timer_UE::Get_CurrentState(InTimer);
                TimerStrings.Add(FString::Printf(TEXT("%s(%s)"), *Name,
                    *StaticEnum<ECk_Timer_State>()->GetNameStringByValue(static_cast<int64>(State))));
            }
        });

    if (TotalCount == 0) { return; }

    FString DisplayStr = FString::Join(TimerStrings, TEXT(", "));
    if (TotalCount > MaxInline)
    {
        DisplayStr += FString::Printf(TEXT(", +%d more"), TotalCount - MaxInline);
    }

    FCk_DebugOverlay_Row Row;
    Row.FieldTag = FieldTag_Value();
    Row.Value    = FText::FromString(FString::Printf(TEXT("[%d] %s"), TotalCount, *DisplayStr));
    Row.Severity = ECk_DebugOverlay_Severity::Normal;
    Out.Rows.Add(MoveTemp(Row));
}

// --------------------------------------------------------------------------------------------------------------------
// CompactToken
// --------------------------------------------------------------------------------------------------------------------

auto FCk_DebugOverlay_Provider_Timer::Get_CompactToken(
    const FCk_Handle&                      Entity,
    const FCk_DebugOverlay_ProviderConfig& /*Cfg*/) const -> FString
{
    if (ck::Is_NOT_Valid(Entity)) { return {}; }

    int32 Count = 0;
    UCk_Utils_Timer_UE::ForEach_Timer(Entity,
        [&Count](FCk_Handle_Timer) { ++Count; });

    if (Count == 0) { return {}; }
    return FString::Printf(TEXT("Tmr:%d"), Count);
}

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUG_OVERLAY_PROVIDER(FCk_DebugOverlay_Provider_Timer)

// --------------------------------------------------------------------------------------------------------------------
