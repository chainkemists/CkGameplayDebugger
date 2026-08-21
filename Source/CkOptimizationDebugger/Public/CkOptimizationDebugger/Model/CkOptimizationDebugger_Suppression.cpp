#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Suppression.h"

#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity.
namespace ck_optimization_debugger_suppression_impl
{
    const auto k_FieldSeparator = FString{TEXT(";")};

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ScopeToken(
            ECkOptimizationDebugger_SuppressionScope InScope)
        -> FString
    {
        switch (InScope)
        {
            case ECkOptimizationDebugger_SuppressionScope::Finding: return FString{TEXT("Finding")};
            case ECkOptimizationDebugger_SuppressionScope::Asset:   return FString{TEXT("Asset")};
            case ECkOptimizationDebugger_SuppressionScope::Folder:  return FString{TEXT("Folder")};
            case ECkOptimizationDebugger_SuppressionScope::Check:   return FString{TEXT("Check")};
            default: break;
        }

        return FString{TEXT("Finding")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_Scope(
            const FString& InToken,
            ECkOptimizationDebugger_SuppressionScope& OutScope)
        -> bool
    {
        if (InToken == TEXT("Finding")) { OutScope = ECkOptimizationDebugger_SuppressionScope::Finding; return true; }
        if (InToken == TEXT("Asset"))   { OutScope = ECkOptimizationDebugger_SuppressionScope::Asset;   return true; }
        if (InToken == TEXT("Folder"))  { OutScope = ECkOptimizationDebugger_SuppressionScope::Folder;  return true; }
        if (InToken == TEXT("Check"))   { OutScope = ECkOptimizationDebugger_SuppressionScope::Check;   return true; }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Splits one `Key=Value` field off the front, leaving the remainder. Returns false when the line does not
     *  start with the expected key, which is what makes a malformed line droppable rather than half-read. */
    auto
        TrySplit_Field(
            const FString& InRemainder,
            const FString& InKey,
            FString& OutValue,
            FString& OutRest)
        -> bool
    {
        const auto Prefix = InKey + TEXT("=");

        if (NOT InRemainder.StartsWith(Prefix, ESearchCase::CaseSensitive))
        { return false; }

        const auto Body = InRemainder.RightChop(Prefix.Len());

        auto Left = FString{};
        auto Right = FString{};

        if (Body.Split(k_FieldSeparator, &Left, &Right))
        {
            OutValue = Left;
            OutRest = Right;

            return true;
        }

        OutValue = Body;
        OutRest = FString{};

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_suppression
{
    using namespace ck_optimization_debugger_suppression_impl;

    auto
        Matches(
            const FCkOptimizationDebugger_Suppression& InSuppression,
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> bool
    {
        switch (InSuppression.Scope)
        {
            case ECkOptimizationDebugger_SuppressionScope::Finding:
            {
                // The stable key already carries the check id, so no second narrowing is needed or accepted.
                return NOT InSuppression.Pattern.IsEmpty() && InSuppression.Pattern == InFinding.StableKey;
            }
            case ECkOptimizationDebugger_SuppressionScope::Check:
            {
                return NOT InSuppression.CheckId.IsNone() && InSuppression.CheckId == InFinding.CheckId;
            }
            case ECkOptimizationDebugger_SuppressionScope::Asset:
            {
                if (InSuppression.Pattern.IsEmpty())
                { return false; }

                // A check id NARROWS an asset rule rather than being required by it: "everything about this asset"
                // and "this one complaint about this asset" are both things a reader means, and `None` is how the
                // record spells the first.
                if (NOT InSuppression.CheckId.IsNone() && InSuppression.CheckId != InFinding.CheckId)
                { return false; }

                return InFinding.Target.Path.ToString() == InSuppression.Pattern;
            }
            case ECkOptimizationDebugger_SuppressionScope::Folder:
            {
                if (InSuppression.Pattern.IsEmpty())
                { return false; }

                if (NOT InSuppression.CheckId.IsNone() && InSuppression.CheckId != InFinding.CheckId)
                { return false; }

                const auto Path = InFinding.Target.Path.ToString();

                // A pathless finding — every `ProjectSettings` one — is covered by NO folder rule. Same reading as
                // the path-scope filter: a reader narrowing to a content folder is not talking about the renderer.
                if (Path.IsEmpty())
                { return false; }

                return Path.StartsWith(InSuppression.Pattern, ESearchCase::IgnoreCase);
            }
            default: break;
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_Match(
            const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions,
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> const FCkOptimizationDebugger_Suppression*
    {
        for (const auto& Suppression : InSuppressions)
        {
            if (Matches(Suppression, InFinding))
            { return &Suppression; }
        }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Is_Suppressed(
            const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions,
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> bool
    {
        return TryGet_Match(InSuppressions, InFinding) != nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_Label(
            const FCkOptimizationDebugger_Suppression& InSuppression)
        -> FString
    {
        const auto TierWord = InSuppression.Tier == ECkOptimizationDebugger_SuppressionTier::Project
            ? FString{TEXT("project")}
            : FString{TEXT("yours")};

        auto Covers = FString{};

        switch (InSuppression.Scope)
        {
            case ECkOptimizationDebugger_SuppressionScope::Finding:
            { Covers = TEXT("this finding"); break; }
            case ECkOptimizationDebugger_SuppressionScope::Check:
            { Covers = ck::Format_UE(TEXT("every {} finding"), InSuppression.CheckId); break; }
            case ECkOptimizationDebugger_SuppressionScope::Asset:
            {
                Covers = InSuppression.CheckId.IsNone()
                    ? ck::Format_UE(TEXT("everything on {}"), InSuppression.Pattern)
                    : ck::Format_UE(TEXT("{} on {}"), InSuppression.CheckId, InSuppression.Pattern);
                break;
            }
            case ECkOptimizationDebugger_SuppressionScope::Folder:
            {
                Covers = InSuppression.CheckId.IsNone()
                    ? ck::Format_UE(TEXT("everything under {}"), InSuppression.Pattern)
                    : ck::Format_UE(TEXT("{} under {}"), InSuppression.CheckId, InSuppression.Pattern);
                break;
            }
            default: break;
        }

        // The reason is never optional in the label, because a suppression whose reason cannot be read is the
        // silent exception this whole feature exists to prevent.
        const auto Reason = InSuppression.Reason.IsEmpty() ? FString{TEXT("no reason given")} : InSuppression.Reason;

        return ck::Format_UE(TEXT("{} ({}) — {}"), Covers, TierWord, Reason);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ForFinding(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            ECkOptimizationDebugger_SuppressionScope InScope,
            ECkOptimizationDebugger_SuppressionTier InTier,
            const FString& InReason,
            const FString& InAuthor,
            const FString& InDate)
        -> FCkOptimizationDebugger_Suppression
    {
        auto Suppression = FCkOptimizationDebugger_Suppression{};
        Suppression.Scope = InScope;
        Suppression.Tier = InTier;
        Suppression.Reason = InReason;
        Suppression.Author = InAuthor;
        Suppression.Date = InDate;

        const auto Path = InFinding.Target.Path.ToString();

        switch (InScope)
        {
            case ECkOptimizationDebugger_SuppressionScope::Finding:
            {
                Suppression.Pattern = InFinding.StableKey;
                break;
            }
            case ECkOptimizationDebugger_SuppressionScope::Asset:
            {
                Suppression.Pattern = Path;
                Suppression.CheckId = InFinding.CheckId;
                break;
            }
            case ECkOptimizationDebugger_SuppressionScope::Folder:
            {
                // The asset's own folder, which is the scope a reader means by "and everything beside it".
                auto Folder = Path;
                auto Left = FString{};
                auto Right = FString{};

                if (Path.Split(TEXT("/"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
                { Folder = Left; }

                Suppression.Pattern = Folder;
                Suppression.CheckId = InFinding.CheckId;
                break;
            }
            case ECkOptimizationDebugger_SuppressionScope::Check:
            {
                Suppression.CheckId = InFinding.CheckId;
                break;
            }
            default: break;
        }

        return Suppression;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Serialize(
            const FCkOptimizationDebugger_Suppression& InSuppression)
        -> FString
    {
        // `Reason` LAST and unescaped: it is the one free-text field, and putting it at the end means a reason
        // containing the separator costs nothing to read in a diff. Newlines are stripped because a config line
        // cannot hold one and a half-written record is worse than a reason without its line breaks.
        auto Reason = InSuppression.Reason;
        Reason.ReplaceInline(TEXT("\r"), TEXT(" "));
        Reason.ReplaceInline(TEXT("\n"), TEXT(" "));

        return ck::Format_UE(TEXT("Scope={};Check={};Pattern={};Author={};Date={};Reason={}"),
            Get_ScopeToken(InSuppression.Scope),
            InSuppression.CheckId.IsNone() ? FString{} : InSuppression.CheckId.ToString(),
            InSuppression.Pattern,
            InSuppression.Author,
            InSuppression.Date,
            Reason);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryParse(
            const FString& InLine,
            FCkOptimizationDebugger_Suppression& OutSuppression)
        -> bool
    {
        auto Rest = InLine.TrimStartAndEnd();

        auto ScopeToken = FString{};
        auto CheckToken = FString{};
        auto Pattern = FString{};
        auto Author = FString{};
        auto Date = FString{};

        if (NOT TrySplit_Field(Rest, TEXT("Scope"), ScopeToken, Rest))
        { return false; }

        auto Scope = ECkOptimizationDebugger_SuppressionScope::Finding;

        if (NOT TryGet_Scope(ScopeToken, Scope))
        { return false; }

        if (NOT TrySplit_Field(Rest, TEXT("Check"), CheckToken, Rest))
        { return false; }

        if (NOT TrySplit_Field(Rest, TEXT("Pattern"), Pattern, Rest))
        { return false; }

        if (NOT TrySplit_Field(Rest, TEXT("Author"), Author, Rest))
        { return false; }

        if (NOT TrySplit_Field(Rest, TEXT("Date"), Date, Rest))
        { return false; }

        // Whatever is left IS the reason, separator and all.
        auto Reason = FString{};

        if (Rest.StartsWith(TEXT("Reason="), ESearchCase::CaseSensitive))
        { Reason = Rest.RightChop(7); }

        // A rule that covers nothing is not a rule. Dropping it beats keeping a record that can never match and
        // that a reader would spend time trying to understand.
        const auto NeedsPattern = Scope != ECkOptimizationDebugger_SuppressionScope::Check;

        if (NeedsPattern && Pattern.IsEmpty())
        { return false; }

        if (Scope == ECkOptimizationDebugger_SuppressionScope::Check && CheckToken.IsEmpty())
        { return false; }

        OutSuppression = FCkOptimizationDebugger_Suppression{};
        OutSuppression.Scope = Scope;
        OutSuppression.CheckId = CheckToken.IsEmpty() ? FName{} : FName{*CheckToken};
        OutSuppression.Pattern = Pattern;
        OutSuppression.Author = Author;
        OutSuppression.Date = Date;
        OutSuppression.Reason = Reason;

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Serialize_All(
            const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions)
        -> TArray<FString>
    {
        auto Lines = TArray<FString>{};
        Lines.Reserve(InSuppressions.Num());

        for (const auto& Suppression : InSuppressions)
        { Lines.Add(Serialize(Suppression)); }

        // Sorted, because this file is COMMITTED: an order that follows insertion or a hash layout would put the
        // whole list in every diff the moment two people add an entry.
        Lines.Sort([](const FString& InLhs, const FString& InRhs)
        {
            return InLhs.Compare(InRhs, ESearchCase::CaseSensitive) < 0;
        });

        return Lines;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Parse_All(
            const TArray<FString>& InLines,
            ECkOptimizationDebugger_SuppressionTier InTier,
            int32& OutDroppedCount)
        -> TArray<FCkOptimizationDebugger_Suppression>
    {
        OutDroppedCount = 0;

        auto Suppressions = TArray<FCkOptimizationDebugger_Suppression>{};

        for (const auto& Line : InLines)
        {
            if (Line.TrimStartAndEnd().IsEmpty())
            { continue; }

            auto Parsed = FCkOptimizationDebugger_Suppression{};

            if (NOT TryParse(Line, Parsed))
            {
                ++OutDroppedCount;
                continue;
            }

            Parsed.Tier = InTier;
            Suppressions.Add(MoveTemp(Parsed));
        }

        return Suppressions;
    }
}

// --------------------------------------------------------------------------------------------------------------------
