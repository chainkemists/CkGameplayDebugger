#pragma once
#include "GameplayTagContainer.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_DebugOverlay_HistoryKey
{
    uint32       EntityId = 0;
    FGameplayTag FieldTag;
    bool operator==(const FCk_DebugOverlay_HistoryKey& O) const { return EntityId == O.EntityId && FieldTag == O.FieldTag; }
};

FORCEINLINE uint32 GetTypeHash(const FCk_DebugOverlay_HistoryKey& K)
{ return HashCombine(::GetTypeHash(K.EntityId), GetTypeHash(K.FieldTag)); }

// --------------------------------------------------------------------------------------------------------------------

class CKENTITYDEBUGOVERLAY_API FCk_DebugOverlay_History
{
public:
    static constexpr int32 MaxDepth = 4;

    auto Observe(const FCk_DebugOverlay_HistoryKey& Key, const FString& Value, double Now) -> void;
    auto Get_Trail(const FCk_DebugOverlay_HistoryKey& Key, int32 Depth) const -> TArray<FString>;
    auto Get_LastChangedTime(const FCk_DebugOverlay_HistoryKey& Key) const -> double;
    auto Forget(uint32 EntityId) -> void;

private:
    struct FRecord { TArray<FString> Ring; double LastChanged = 0.0; }; // Ring incl. current at tail
    TMap<FCk_DebugOverlay_HistoryKey, FRecord> Records;
};

// --------------------------------------------------------------------------------------------------------------------
