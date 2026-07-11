#include "CkEcsDebugger_Query.h"

#include "CkCore/String/CkFuzzyMatch_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_ecs_debugger_query_impl
{
    // Whitespace splitting that honors double quotes, so multi-word values survive as
    // one token: arch:"UnrealComponent: BackWall". Quotes delimit and are never emitted;
    // an unbalanced quote (mid-typing) swallows the rest into one token — harmless.
    static auto TokenizeRespectingQuotes(const FString& InQuery) -> TArray<FString>
    {
        auto Tokens = TArray<FString>{};
        auto Current = FString{};
        auto InQuotes = false;

        for (const auto Char : InQuery)
        {
            if (Char == TEXT('"'))
            {
                InQuotes = NOT InQuotes;
                continue;
            }

            if (FChar::IsWhitespace(Char) && NOT InQuotes)
            {
                if (NOT Current.IsEmpty())
                {
                    Tokens.Add(Current);
                    Current.Reset();
                }
                continue;
            }

            Current.AppendChar(Char);
        }

        if (NOT Current.IsEmpty())
        { Tokens.Add(Current); }

        return Tokens;
    }

    // Incomplete tokens ("has:", "net:") are DROPPED rather than evaluated — a
    // half-typed term must never blank the tree mid-keystroke.
    static auto ParseOne(const FString& InToken) -> TOptional<ck::ecs_debugger_query::FQueryTerm>
    {
        using namespace ck::ecs_debugger_query;

        auto ColonIndex = int32{INDEX_NONE};
        if (NOT InToken.FindChar(TEXT(':'), ColonIndex))
        { return FQueryTerm{ ETermType::Fuzzy, InToken }; }

        const auto Key = InToken.Left(ColonIndex).ToLower();
        const auto Value = InToken.Mid(ColonIndex + 1).ToLower();

        if (Key == TEXT("has"))
        {
            if (Value.IsEmpty())
            { return {}; }
            return FQueryTerm{ ETermType::Has, Value };
        }

        if (Key == TEXT("is"))
        {
            if (Value.IsEmpty())
            { return {}; }
            if (Value == TEXT("primary"))
            { return FQueryTerm{ ETermType::IsPrimary, Value }; }
            if (Value == TEXT("aux"))
            { return FQueryTerm{ ETermType::IsAux, Value }; }
            return FQueryTerm{ ETermType::Is, Value };
        }

        if (Key == TEXT("net"))
        {
            // Prefix-forgiving: "net:a" already reads as authority while typing.
            if (Value.IsEmpty())
            { return {}; }
            return FQueryTerm{ ETermType::Net, Value };
        }

        if (Key == TEXT("id"))
        {
            if (Value.IsEmpty())
            { return {}; }

            if (Value.IsNumeric())
            {
                auto Term = FQueryTerm{ ETermType::Id, Value };
                Term.IdValue = static_cast<uint32>(FCString::Strtoui64(*Value, nullptr, 10));
                return Term;
            }

            // Malformed id → fuzzy so the raw text still searches names.
            return FQueryTerm{ ETermType::Fuzzy, InToken };
        }

        if (Key == TEXT("arch"))
        {
            if (Value.IsEmpty())
            { return {}; }
            return FQueryTerm{ ETermType::Arch, Value };
        }

        // Unknown key → the whole token is fuzzy text.
        return FQueryTerm{ ETermType::Fuzzy, InToken };
    }

    static auto MatchesNet(const FString& InLowerValue, ck::ecs_debugger_query::EQueryNetMode InMode) -> bool
    {
        using namespace ck::ecs_debugger_query;

        const auto ModeString = [InMode]() -> FString
        {
            switch (InMode)
            {
                case EQueryNetMode::Authority: return TEXT("authority");
                case EQueryNetMode::Proxy:     return TEXT("proxy");
                default:                       return TEXT("none");
            }
        }();

        // "auth", "a", "prox", "n" all resolve while typing.
        return ModeString.StartsWith(InLowerValue);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_query::
    Parse(
        const FString& InQuery)
    -> FParsedQuery
{
    auto Result = FParsedQuery{};

    const auto Tokens = ck_ecs_debugger_query_impl::TokenizeRespectingQuotes(InQuery);

    for (const auto& Token : Tokens)
    {
        if (const auto Term = ck_ecs_debugger_query_impl::ParseOne(Token); Term.IsSet())
        { Result.Terms.Add(Term.GetValue()); }
    }

    return Result;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_query::FFeatureTokenTable::
    Add(
        const FString& InKey,
        uint64 InBits)
    -> void
{
    Entries.Add(FEntry{ InKey.ToLower(), InBits });
}

auto
    ck::ecs_debugger_query::FFeatureTokenTable::
    Resolve(
        const FString& InLowerToken)
    const -> uint64
{
    auto Bits = uint64{0};

    for (const auto& Entry : Entries)
    {
        if (Entry.LowerKey.StartsWith(InLowerToken))
        { Bits |= Entry.Bits; }
    }

    return Bits;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_query::
    Matches(
        const FEntityQueryContext& InContext,
        const FParsedQuery& InQuery,
        const FFeatureTokenTable& InTokenTable)
    -> bool
{
    // Repeated arch: terms OR-compose (an entity has exactly ONE archetype, so ANDing
    // them is always empty — OR is the only useful multi-card semantics); every other
    // term type ANDs as usual, including against the arch group as a whole.
    auto ArchTermSeen = false;
    auto ArchAnyMatched = false;

    for (const auto& Term : InQuery.Terms)
    {
        if (Term.Type == ETermType::Arch)
        {
            ArchTermSeen = true;
            ArchAnyMatched = ArchAnyMatched || InContext.ArchetypeName.Contains(Term.Value);
            continue;
        }

        const auto TermMatches = [&]() -> bool
        {
            switch (Term.Type)
            {
                case ETermType::Has:
                {
                    const auto Bits = InTokenTable.Resolve(Term.Value);
                    return Bits != 0 && ((InContext.OwnBits | InContext.RollupBits) & Bits) != 0;
                }
                case ETermType::Is:
                {
                    const auto Bits = InTokenTable.Resolve(Term.Value);
                    return Bits != 0 && (InContext.OwnBits & Bits) != 0;
                }
                case ETermType::IsPrimary:
                { return NOT InContext.IsInternal; }
                case ETermType::IsAux:
                { return InContext.IsInternal; }
                case ETermType::Net:
                { return ck_ecs_debugger_query_impl::MatchesNet(Term.Value, InContext.NetMode); }
                case ETermType::Id:
                { return InContext.EntityId == Term.IdValue; }
                case ETermType::Fuzzy:
                { return ck::fuzzy::Match(Term.Value, InContext.DisplayName, {}).Get_IsMatch(); }
                default:
                { return true; }
            }
        }();

        if (NOT TermMatches)
        { return false; }
    }

    return NOT ArchTermSeen || ArchAnyMatched;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_query::
    Get_ArchTokens(
        const FString& InFilter)
    -> TSet<FString>
{
    auto Result = TSet<FString>{};

    for (const auto& Term : Parse(InFilter).Terms)
    {
        if (Term.Type == ETermType::Arch)
        { Result.Add(Term.Value); }   // Parse lowercases values
    }

    return Result;
}

auto
    ck::ecs_debugger_query::
    Toggle_ArchToken(
        const FString& InFilter,
        const FString& InToken,
        bool InEnable)
    -> FString
{
    const auto NeedsQuotes = InToken.Contains(TEXT(" "));
    const auto QuotedForm = FString::Printf(TEXT("arch:\"%s\""), *InToken);
    const auto BareForm = FString::Printf(TEXT("arch:%s"), *InToken);

    auto NewFilter = InFilter;

    // Remove an existing occurrence — quoted form is unambiguous; the bare form only
    // when it ends at a word boundary.
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

    return NewFilter;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    ck::ecs_debugger_query::
    Get_InferredArchetypeKey(
        const FString& InCleanName,
        uint64 InOwnBits)
    -> FString
{
    // Strip instance suffixes: trailing digit runs and separators, repeatedly, so
    // "Enemy_12" → "Enemy" and "Ck_CueRelay_UE_3" → "Ck_CueRelay_UE".
    auto Base = InCleanName;
    while (Base.Len() > 0)
    {
        const auto Last = Base[Base.Len() - 1];
        if (FChar::IsDigit(Last) || Last == TEXT('_') || Last == TEXT('-') || Last == TEXT(' '))
        { Base.LeftChopInline(1, EAllowShrinking::No); }
        else
        { break; }
    }

    if (Base.IsEmpty())
    { Base = InCleanName; }

    // '#' separates display base from the feature signature — same-named entities with
    // different features group apart. Consumers show the part left of '#'.
    return FString::Printf(TEXT("%s#%llx"), *Base, InOwnBits);
}
