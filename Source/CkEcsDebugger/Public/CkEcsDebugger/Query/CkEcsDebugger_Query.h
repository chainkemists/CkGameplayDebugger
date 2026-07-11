#pragma once

#include <CoreMinimal.h>

// --------------------------------------------------------------------------------------------------------------------
// Query grammar (redesign spec §3.5):
//
//   <term> ::= has:<feat> | is:<feat|primary|aux> | net:<auth|proxy|none> | id:<n>
//            | arch:<substr> | <fuzzy-text>
//
// Terms AND-compose. `has:x` matches own ∪ rollup features; `is:x` matches own only;
// `is:primary` / `is:aux` key off the classification model. Feature tokens PREFIX-match
// against feature-flag ids and display names, case-insensitive.
//
// Pure logic: Parse() tokenizes a query string; Matches() evaluates one entity's
// precomputed context. The tree builds contexts per rebuild; specs feed them directly.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::ecs_debugger_query
{
    enum class ETermType : uint8
    {
        Has,        // has:<feat>
        Is,         // is:<feat>
        IsPrimary,  // is:primary
        IsAux,      // is:aux
        Net,        // net:<auth|proxy|none>
        Id,         // id:<n>
        Arch,       // arch:<substr>
        Fuzzy,      // bare text
    };

    // Named to dodge the engine's global ENetMode.
    enum class EQueryNetMode : uint8
    {
        None,
        Authority,
        Proxy,
    };

    struct FQueryTerm
    {
        ETermType Type = ETermType::Fuzzy;
        FString Value;      // lower-cased for token terms, raw for fuzzy
        uint32 IdValue = 0; // parsed for Id terms
    };

    struct FParsedQuery
    {
        TArray<FQueryTerm> Terms;

        auto IsEmpty() const -> bool { return Terms.IsEmpty(); }
    };

    // Whitespace-separated terms; unknown `key:` prefixes and malformed ids fall back to
    // fuzzy text so a half-typed token never hides everything mid-keystroke.
    CKECSDEBUGGER_API auto Parse(const FString& InQuery) -> FParsedQuery;

    // Feature-token resolution: PREFIX match (case-insensitive) against every key —
    // feature-flag ids and display names both map to the same bit mask. A token matching
    // several features resolves to the OR of their bits (any of them satisfies the term).
    struct FFeatureTokenTable
    {
        struct FEntry
        {
            FString LowerKey;
            uint64 Bits = 0;
        };

        TArray<FEntry> Entries;

        auto Add(const FString& InKey, uint64 InBits) -> void;
        auto Resolve(const FString& InLowerToken) const -> uint64;
    };

    // Everything Matches() needs, precomputed by the caller once per entity per rebuild.
    struct FEntityQueryContext
    {
        uint64 OwnBits = 0;
        uint64 RollupBits = 0;         // OR of rolled-up internal-feature bits
        bool IsInternal = false;       // classification (spec §3.1)
        EQueryNetMode NetMode = EQueryNetMode::None;
        uint32 EntityId = 0;
        FString ArchetypeName;         // registered or inferred; lower-cased by the caller
        FString DisplayName;           // cleaned name, used by fuzzy terms
    };

    CKECSDEBUGGER_API auto Matches(
        const FEntityQueryContext& InContext,
        const FParsedQuery& InQuery,
        const FFeatureTokenTable& InTokenTable) -> bool;

    // Inferred archetype key for UNREGISTERED entities (spec §3.3 fallback): cleaned
    // base name (instance suffixes stripped: trailing digits / "_UE_7" style tails)
    // + the own-feature signature, so same-named entities with different features
    // group apart. Registered archetype names always take precedence over this.
    CKECSDEBUGGER_API auto Get_InferredArchetypeKey(
        const FString& InCleanName,
        uint64 InOwnBits) -> FString;

    // ---- arch-token helpers (multi-select archetype cards) --------------------------

    // Lowercased values of every arch: term in a filter string — cards derive their
    // toggled state from this.
    CKECSDEBUGGER_API auto Get_ArchTokens(const FString& InFilter) -> TSet<FString>;

    // Returns InFilter with `arch:<InToken>` added/removed (quoted when the token has
    // whitespace). Removal is word-boundary safe (arch:Timer never clips arch:TimerX)
    // and preserves every other term as typed.
    CKECSDEBUGGER_API auto Toggle_ArchToken(
        const FString& InFilter,
        const FString& InToken,
        bool InEnable) -> FString;
}
