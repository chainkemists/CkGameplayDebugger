#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCkOptimizationDebugger_FindingRow;

// --------------------------------------------------------------------------------------------------------------------

/** How wide a suppression reaches.
 *
 *  Four scopes rather than one, because "this finding is intentional" and "this whole check does not apply to us"
 *  are different statements with different lifetimes. A per-finding suppression dies with the asset; a per-check
 *  one is a project policy. Collapsing them into one would make the narrow case unstateable and the broad case a
 *  hundred rows. */
enum class ECkOptimizationDebugger_SuppressionScope : uint8
{
    /** One finding, keyed by its stable key. The narrowest, and what a right-click on a row makes. */
    Finding,

    /** Every finding on one asset. Optionally narrowed to one check. */
    Asset,

    /** Every finding under a content path prefix. Optionally narrowed to one check. */
    Folder,

    /** Every finding a check produces, anywhere. The broadest, and the one that most deserves a reason. */
    Check,
};

// --------------------------------------------------------------------------------------------------------------------

/** Where a suppression is stored, which is the same thing as who inherits it.
 *
 *  A suppression is a RULING, not a preference — that is why the project tier exists at all and why it is the
 *  default. It also has to be possible to say "I don't want to see this while I work" without committing that
 *  opinion to everyone, which is what the personal tier is for and all it is for. */
enum class ECkOptimizationDebugger_SuppressionTier : uint8
{
    /** Committed to the project's config. Travels with the repository; the whole team inherits it. */
    Project,

    /** This machine, this user. Never committed. */
    Personal,
};

// --------------------------------------------------------------------------------------------------------------------

/** One reason a finding is not shown, and everything needed to audit that decision later.
 *
 *  `Reason`, `Author` and `Date` are not decoration: a suppression with no reason is indistinguishable from a bug
 *  six months later, and the person who has to decide whether it still applies is usually not the person who made
 *  it. */
struct CKOPTIMIZATIONDEBUGGER_API FCkOptimizationDebugger_Suppression
{
    ECkOptimizationDebugger_SuppressionScope Scope = ECkOptimizationDebugger_SuppressionScope::Finding;

    ECkOptimizationDebugger_SuppressionTier Tier = ECkOptimizationDebugger_SuppressionTier::Project;

    /** The stable key, asset path or folder prefix this applies to. Empty for a `Check`-scoped entry. */
    FString Pattern;

    /** The check this is limited to. Required for `Check` scope; optional for `Asset` and `Folder`, where `None`
     *  means every check. Unused by `Finding`, whose stable key already carries the check id. */
    FName CheckId;

    FString Reason;

    FString Author;

    /** ISO-8601 date, so the line sorts and diffs as text. */
    FString Date;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_suppression
{
    /** Whether THIS suppression covers THIS finding. Pure over plain data, so every scope's semantics are pinned by
     *  a spec rather than discovered in a project. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Matches(
        const FCkOptimizationDebugger_Suppression& InSuppression,
        const FCkOptimizationDebugger_FindingRow& InFinding) -> bool;

    /** The first suppression covering this finding, or null. Returns the MATCH rather than a bool because the
     *  reader has to be able to see which rule hid the row and why — a suppressed finding whose reason cannot be
     *  read is exactly the silent exception this feature exists to prevent. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryGet_Match(
        const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions,
        const FCkOptimizationDebugger_FindingRow& InFinding) -> const FCkOptimizationDebugger_Suppression*;

    CKOPTIMIZATIONDEBUGGER_API auto
    Is_Suppressed(
        const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions,
        const FCkOptimizationDebugger_FindingRow& InFinding) -> bool;

    /** What the suppressed section prints beside a row: the scope, what it covers, and the reason. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_Label(
        const FCkOptimizationDebugger_Suppression& InSuppression) -> FString;

    /** Builds the suppression a right-click would create for this finding at this scope, stamped with who and when.
     *  Pure: the caller supplies the date, so a spec can pin the whole record. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Build_ForFinding(
        const FCkOptimizationDebugger_FindingRow& InFinding,
        ECkOptimizationDebugger_SuppressionScope InScope,
        ECkOptimizationDebugger_SuppressionTier InTier,
        const FString& InReason,
        const FString& InAuthor,
        const FString& InDate) -> FCkOptimizationDebugger_Suppression;

    // ----------------------------------------------------------------------------------------------------------------

    /** One config line. Field order is fixed and `Reason` is LAST, which is what lets a reason contain the
     *  separator without an escaping scheme nobody would read in a diff. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Serialize(
        const FCkOptimizationDebugger_Suppression& InSuppression) -> FString;

    /** Parses one config line. A line that does not parse is DROPPED and reported by the caller rather than
     *  half-applied: a suppression whose scope could not be read would hide findings nobody chose to hide. */
    CKOPTIMIZATIONDEBUGGER_API auto
    TryParse(
        const FString& InLine,
        FCkOptimizationDebugger_Suppression& OutSuppression) -> bool;

    /** The whole set as config lines, SORTED — a committed file that reorders itself between machines is a file
     *  that shows up in every diff. */
    CKOPTIMIZATIONDEBUGGER_API auto
    Serialize_All(
        const TArray<FCkOptimizationDebugger_Suppression>& InSuppressions) -> TArray<FString>;

    CKOPTIMIZATIONDEBUGGER_API auto
    Parse_All(
        const TArray<FString>& InLines,
        ECkOptimizationDebugger_SuppressionTier InTier,
        int32& OutDroppedCount) -> TArray<FCkOptimizationDebugger_Suppression>;
}

// --------------------------------------------------------------------------------------------------------------------
