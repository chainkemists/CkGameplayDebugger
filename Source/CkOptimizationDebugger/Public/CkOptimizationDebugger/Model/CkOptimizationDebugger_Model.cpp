#include "CkOptimizationDebugger/Model/CkOptimizationDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Containers/Map.h"
#include "Math/UnrealMathUtility.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Reset()
    -> void
{
    _Findings.Empty();
    _HasScanned = false;
    _ScannedLevelNames.Empty();
    _LastScanTime = FDateTime{};

    // The queue goes with the findings it staged. Every entry names a finding derived from the world this boundary
    // just swapped, so a surviving queue would offer to apply fixes against rows the list no longer holds. The
    // COLLAPSE set survives alongside the filter for the opposite reason: it is how the reader arranged the view,
    // and a boundary invalidates the answers, never the arrangement.
    _QueuedStableKeys.Empty();

    // The census describes a world that just changed underneath it, and so does the one before it — a delta against
    // a pre-PIE scan would put a number on a comparison nobody made. `_ExcludedLevelNames` deliberately survives:
    // it is the user's narrowing of the question, like the filter state.
    _Summary = FCkOptimizationDebugger_ScanSummary{};
    _PreviousSummary = FCkOptimizationDebugger_ScanSummary{};
    _HasSummary = false;
    _HasPreviousSummary = false;

    // Entering or leaving PIE loads and unloads content wholesale, so a resident census taken before the boundary
    // describes objects that may no longer be there. The memory FILTER survives for the same reason the finding
    // filter does — it is the user's question, not the answer.
    _MemoryRows.Empty();
    _HasMemoryScan = false;
    _StreamingAvailability = ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable;

    // `_CleanupRows` deliberately SURVIVES. A finding names an actor inside a world and a memory row names an object
    // the play session may have freed; a cleanup row names an ASSET, and an asset nothing references is unreferenced
    // whether or not somebody pressed Play. Dropping the census at a PIE boundary would throw away a scan that is
    // still true. The dirty-package rows inside it are live state and go stale — which is why that category is
    // refresh-on-demand and says so.
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_Findings(
        TArray<FCkOptimizationDebugger_FindingRow> InFindings)
    -> void
{
    _Findings = ck_optimization_debugger_model::Get_SortedFindings(InFindings);
    _HasScanned = true;

    // The queue is pruned to what the new scan still has. A staged fix names a finding the reader looked at and
    // decided about; once that finding is gone — fixed, muted into irrelevance, or in a level this scan excluded —
    // the entry would sit in the tray naming a row nothing on screen matches and offering to apply a fix against it.
    //
    // Deliberately NOT what happens to the muted set, which survives a scan that fails to reproduce it: a mute is a
    // standing judgement about a PROBLEM, a queue entry is work staged against a ROW. Pruning the mute set here
    // would silently un-mute every finding in a level nobody opened this session.
    _QueuedStableKeys = ck_optimization_debugger_model::Prune_KeysToFindings(_QueuedStableKeys, _Findings);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_ScanInfo(
        TArray<FString> InScannedLevelNames,
        FDateTime InScanTime)
    -> void
{
    _ScannedLevelNames = MoveTemp(InScannedLevelNames);
    _LastScanTime = InScanTime;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_Summary(
        FCkOptimizationDebugger_ScanSummary InSummary)
    -> void
{
    // Rotate BEFORE overwriting. Gated on `_HasSummary` rather than `_HasScanned` so the rotation does not depend on
    // the order this is called in relative to `Set_Findings` — a call-order dependency between two setters is a bug
    // waiting for the third caller.
    if (_HasSummary)
    {
        _PreviousSummary = _Summary;
        _HasPreviousSummary = true;
    }

    _Summary = MoveTemp(InSummary);
    _HasSummary = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_SummaryDelta() const
    -> FCkOptimizationDebugger_SummaryDelta
{
    if (NOT _HasPreviousSummary)
    { return FCkOptimizationDebugger_SummaryDelta{}; }

    return ck_optimization_debugger_model::Get_SummaryDelta(_PreviousSummary, _Summary);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleFindings() const
    -> TArray<FCkOptimizationDebugger_FindingRow>
{
    auto Visible = TArray<FCkOptimizationDebugger_FindingRow>{};
    Visible.Reserve(_Findings.Num());

    for (const auto& Finding : _Findings)
    {
        if (NOT ck_optimization_debugger_model::Matches_Filter(Finding, _Filter))
        { continue; }

        Visible.Add(Finding);
    }

    return Visible;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleFindingCount() const
    -> int32
{
    auto Count = 0;

    for (const auto& Finding : _Findings)
    {
        if (ck_optimization_debugger_model::Matches_Filter(Finding, _Filter))
        { ++Count; }
    }

    return Count;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_CountsBySeverity() const
    -> FCkOptimizationDebugger_SeverityCounts
{
    return ck_optimization_debugger_model::Get_CountsBySeverity(_Findings);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleCountsBySeverity() const
    -> FCkOptimizationDebugger_SeverityCounts
{
    // Counted in place rather than through Get_VisibleFindings — a tab-bar count binds to this on the paint path
    // and must not allocate a copy of the whole list per frame.
    auto Counts = FCkOptimizationDebugger_SeverityCounts{};

    for (const auto& Finding : _Findings)
    {
        if (NOT ck_optimization_debugger_model::Matches_Filter(Finding, _Filter))
        { continue; }

        switch (Finding.Severity)
        {
            case ECkOptimizationDebugger_Severity::Critical: ++Counts.CriticalCount; break;
            case ECkOptimizationDebugger_Severity::Major:    ++Counts.MajorCount;    break;
            case ECkOptimizationDebugger_Severity::Minor:    ++Counts.MinorCount;    break;
            default: break;
        }
    }

    return Counts;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleCountsBySeverity_IgnoringSeverityMask() const
    -> FCkOptimizationDebugger_SeverityCounts
{
    auto Counts = FCkOptimizationDebugger_SeverityCounts{};

    auto UnmaskedFilter = _Filter;
    UnmaskedFilter.SeverityMask = ck_optimization_debugger_model::k_AllSeverityMask;

    for (const auto& Finding : _Findings)
    {
        if (NOT ck_optimization_debugger_model::Matches_Filter(Finding, UnmaskedFilter))
        { continue; }

        switch (Finding.Severity)
        {
            case ECkOptimizationDebugger_Severity::Critical: ++Counts.CriticalCount; break;
            case ECkOptimizationDebugger_Severity::Major:    ++Counts.MajorCount;    break;
            case ECkOptimizationDebugger_Severity::Minor:    ++Counts.MinorCount;    break;
            default: break;
        }
    }

    return Counts;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleCountsByCategory() const
    -> TArray<int32>
{
    const auto AllCategories = ck_optimization_debugger_model::Get_AllCategories();

    auto Counts = TArray<int32>{};
    Counts.SetNumZeroed(AllCategories.Num());

    // The category mask is lifted OUT for this pass. A count printed on the control that toggles a category has to
    // say what that control would give the reader, and a category currently toggled off would otherwise read zero —
    // making the button that turns it back on the one claiming there is nothing behind it. Every other axis is
    // honoured, so the counts still describe the filter the reader has built.
    auto UnmaskedFilter = _Filter;
    UnmaskedFilter.CategoryMask = ck_optimization_debugger_model::k_AllCategoryMask;

    for (const auto& Finding : _Findings)
    {
        if (NOT ck_optimization_debugger_model::Matches_Filter(Finding, UnmaskedFilter))
        { continue; }

        const auto Index = static_cast<int32>(Finding.Category);

        if (Counts.IsValidIndex(Index))
        { ++Counts[Index]; }
    }

    return Counts;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleFindingsGroupedByCheck() const
    -> TArray<FCkOptimizationDebugger_FindingGroup>
{
    return ck_optimization_debugger_model::Get_FindingsGroupedByCheck(Get_VisibleFindings());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    TryGet_WorstVisibleSeverity() const
    -> TOptional<ECkOptimizationDebugger_Severity>
{
    // Walked in place rather than through Get_VisibleFindings: the findings are already sorted most-severe-first,
    // so the first survivor IS the answer and nothing has to be copied to learn it.
    for (const auto& Finding : _Findings)
    {
        if (ck_optimization_debugger_model::Matches_Filter(Finding, _Filter))
        { return Finding.Severity; }
    }

    return {};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_SeverityVisible(
        ECkOptimizationDebugger_Severity InSeverity) const
    -> bool
{
    return (_Filter.SeverityMask & ck_optimization_debugger_model::Get_SeverityBit(InSeverity)) != 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_SeverityVisible(
        ECkOptimizationDebugger_Severity InSeverity,
        bool InVisible)
    -> void
{
    const auto Bit = ck_optimization_debugger_model::Get_SeverityBit(InSeverity);

    if (InVisible)
    { _Filter.SeverityMask |= Bit; }
    else
    { _Filter.SeverityMask &= static_cast<uint8>(~Bit); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_CategoryVisible(
        ECkOptimizationDebugger_Category InCategory) const
    -> bool
{
    return (_Filter.CategoryMask & ck_optimization_debugger_model::Get_CategoryBit(InCategory)) != 0;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_CategoryVisible(
        ECkOptimizationDebugger_Category InCategory,
        bool InVisible)
    -> void
{
    const auto Bit = ck_optimization_debugger_model::Get_CategoryBit(InCategory);

    if (InVisible)
    { _Filter.CategoryMask |= Bit; }
    else
    { _Filter.CategoryMask &= static_cast<uint8>(~Bit); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_SeveritySolo(
        ECkOptimizationDebugger_Severity InSeverity)
    -> void
{
    // An assignment, never a toggle: the badge the reader clicked names the one severity they want, and a second
    // click on the same badge must leave that answer standing rather than emptying the list.
    _Filter.SeverityMask = ck_optimization_debugger_model::Get_SeverityBit(InSeverity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_PathScope() const
    -> const FString&
{
    return _Filter.PathScope;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_PathScope(
        FString InPathScope)
    -> void
{
    // Trimmed, and a lone trailing slash is kept rather than stripped: `/Game/Char` and `/Game/Char/` are different
    // questions (the first also admits `/Game/Characters`), and the reader who typed the slash meant it.
    _Filter.PathScope = InPathScope.TrimStartAndEnd();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_ShowOnlyWithSuggestedFix() const
    -> bool
{
    return _Filter.ShowOnlyWithSuggestedFix;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_ShowOnlyWithSuggestedFix(
        bool InShowOnly)
    -> void
{
    _Filter.ShowOnlyWithSuggestedFix = InShowOnly;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_IsMuted(
        const FString& InStableKey) const
    -> bool
{
    return _Filter.MutedStableKeys.Contains(InStableKey);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_Muted(
        const FString& InStableKey,
        bool InMuted)
    -> void
{
    // An empty key would mute nothing and, worse, would sit in the persisted set forever matching a finding that can
    // never be built — `Build_StableKey` always produces a check id followed by a separator.
    if (InStableKey.IsEmpty())
    { return; }

    if (InMuted)
    {
        _Filter.MutedStableKeys.Add(InStableKey);
        return;
    }

    _Filter.MutedStableKeys.Remove(InStableKey);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_ShowMuted() const
    -> bool
{
    return _Filter.ShowMuted;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_ShowMuted(
        bool InShowMuted)
    -> void
{
    _Filter.ShowMuted = InShowMuted;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_MutedFindingCount() const
    -> int32
{
    auto Count = 0;

    // Over the CURRENT findings, not over the muted set. The set accumulates keys from other scans and other
    // branches; printing its size would claim this level hides findings it does not have.
    for (const auto& Finding : _Findings)
    {
        if (_Filter.MutedStableKeys.Contains(Finding.StableKey))
        { ++Count; }
    }

    return Count;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_MutedStableKeys(
        TSet<FString> InStableKeys)
    -> void
{
    _Filter.MutedStableKeys = MoveTemp(InStableKeys);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_MutedStableKeys() const
    -> const TSet<FString>&
{
    return _Filter.MutedStableKeys;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_MinBudgetRatio() const
    -> float
{
    return _Filter.MinBudgetRatio;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_MinBudgetRatio(
        float InMinRatio)
    -> void
{
    // Clamped at zero rather than ensured on: a negative threshold is not a reader error to report, it is the same
    // question as "no threshold", and every ratio is non-negative by construction.
    _Filter.MinBudgetRatio = FMath::Max(0.0f, InMinRatio);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_FindingsWithBudgetCount() const
    -> int32
{
    auto Count = 0;

    for (const auto& Finding : _Findings)
    {
        if (Finding.BudgetRatio > 0.0f)
        { ++Count; }
    }

    return Count;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_IsCheckCollapsed(
        FName InCheckId) const
    -> bool
{
    return _CollapsedCheckIds.Contains(InCheckId);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_CheckCollapsed(
        FName InCheckId,
        bool InCollapsed)
    -> void
{
    if (InCheckId.IsNone())
    { return; }

    if (InCollapsed)
    { _CollapsedCheckIds.Add(InCheckId); }
    else
    { _CollapsedCheckIds.Remove(InCheckId); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_AllChecksCollapsed(
        bool InCollapsed)
    -> void
{
    // Expanding EMPTIES the set rather than listing every id as expanded. The set names what is folded, so a check
    // this scan has never produced comes up expanded by default — which is the state a fresh answer should arrive
    // in. Storing the inverse would make a new check inherit whatever the last Collapse-all decided about checks it
    // was not part of.
    if (NOT InCollapsed)
    {
        _CollapsedCheckIds.Empty();
        return;
    }

    // Over the whole scan, not the visible subset: the reader pressing Collapse all is arranging the LIST, and a
    // group that reappears when they widen the filter must not come back expanded on its own.
    for (const auto& Finding : _Findings)
    {
        _CollapsedCheckIds.Add(Finding.CheckId);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_CollapsedCheckIds(
        TSet<FName> InCheckIds)
    -> void
{
    _CollapsedCheckIds = MoveTemp(InCheckIds);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_CollapsedCheckIds() const
    -> const TSet<FName>&
{
    return _CollapsedCheckIds;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_IsQueued(
        const FString& InStableKey) const
    -> bool
{
    return _QueuedStableKeys.Contains(InStableKey);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_Queued(
        const FString& InStableKey,
        bool InQueued)
    -> void
{
    // Same guard as the muted set, and for the same reason: an empty key matches no finding that can ever be built,
    // so it would sit in the queue forever contributing to a count nothing on screen explains.
    if (InStableKey.IsEmpty())
    { return; }

    if (InQueued)
    { _QueuedStableKeys.Add(InStableKey); }
    else
    { _QueuedStableKeys.Remove(InStableKey); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_QueuedForKeys(
        const TArray<FString>& InStableKeys,
        bool InQueued)
    -> void
{
    for (const auto& Key : InStableKeys)
    {
        Set_Queued(Key, InQueued);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Clear_Queue()
    -> void
{
    _QueuedStableKeys.Empty();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_QueuedFindings() const
    -> TArray<FCkOptimizationDebugger_FindingRow>
{
    // Walked over the SORTED findings rather than over the key set, so the tray reads worst-first exactly as the
    // list does. Iterating the set would order the tray by hash layout, which is an order nothing else in this
    // window uses and which changes between sessions.
    return ck_optimization_debugger_model::Get_FindingsForKeys(_Findings, _QueuedStableKeys);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_QueuedFindingCount() const
    -> int32
{
    // The SET's size is the right number here, and only because `Set_Findings` prunes it: every key in it names a
    // finding the current scan holds. Counting the findings instead would walk the whole list to reach the same
    // answer on the paint path.
    return _QueuedStableKeys.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_QueuedFindingsGroupedByCheck() const
    -> TArray<FCkOptimizationDebugger_FindingGroup>
{
    return ck_optimization_debugger_model::Get_FindingsGroupedByCheck(Get_QueuedFindings());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_QueuedStableKeys() const
    -> const TSet<FString>&
{
    return _QueuedStableKeys;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_LevelExcluded(
        FName InLevelName) const
    -> bool
{
    return _ExcludedLevelNames.Contains(InLevelName);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_LevelExcluded(
        FName InLevelName,
        bool InExcluded)
    -> void
{
    if (InLevelName.IsNone())
    { return; }

    if (InExcluded)
    { _ExcludedLevelNames.Add(InLevelName); }
    else
    { _ExcludedLevelNames.Remove(InLevelName); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_ExcludedLevelNames(
        TSet<FName> InLevelNames)
    -> void
{
    _ExcludedLevelNames = MoveTemp(InLevelNames);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_MemoryRows(
        TArray<FCkOptimizationDebugger_MemoryRow> InRows,
        ECkOptimizationDebugger_StreamingAvailability InStreamingAvailability)
    -> void
{
    // Stored as handed in. Unlike the findings, memory rows are NOT sorted on store: the reader picks the column,
    // and a stored order would be a fourth ordering nothing renders from.
    _MemoryRows = MoveTemp(InRows);
    _StreamingAvailability = InStreamingAvailability;
    _HasMemoryScan = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_SortedMemoryRows(
        ECkOptimizationDebugger_MemoryTable InTable,
        ECkOptimizationDebugger_MemoryColumn InColumn,
        bool InAscending) const
    -> TArray<FCkOptimizationDebugger_MemoryRow>
{
    return ck_optimization_debugger_model::Get_SortedMemoryRows(
        _MemoryRows, InTable, InColumn, InAscending, _MemoryFilterString);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_MemoryTotals() const
    -> FCkOptimizationDebugger_MemoryTotals
{
    return ck_optimization_debugger_model::Get_MemoryTotals(_MemoryRows);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleMemoryRowCount(
        ECkOptimizationDebugger_MemoryTable InTable) const
    -> int32
{
    // Counted without materializing the rows — this is what a tab-bar count binds to.
    auto Count = 0;

    for (const auto& Row : _MemoryRows)
    {
        if (Row.Table != InTable)
        { continue; }

        if (NOT ck_optimization_debugger_model::Matches_MemoryFilter(Row, _MemoryFilterString))
        { continue; }

        ++Count;
    }

    return Count;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Set_CleanupRows(
        TArray<FCkOptimizationDebugger_CleanupRow> InRows,
        FDateTime InScanTime)
    -> void
{
    // Stored as handed in. The cleanup scan already sorts what it produces so two identical passes agree; the page
    // orders per category on read, because the category the reader is looking at decides which order means anything.
    _CleanupRows = MoveTemp(InRows);
    _LastCleanupScanTime = InScanTime;
    _HasCleanupScan = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_SortedCleanupRows(
        ECkOptimizationDebugger_CleanupCategory InCategory) const
    -> TArray<FCkOptimizationDebugger_CleanupRow>
{
    return ck_optimization_debugger_model::Get_SortedCleanupRows(_CleanupRows, InCategory, _CleanupFilterString);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_CleanupGroups(
        ECkOptimizationDebugger_CleanupCategory InCategory) const
    -> TArray<FCkOptimizationDebugger_CleanupGroup>
{
    return ck_optimization_debugger_model::Get_CleanupGroups(_CleanupRows, InCategory, _CleanupFilterString);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_CleanupTotals() const
    -> FCkOptimizationDebugger_CleanupTotals
{
    return ck_optimization_debugger_model::Get_CleanupTotals(_CleanupRows);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Get_VisibleCleanupRowCount(
        ECkOptimizationDebugger_CleanupCategory InCategory) const
    -> int32
{
    // Counted without materializing the rows — this is what a sub-tab count binds to.
    auto Count = 0;

    for (const auto& Row : _CleanupRows)
    {
        if (Row.Category != InCategory)
        { continue; }

        if (NOT ck_optimization_debugger_model::Matches_CleanupFilter(Row, _CleanupFilterString))
        { continue; }

        ++Count;
    }

    return Count;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkOptimizationDebugger_Model::
    Drop_DirtyPackageRows()
    -> int32
{
    const auto Before = _CleanupRows.Num();

    _CleanupRows.RemoveAll([](const FCkOptimizationDebugger_CleanupRow& InRow) -> bool
    {
        return InRow.Category == ECkOptimizationDebugger_CleanupCategory::DirtyPackages;
    });

    // `_HasCleanupScan` deliberately stays true. The other three categories are still a real answer, and flipping
    // the page back to its empty state would throw away a scan that is still true to remove the one part that is not.
    return Before - _CleanupRows.Num();
}

// --------------------------------------------------------------------------------------------------------------------
// Pure projections
// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_model
{
    auto
        Get_AllPages()
        -> TArray<ECkOptimizationDebugger_Page>
    {
        return TArray<ECkOptimizationDebugger_Page>
        {
            ECkOptimizationDebugger_Page::Dashboard,
            ECkOptimizationDebugger_Page::Findings,
            ECkOptimizationDebugger_Page::Memory,
            ECkOptimizationDebugger_Page::Profiling,
            ECkOptimizationDebugger_Page::Cleanup,
        };
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllSeverities()
        -> TArray<ECkOptimizationDebugger_Severity>
    {
        return TArray<ECkOptimizationDebugger_Severity>
        {
            ECkOptimizationDebugger_Severity::Critical,
            ECkOptimizationDebugger_Severity::Major,
            ECkOptimizationDebugger_Severity::Minor,
        };
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllCategories()
        -> TArray<ECkOptimizationDebugger_Category>
    {
        return TArray<ECkOptimizationDebugger_Category>
        {
            ECkOptimizationDebugger_Category::Mesh,
            ECkOptimizationDebugger_Category::Texture,
            ECkOptimizationDebugger_Category::Material,
            ECkOptimizationDebugger_Category::Lighting,
            ECkOptimizationDebugger_Category::Actor,
            ECkOptimizationDebugger_Category::Blueprint,
            ECkOptimizationDebugger_Category::ProjectSettings,
        };
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_PageId(
            ECkOptimizationDebugger_Page InPage)
        -> FName
    {
        switch (InPage)
        {
            case ECkOptimizationDebugger_Page::Dashboard: return FName{TEXT("Dashboard")};
            case ECkOptimizationDebugger_Page::Findings:  return FName{TEXT("Findings")};
            case ECkOptimizationDebugger_Page::Memory:    return FName{TEXT("Memory")};
            case ECkOptimizationDebugger_Page::Profiling: return FName{TEXT("Profiling")};
            case ECkOptimizationDebugger_Page::Cleanup:   return FName{TEXT("Cleanup")};
            default:                                     return FName{TEXT("Dashboard")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_PageLabel(
            ECkOptimizationDebugger_Page InPage)
        -> FString
    {
        switch (InPage)
        {
            case ECkOptimizationDebugger_Page::Dashboard: return TEXT("Dashboard");
            case ECkOptimizationDebugger_Page::Findings:  return TEXT("Level analysis");
            case ECkOptimizationDebugger_Page::Memory:    return TEXT("Memory");
            case ECkOptimizationDebugger_Page::Profiling: return TEXT("Profiling");
            case ECkOptimizationDebugger_Page::Cleanup:   return TEXT("Cleanup");
            default:                                     return TEXT("Dashboard");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_PageIndex(
            ECkOptimizationDebugger_Page InPage)
        -> int32
    {
        return static_cast<int32>(InPage);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_PageFromId(
            FName InPageId)
        -> TOptional<ECkOptimizationDebugger_Page>
    {
        for (const auto Page : Get_AllPages())
        {
            if (Get_PageId(Page) == InPageId)
            { return Page; }
        }

        return {};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SeverityBit(
            ECkOptimizationDebugger_Severity InSeverity)
        -> uint8
    {
        return static_cast<uint8>(1 << static_cast<uint8>(InSeverity));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CategoryBit(
            ECkOptimizationDebugger_Category InCategory)
        -> uint8
    {
        return static_cast<uint8>(1 << static_cast<uint8>(InCategory));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SeverityLabel(
            ECkOptimizationDebugger_Severity InSeverity)
        -> FString
    {
        switch (InSeverity)
        {
            case ECkOptimizationDebugger_Severity::Critical: return TEXT("Critical");
            case ECkOptimizationDebugger_Severity::Major:    return TEXT("Major");
            case ECkOptimizationDebugger_Severity::Minor:    return TEXT("Minor");
            default:                                         return TEXT("Minor");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CategoryLabel(
            ECkOptimizationDebugger_Category InCategory)
        -> FString
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_Category::Mesh:            return TEXT("Mesh");
            case ECkOptimizationDebugger_Category::Texture:         return TEXT("Texture");
            case ECkOptimizationDebugger_Category::Material:        return TEXT("Material");
            case ECkOptimizationDebugger_Category::Lighting:        return TEXT("Lighting");
            case ECkOptimizationDebugger_Category::Actor:           return TEXT("Actor");
            case ECkOptimizationDebugger_Category::Blueprint:       return TEXT("Blueprint");
            case ECkOptimizationDebugger_Category::ProjectSettings: return TEXT("Project settings");
            default:                                                return TEXT("Mesh");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SeverityTone(
            ECkOptimizationDebugger_Severity InSeverity)
        -> ECk_Tone
    {
        switch (InSeverity)
        {
            case ECkOptimizationDebugger_Severity::Critical: return ECk_Tone::Err;
            case ECkOptimizationDebugger_Severity::Major:    return ECk_Tone::Warn;
            case ECkOptimizationDebugger_Severity::Minor:    return ECk_Tone::Info;
            default:                                         return ECk_Tone::Neutral;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_StableKey(
            FName InCheckId,
            const FCkOptimizationDebugger_Target& InTarget)
        -> FString
    {
        // A ProjectSettings finding names no object, so its section stands in for the path. Everything else is
        // addressed by its soft path, which is the only identifier that survives a re-scan unchanged.
        const auto TargetText = InTarget.Kind == ECkOptimizationDebugger_TargetKind::ProjectSettings
            ? InTarget.SettingsSectionName
            : InTarget.Path.ToString();

        return ck::Format_UE(TEXT("{}|{}"), InCheckId, TargetText);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_FindingSearchText(
            const FCkOptimizationDebugger_FindingRow& InFinding)
        -> FString
    {
        return ck::Format_UE(TEXT("{} {} {}"), InFinding.Title, InFinding.Target.DisplayName, InFinding.CheckId);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Passes_TextFilter(
            const FString& InSearchText,
            const FString& InFilter)
        -> bool
    {
        if (InFilter.IsEmpty())
        { return true; }

        return InSearchText.Contains(InFilter, ESearchCase::IgnoreCase);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Matches_PathScope(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            const FString& InPathScope)
        -> bool
    {
        if (InPathScope.IsEmpty())
        { return true; }

        const auto Path = InFinding.Target.Path.ToString();

        // A `ProjectSettings` finding has no path at all, so ANY non-empty scope excludes it. Deliberate: a reader
        // narrowing to a content folder is not asking about the renderer's settings, and silently keeping those rows
        // in a scoped view would make the scope look like it had failed to apply.
        if (Path.IsEmpty())
        { return false; }

        // Case-insensitive because package paths are, on the platform this ships on and in the Content Browser.
        return Path.StartsWith(InPathScope, ESearchCase::IgnoreCase);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Matches_Filter(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            const FCkOptimizationDebugger_FilterState& InFilter)
        -> bool
    {
        if ((InFilter.SeverityMask & Get_SeverityBit(InFinding.Severity)) == 0)
        { return false; }

        if ((InFilter.CategoryMask & Get_CategoryBit(InFinding.Category)) == 0)
        { return false; }

        if (InFilter.ShowOnlyWithSuggestedFix && NOT InFinding.HasAutoFix)
        { return false; }

        // A finding with no budget behind it carries a zero ratio and is excluded by ANY non-zero threshold — the
        // same reading that excludes a pathless ProjectSettings finding from a path scope. A reader asking what is
        // furthest over budget is not asking about the checks that measure nothing, and keeping those rows would
        // make the threshold look like it had failed to apply.
        if (InFilter.MinBudgetRatio > 0.0f && InFinding.BudgetRatio < InFilter.MinBudgetRatio)
        { return false; }

        // Muting is a hide, not a delete: `ShowMuted` brings them back marked rather than un-muting them, so the
        // reader can always audit what they told the tool to stop showing. Checked before the path and text work
        // because it is a set lookup and they are string scans.
        if (NOT InFilter.ShowMuted && InFilter.MutedStableKeys.Contains(InFinding.StableKey))
        { return false; }

        if (NOT Matches_PathScope(InFinding, InFilter.PathScope))
        { return false; }

        // The empty-filter case is answered BEFORE the haystack is built. `Passes_TextFilter` early-outs on an empty
        // query too, but its argument is evaluated first — so building the string there would allocate one `FString`
        // per finding on a query that matches everything, which is the state the list spends most of its life in.
        if (InFilter.FilterString.IsEmpty())
        { return true; }

        return Passes_TextFilter(Build_FindingSearchText(InFinding), InFilter.FilterString);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Matches_Highlight(
            const FCkOptimizationDebugger_FindingRow& InFinding,
            const FCkOptimizationDebugger_FilterState& InFilter)
        -> bool
    {
        // Same argument-evaluation guard as `Matches_Filter`: an empty highlight is the resting state, and it must
        // not cost an allocation per finding.
        if (InFilter.HighlightString.IsEmpty())
        { return true; }

        return Passes_TextFilter(Build_FindingSearchText(InFinding), InFilter.HighlightString);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SortedFindings(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TArray<FCkOptimizationDebugger_FindingRow>
    {
        auto Sorted = InFindings;

        Sorted.Sort([](const FCkOptimizationDebugger_FindingRow& InLhs, const FCkOptimizationDebugger_FindingRow& InRhs)
        {
            const auto LhsSeverity = static_cast<uint8>(InLhs.Severity);
            const auto RhsSeverity = static_cast<uint8>(InRhs.Severity);

            if (LhsSeverity != RhsSeverity)
            { return LhsSeverity < RhsSeverity; }

            const auto LhsCategory = static_cast<uint8>(InLhs.Category);
            const auto RhsCategory = static_cast<uint8>(InRhs.Category);

            if (LhsCategory != RhsCategory)
            { return LhsCategory < RhsCategory; }

            const auto TitleOrder = InLhs.Title.Compare(InRhs.Title, ESearchCase::IgnoreCase);

            if (TitleOrder != 0)
            { return TitleOrder < 0; }

            // Total by construction: TArray::Sort is unstable, so without this two findings equal on every visible
            // key would swap places between otherwise identical scans.
            return InLhs.StableKey.Compare(InRhs.StableKey, ESearchCase::CaseSensitive) < 0;
        });

        return Sorted;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CountsBySeverity(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> FCkOptimizationDebugger_SeverityCounts
    {
        auto Counts = FCkOptimizationDebugger_SeverityCounts{};

        for (const auto& Finding : InFindings)
        {
            switch (Finding.Severity)
            {
                case ECkOptimizationDebugger_Severity::Critical: ++Counts.CriticalCount; break;
                case ECkOptimizationDebugger_Severity::Major:    ++Counts.MajorCount;    break;
                case ECkOptimizationDebugger_Severity::Minor:    ++Counts.MinorCount;    break;
                default: break;
            }
        }

        return Counts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CountsByCategory(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TArray<int32>
    {
        auto Counts = TArray<int32>{};

        // Sized from the category list rather than from the largest value seen, so a category with no findings still
        // has an entry to read and a caller never has to distinguish "absent" from "zero".
        Counts.SetNumZeroed(Get_AllCategories().Num());

        for (const auto& Finding : InFindings)
        {
            const auto Index = static_cast<int32>(Finding.Category);

            if (Counts.IsValidIndex(Index))
            { ++Counts[Index]; }
        }

        return Counts;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_FindingsForKeys(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings,
            const TSet<FString>& InStableKeys)
        -> TArray<FCkOptimizationDebugger_FindingRow>
    {
        auto Matching = TArray<FCkOptimizationDebugger_FindingRow>{};

        if (InStableKeys.IsEmpty())
        { return Matching; }

        Matching.Reserve(InStableKeys.Num());

        // Driven by the FINDINGS, not by the key set: the findings carry the sorted order every other list in this
        // window renders in, while a set iterates in hash-layout order that changes between sessions.
        for (const auto& Finding : InFindings)
        {
            if (InStableKeys.Contains(Finding.StableKey))
            { Matching.Add(Finding); }
        }

        return Matching;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Prune_KeysToFindings(
            const TSet<FString>& InStableKeys,
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TSet<FString>
    {
        auto Kept = TSet<FString>{};

        if (InStableKeys.IsEmpty())
        { return Kept; }

        for (const auto& Finding : InFindings)
        {
            if (InStableKeys.Contains(Finding.StableKey))
            { Kept.Add(Finding.StableKey); }
        }

        return Kept;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_BudgetRatio(
            float InRatio)
        -> FString
    {
        // Empty rather than `0x` for a finding with no budget: a badge that printed a number would claim this check
        // measured something against a budget it does not have.
        if (InRatio <= 0.0f)
        { return FString{}; }

        // One decimal below ten, none above it. `12.7x` and `13x` are the same statement to a reader triaging a
        // list, and the shorter one fits a cell that has no room to spare.
        if (InRatio < 10.0f)
        { return FString::Printf(TEXT("%.1fx"), InRatio); }

        return FString::Printf(TEXT("%.0fx"), InRatio);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_FindingsGroupedByCheck(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TArray<FCkOptimizationDebugger_FindingGroup>
    {
        auto Groups = TArray<FCkOptimizationDebugger_FindingGroup>{};
        auto IndexByCheckId = TMap<FName, int32>{};

        for (const auto& Finding : InFindings)
        {
            if (const auto* Found = IndexByCheckId.Find(Finding.CheckId))
            {
                auto& Group = Groups[*Found];
                Group.Findings.Add(Finding);

                // The input is severity-sorted, so a later member can never be MORE severe than the first one. The
                // comparison is kept anyway: the projection must stay honest if it is ever handed an unsorted list.
                if (static_cast<uint8>(Finding.Severity) < static_cast<uint8>(Group.WorstSeverity))
                { Group.WorstSeverity = Finding.Severity; }

                continue;
            }

            auto Group = FCkOptimizationDebugger_FindingGroup{};
            Group.CheckId = Finding.CheckId;
            Group.Title = Finding.Title;
            Group.WorstSeverity = Finding.Severity;
            Group.Category = Finding.Category;
            Group.Findings.Add(Finding);

            IndexByCheckId.Add(Finding.CheckId, Groups.Num());
            Groups.Add(MoveTemp(Group));
        }

        return Groups;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_WorstSeverity(
            const TArray<FCkOptimizationDebugger_FindingRow>& InFindings)
        -> TOptional<ECkOptimizationDebugger_Severity>
    {
        auto Worst = TOptional<ECkOptimizationDebugger_Severity>{};

        for (const auto& Finding : InFindings)
        {
            if (NOT Worst.IsSet() || static_cast<uint8>(Finding.Severity) < static_cast<uint8>(Worst.GetValue()))
            { Worst = Finding.Severity; }
        }

        return Worst;
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Dashboard projections
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllDiskCategories()
        -> TArray<ECkOptimizationDebugger_DiskCategory>
    {
        return TArray<ECkOptimizationDebugger_DiskCategory>
        {
            ECkOptimizationDebugger_DiskCategory::Textures,
            ECkOptimizationDebugger_DiskCategory::StaticMeshes,
            ECkOptimizationDebugger_DiskCategory::Materials,
            ECkOptimizationDebugger_DiskCategory::Blueprints,
            ECkOptimizationDebugger_DiskCategory::Levels,
            ECkOptimizationDebugger_DiskCategory::Audio,
            ECkOptimizationDebugger_DiskCategory::Animation,
            ECkOptimizationDebugger_DiskCategory::Other,
        };
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DiskCategoryLabel(
            ECkOptimizationDebugger_DiskCategory InCategory)
        -> FString
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_DiskCategory::Textures:     return TEXT("Textures");
            case ECkOptimizationDebugger_DiskCategory::StaticMeshes: return TEXT("Static meshes");
            case ECkOptimizationDebugger_DiskCategory::Materials:    return TEXT("Materials");
            case ECkOptimizationDebugger_DiskCategory::Blueprints:   return TEXT("Blueprints");
            case ECkOptimizationDebugger_DiskCategory::Levels:       return TEXT("Levels");
            case ECkOptimizationDebugger_DiskCategory::Audio:        return TEXT("Audio");
            case ECkOptimizationDebugger_DiskCategory::Animation:    return TEXT("Animation");
            case ECkOptimizationDebugger_DiskCategory::Other:        return TEXT("Other");
            default:                                                 return TEXT("Other");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        ShouldScanLevel(
            FName InLevelName,
            const TSet<FName>& InExcludedLevelNames)
        -> bool
    {
        // A nameless level cannot be excluded by name, and refusing to scan it would hide it from the reader with no
        // way for them to put it back.
        if (InLevelName.IsNone())
        { return true; }

        return NOT InExcludedLevelNames.Contains(InLevelName);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SummaryDelta(
            const FCkOptimizationDebugger_ScanSummary& InPrevious,
            const FCkOptimizationDebugger_ScanSummary& InCurrent)
        -> FCkOptimizationDebugger_SummaryDelta
    {
        const auto Diff = [](int64 InCurrentValue, int64 InPreviousValue) -> int64
        {
            return InCurrentValue - InPreviousValue;
        };

        auto Delta = FCkOptimizationDebugger_SummaryDelta{};
        Delta.HasPrevious = true;

        Delta.ActorCountDelta               = Diff(InCurrent.ActorCount, InPrevious.ActorCount);
        Delta.StaticMeshUsageCountDelta     = Diff(InCurrent.StaticMeshUsageCount, InPrevious.StaticMeshUsageCount);
        Delta.UniqueStaticMeshCountDelta    = Diff(InCurrent.UniqueStaticMeshCount, InPrevious.UniqueStaticMeshCount);
        Delta.UniqueMaterialCountDelta      = Diff(InCurrent.UniqueMaterialCount, InPrevious.UniqueMaterialCount);
        Delta.UniqueTextureCountDelta       = Diff(InCurrent.UniqueTextureCount, InPrevious.UniqueTextureCount);
        Delta.Lod0TriangleTotalDelta        = Diff(InCurrent.Lod0TriangleTotal, InPrevious.Lod0TriangleTotal);
        Delta.LightCountDelta               = Diff(InCurrent.LightCount, InPrevious.LightCount);

        Delta.CriticalCountDelta = Diff(InCurrent.FindingCounts.CriticalCount, InPrevious.FindingCounts.CriticalCount);
        Delta.MajorCountDelta    = Diff(InCurrent.FindingCounts.MajorCount,    InPrevious.FindingCounts.MajorCount);
        Delta.MinorCountDelta    = Diff(InCurrent.FindingCounts.MinorCount,    InPrevious.FindingCounts.MinorCount);
        Delta.FindingTotalDelta  = Diff(InCurrent.FindingCounts.Get_Total(),   InPrevious.FindingCounts.Get_Total());

        Delta.DiskTotalBytesDelta = Diff(InCurrent.Disk.TotalBytes, InPrevious.Disk.TotalBytes);

        return Delta;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_ByteSize(
            int64 InBytes)
        -> FString
    {
        const auto IsNegative = InBytes < 0;

        // Widened to double BEFORE negating, so INT64_MIN cannot overflow on the way to its own absolute value.
        const auto Magnitude = IsNegative ? -static_cast<double>(InBytes) : static_cast<double>(InBytes);

        const auto Sign = FString{IsNegative ? TEXT("-") : TEXT("")};

        constexpr auto k_Kilobyte = 1024.0;
        constexpr auto k_Megabyte = k_Kilobyte * 1024.0;
        constexpr auto k_Gigabyte = k_Megabyte * 1024.0;
        constexpr auto k_Terabyte = k_Gigabyte * 1024.0;

        // Bytes print whole. A "0.4 KB" reads as a rounding artifact where "412 B" reads as a fact.
        if (Magnitude < k_Kilobyte)
        { return ck::Format_UE(TEXT("{}{} B"), Sign, static_cast<int64>(Magnitude)); }

        if (Magnitude < k_Megabyte)
        { return ck::Format_UE(TEXT("{}{:.1f} KB"), Sign, Magnitude / k_Kilobyte); }

        if (Magnitude < k_Gigabyte)
        { return ck::Format_UE(TEXT("{}{:.1f} MB"), Sign, Magnitude / k_Megabyte); }

        if (Magnitude < k_Terabyte)
        { return ck::Format_UE(TEXT("{}{:.1f} GB"), Sign, Magnitude / k_Gigabyte); }

        return ck::Format_UE(TEXT("{}{:.1f} TB"), Sign, Magnitude / k_Terabyte);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_AbbreviatedCount(
            int64 InValue)
        -> FString
    {
        const auto IsNegative = InValue < 0;
        const auto Magnitude = IsNegative ? -static_cast<double>(InValue) : static_cast<double>(InValue);

        const auto Sign = FString{IsNegative ? TEXT("-") : TEXT("")};

        constexpr auto k_Thousand = 1000.0;
        constexpr auto k_Million  = k_Thousand * 1000.0;
        constexpr auto k_Billion  = k_Million * 1000.0;

        if (Magnitude < k_Thousand)
        { return ck::Format_UE(TEXT("{}{}"), Sign, static_cast<int64>(Magnitude)); }

        if (Magnitude < k_Million)
        { return ck::Format_UE(TEXT("{}{:.1f}K"), Sign, Magnitude / k_Thousand); }

        if (Magnitude < k_Billion)
        { return ck::Format_UE(TEXT("{}{:.1f}M"), Sign, Magnitude / k_Million); }

        return ck::Format_UE(TEXT("{}{:.1f}B"), Sign, Magnitude / k_Billion);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_Delta(
            int64 InDelta)
        -> FString
    {
        if (InDelta == 0)
        { return TEXT("—"); }

        // The '+' is explicit: a bare "12" under a stat tile reads as a second value, not as a change to the one
        // above it.
        return InDelta > 0
            ? ck::Format_UE(TEXT("+{}"), Format_AbbreviatedCount(InDelta))
            : ck::Format_UE(TEXT("-{}"), Format_AbbreviatedCount(-InDelta));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DeltaTone(
            int64 InDelta,
            bool InFewerIsBetter)
        -> ECk_Tone
    {
        // A stat with no better direction is never coloured. Painting a level that grew by ten actors red would be
        // this tool asserting that growth is a defect, which it has no basis to claim.
        if (NOT InFewerIsBetter)
        { return ECk_Tone::Neutral; }

        if (InDelta < 0)
        { return ECk_Tone::Ok; }

        if (InDelta > 0)
        { return ECk_Tone::Err; }

        return ECk_Tone::Neutral;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DiskCategoryFraction(
            int64 InCategoryBytes,
            int64 InTotalBytes)
        -> float
    {
        if (InTotalBytes <= 0)
        { return 0.0f; }

        return FMath::Clamp(static_cast<float>(static_cast<double>(InCategoryBytes) /
                                               static_cast<double>(InTotalBytes)), 0.0f, 1.0f);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // MEMORY ANALYZER PROJECTIONS
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllMemoryTables()
        -> TArray<ECkOptimizationDebugger_MemoryTable>
    {
        return TArray<ECkOptimizationDebugger_MemoryTable>{
            ECkOptimizationDebugger_MemoryTable::Textures,
            ECkOptimizationDebugger_MemoryTable::RenderTargets,
            ECkOptimizationDebugger_MemoryTable::StaticMeshes};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Has_SeparableGpuSize(
            ECkOptimizationDebugger_MemoryTable InTable)
        -> bool
    {
        // Textures are the one family whose `GetResourceSizeEx` fills the dedicated-video bucket. Render targets and
        // static meshes both add their whole cost through `AddUnknownMemoryBytes`, so a zero from them is "not
        // reported", not "nothing on the GPU" — which is the distinction the em dash in that column carries.
        return InTable == ECkOptimizationDebugger_MemoryTable::Textures;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryTableId(
            ECkOptimizationDebugger_MemoryTable InTable)
        -> FName
    {
        switch (InTable)
        {
            case ECkOptimizationDebugger_MemoryTable::Textures:     return FName{TEXT("Memory.Textures")};
            case ECkOptimizationDebugger_MemoryTable::RenderTargets:return FName{TEXT("Memory.RenderTargets")};
            case ECkOptimizationDebugger_MemoryTable::StaticMeshes: return FName{TEXT("Memory.StaticMeshes")};
            default:                                                return FName{TEXT("Memory.Textures")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryTableLabel(
            ECkOptimizationDebugger_MemoryTable InTable)
        -> FString
    {
        switch (InTable)
        {
            case ECkOptimizationDebugger_MemoryTable::Textures:      return FString{TEXT("Textures")};
            case ECkOptimizationDebugger_MemoryTable::RenderTargets: return FString{TEXT("Render Targets")};
            case ECkOptimizationDebugger_MemoryTable::StaticMeshes:  return FString{TEXT("Static Meshes")};
            default:                                                 return FString{TEXT("Textures")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_MemoryTableFromId(
            FName InTableId)
        -> TOptional<ECkOptimizationDebugger_MemoryTable>
    {
        for (const auto Table : Get_AllMemoryTables())
        {
            if (Get_MemoryTableId(Table) == InTableId)
            { return Table; }
        }

        return TOptional<ECkOptimizationDebugger_MemoryTable>{};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllMemoryColumns()
        -> TArray<ECkOptimizationDebugger_MemoryColumn>
    {
        return TArray<ECkOptimizationDebugger_MemoryColumn>{
            ECkOptimizationDebugger_MemoryColumn::Name,
            ECkOptimizationDebugger_MemoryColumn::Type,
            ECkOptimizationDebugger_MemoryColumn::Dimensions,
            ECkOptimizationDebugger_MemoryColumn::ResourceSize,
            ECkOptimizationDebugger_MemoryColumn::GpuSize,
            ECkOptimizationDebugger_MemoryColumn::Streaming};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryColumnId(
            ECkOptimizationDebugger_MemoryColumn InColumn)
        -> FName
    {
        switch (InColumn)
        {
            case ECkOptimizationDebugger_MemoryColumn::Name:         return FName{TEXT("Name")};
            case ECkOptimizationDebugger_MemoryColumn::Type:         return FName{TEXT("Type")};
            case ECkOptimizationDebugger_MemoryColumn::Dimensions:   return FName{TEXT("Dimensions")};
            case ECkOptimizationDebugger_MemoryColumn::ResourceSize: return FName{TEXT("ResourceSize")};
            case ECkOptimizationDebugger_MemoryColumn::GpuSize:      return FName{TEXT("GpuSize")};
            case ECkOptimizationDebugger_MemoryColumn::Streaming:    return FName{TEXT("Streaming")};
            default:                                                 return FName{TEXT("Name")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryColumnLabel(
            ECkOptimizationDebugger_MemoryColumn InColumn)
        -> FString
    {
        switch (InColumn)
        {
            case ECkOptimizationDebugger_MemoryColumn::Name:         return FString{TEXT("Asset")};
            case ECkOptimizationDebugger_MemoryColumn::Type:         return FString{TEXT("Format / Class")};
            case ECkOptimizationDebugger_MemoryColumn::Dimensions:   return FString{TEXT("Dimensions")};
            case ECkOptimizationDebugger_MemoryColumn::ResourceSize: return FString{TEXT("Size")};
            case ECkOptimizationDebugger_MemoryColumn::GpuSize:      return FString{TEXT("GPU")};
            case ECkOptimizationDebugger_MemoryColumn::Streaming:    return FString{TEXT("Streaming")};
            default:                                                 return FString{TEXT("Asset")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_MemoryColumnFromId(
            FName InColumnId)
        -> TOptional<ECkOptimizationDebugger_MemoryColumn>
    {
        for (const auto Column : Get_AllMemoryColumns())
        {
            if (Get_MemoryColumnId(Column) == InColumnId)
            { return Column; }
        }

        return TOptional<ECkOptimizationDebugger_MemoryColumn>{};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryTypeText(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FString
    {
        return InRow.FormatName.IsEmpty() ? InRow.ClassName : InRow.FormatName;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryDimensionText(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FString
    {
        if (InRow.Table == ECkOptimizationDebugger_MemoryTable::StaticMeshes)
        {
            return ck::Format_UE(TEXT("{} LOD(s) · {} tris"),
                InRow.LodCount, Format_AbbreviatedCount(InRow.Lod0TriangleCount));
        }

        auto Text = ck::Format_UE(TEXT("{} × {}"), InRow.Width, InRow.Height);

        if (InRow.MipCount > 0)
        { Text += ck::Format_UE(TEXT(" · {} mip(s)"), InRow.MipCount); }

        // The texture group is what the streamer buckets this texture by, so it belongs next to the mips it decides
        // the budget for rather than in a column of its own that would be empty on two tables out of three.
        if (NOT InRow.LodGroupName.IsEmpty())
        { Text += ck::Format_UE(TEXT(" · {}"), InRow.LodGroupName); }

        return Text;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryDimensionSortKey(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> int64
    {
        if (InRow.Table == ECkOptimizationDebugger_MemoryTable::StaticMeshes)
        { return InRow.Lod0TriangleCount; }

        return static_cast<int64>(InRow.Width) * static_cast<int64>(InRow.Height);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryStreamingText(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FString
    {
        if (InRow.HasStreamingMetrics)
        { return ck::Format_UE(TEXT("{} / {}"), InRow.ResidentMipCount, InRow.RequestedMipCount); }

        // "Not streamable" is a measurement; the em dash is the absence of one. Collapsing the two would tell the
        // reader a texture never streams when in fact nobody could ask.
        return InRow.IsStreamable ? FString{TEXT("—")} : FString{TEXT("not streamable")};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryStreamingSortKey(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> int32
    {
        return InRow.HasStreamingMetrics ? InRow.ResidentMipCount : -1;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_MemorySearchText(
            const FCkOptimizationDebugger_MemoryRow& InRow)
        -> FString
    {
        return ck::Format_UE(TEXT("{} {} {} {}"),
            InRow.DisplayName, InRow.AssetPath, InRow.ClassName, InRow.FormatName);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Matches_MemoryFilter(
            const FCkOptimizationDebugger_MemoryRow& InRow,
            const FString& InFilter)
        -> bool
    {
        // Answered before the haystack is built. This predicate is walked over every resident object to produce the
        // sub-table counts, so a per-row allocation on an empty query is the one that actually costs something.
        if (InFilter.IsEmpty())
        { return true; }

        return Passes_TextFilter(Build_MemorySearchText(InRow), InFilter);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Compare_MemoryRows(
            const FCkOptimizationDebugger_MemoryRow& InLhs,
            const FCkOptimizationDebugger_MemoryRow& InRhs,
            ECkOptimizationDebugger_MemoryColumn InColumn)
        -> int32
    {
        const auto CompareInt = [](int64 InLhsValue, int64 InRhsValue) -> int32
        {
            if (InLhsValue < InRhsValue)
            { return -1; }

            return InLhsValue > InRhsValue ? 1 : 0;
        };

        switch (InColumn)
        {
            case ECkOptimizationDebugger_MemoryColumn::Name:
            {
                return InLhs.DisplayName.Compare(InRhs.DisplayName, ESearchCase::IgnoreCase);
            }
            case ECkOptimizationDebugger_MemoryColumn::Type:
            {
                return Get_MemoryTypeText(InLhs).Compare(Get_MemoryTypeText(InRhs), ESearchCase::IgnoreCase);
            }
            case ECkOptimizationDebugger_MemoryColumn::Dimensions:
            {
                return CompareInt(Get_MemoryDimensionSortKey(InLhs), Get_MemoryDimensionSortKey(InRhs));
            }
            case ECkOptimizationDebugger_MemoryColumn::ResourceSize:
            {
                return CompareInt(InLhs.ResourceSizeBytes, InRhs.ResourceSizeBytes);
            }
            case ECkOptimizationDebugger_MemoryColumn::GpuSize:
            {
                return CompareInt(InLhs.GpuSizeBytes, InRhs.GpuSizeBytes);
            }
            case ECkOptimizationDebugger_MemoryColumn::Streaming:
            {
                return CompareInt(Get_MemoryStreamingSortKey(InLhs), Get_MemoryStreamingSortKey(InRhs));
            }
            default:
            {
                return 0;
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SortedMemoryRows(
            const TArray<FCkOptimizationDebugger_MemoryRow>& InRows,
            ECkOptimizationDebugger_MemoryTable InTable,
            ECkOptimizationDebugger_MemoryColumn InColumn,
            bool InAscending,
            const FString& InFilter)
        -> TArray<FCkOptimizationDebugger_MemoryRow>
    {
        auto Rows = TArray<FCkOptimizationDebugger_MemoryRow>{};

        for (const auto& Row : InRows)
        {
            if (Row.Table != InTable)
            { continue; }

            if (NOT Matches_MemoryFilter(Row, InFilter))
            { continue; }

            Rows.Add(Row);
        }

        Rows.Sort([InColumn, InAscending](const FCkOptimizationDebugger_MemoryRow& InLhs,
                                          const FCkOptimizationDebugger_MemoryRow& InRhs)
        {
            const auto Order = Compare_MemoryRows(InLhs, InRhs, InColumn);

            if (Order != 0)
            { return InAscending ? Order < 0 : Order > 0; }

            // The path breaks every tie, ASCENDING in both directions: `TArray::Sort` is unstable, so two rows equal
            // on the sorted column would otherwise swap places every time the list is rebuilt. Reversing the
            // tie-break with the arrow would make the SAME two rows swap on a direction toggle, which is the same
            // churn wearing a different hat.
            return InLhs.AssetPath.Compare(InRhs.AssetPath, ESearchCase::CaseSensitive) < 0;
        });

        return Rows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_MemoryTotals(
            const TArray<FCkOptimizationDebugger_MemoryRow>& InRows)
        -> FCkOptimizationDebugger_MemoryTotals
    {
        auto Totals = FCkOptimizationDebugger_MemoryTotals{};

        // Every table gets an entry up front, empty or not — a caller indexing by enum value must always find one.
        for (const auto Table : Get_AllMemoryTables())
        {
            auto TableTotals = FCkOptimizationDebugger_MemoryTableTotals{};
            TableTotals.Table = Table;

            Totals.Tables.Add(TableTotals);
        }

        for (const auto& Row : InRows)
        {
            const auto TableIndex = static_cast<int32>(Row.Table);

            if (NOT Totals.Tables.IsValidIndex(TableIndex))
            { continue; }

            auto& TableTotals = Totals.Tables[TableIndex];

            ++TableTotals.RowCount;
            TableTotals.ResourceSizeBytes += Row.ResourceSizeBytes;

            ++Totals.RowCount;
            Totals.ResourceSizeBytes += Row.ResourceSizeBytes;

            if (NOT Row.HasSeparableGpuSize)
            { continue; }

            TableTotals.GpuSizeBytes += Row.GpuSizeBytes;
            ++TableTotals.SeparableGpuRowCount;

            Totals.GpuSizeBytes += Row.GpuSizeBytes;
            ++Totals.SeparableGpuRowCount;
        }

        return Totals;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_StreamingAvailabilityNote(
            ECkOptimizationDebugger_StreamingAvailability InAvailability)
        -> FString
    {
        switch (InAvailability)
        {
            case ECkOptimizationDebugger_StreamingAvailability::StreamingDisabled:
            {
                return FString{TEXT("Texture streaming is disabled in this session, so resident/wanted mip counts ")
                    TEXT("are not measured. Sizes below are still real.")};
            }
            case ECkOptimizationDebugger_StreamingAvailability::ManagerUnavailable:
            {
                return FString{TEXT("The streaming manager is not available in this session, so resident/wanted mip ")
                    TEXT("counts are not measured. Sizes below are still real.")};
            }
            case ECkOptimizationDebugger_StreamingAvailability::Available:
            default:
            {
                return FString{};
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // PROJECT CLEANUP PROJECTIONS
    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllCleanupCategories()
        -> TArray<ECkOptimizationDebugger_CleanupCategory>
    {
        return {
            ECkOptimizationDebugger_CleanupCategory::Unreferenced,
            ECkOptimizationDebugger_CleanupCategory::Duplicates,
            ECkOptimizationDebugger_CleanupCategory::NameCollisions,
            ECkOptimizationDebugger_CleanupCategory::Redirectors,
            ECkOptimizationDebugger_CleanupCategory::DirtyPackages};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsGroupedCleanupCategory(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> bool
    {
        return InCategory == ECkOptimizationDebugger_CleanupCategory::Duplicates
            || InCategory == ECkOptimizationDebugger_CleanupCategory::NameCollisions;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CleanupCategoryId(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> FName
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_CleanupCategory::Unreferenced:  return FName{TEXT("Cleanup.Unreferenced")};
            case ECkOptimizationDebugger_CleanupCategory::Duplicates:    return FName{TEXT("Cleanup.Duplicates")};
            case ECkOptimizationDebugger_CleanupCategory::NameCollisions: return FName{TEXT("Cleanup.NameCollisions")};
            case ECkOptimizationDebugger_CleanupCategory::Redirectors:   return FName{TEXT("Cleanup.Redirectors")};
            case ECkOptimizationDebugger_CleanupCategory::DirtyPackages: return FName{TEXT("Cleanup.DirtyPackages")};
            default:                                                     return FName{TEXT("Cleanup.Unreferenced")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CleanupCategoryLabel(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> FString
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_CleanupCategory::Unreferenced:  return FString{TEXT("Unreferenced")};
            // "Possible" is in the LABEL, not only in the hint under it. A tab reading "Duplicates" would be a claim
            // this match cannot support, and the reader who only ever sees the tab is the one most likely to act on it.
            case ECkOptimizationDebugger_CleanupCategory::Duplicates:    return FString{TEXT("Possible duplicates")};
            // "Name collisions", never "Duplicate names": these assets are not duplicates of each other and saying so
            // would send the reader looking for something to delete, when the fix is a rename.
            case ECkOptimizationDebugger_CleanupCategory::NameCollisions: return FString{TEXT("Name collisions")};
            case ECkOptimizationDebugger_CleanupCategory::Redirectors:   return FString{TEXT("Redirectors")};
            case ECkOptimizationDebugger_CleanupCategory::DirtyPackages: return FString{TEXT("Dirty packages")};
            default:                                                     return FString{TEXT("Unreferenced")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_CleanupCategoryFromId(
            FName InCategoryId)
        -> TOptional<ECkOptimizationDebugger_CleanupCategory>
    {
        for (const auto Category : Get_AllCleanupCategories())
        {
            if (Get_CleanupCategoryId(Category) == InCategoryId)
            { return Category; }
        }

        return {};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CleanupCategoryHint(
            ECkOptimizationDebugger_CleanupCategory InCategory)
        -> FString
    {
        switch (InCategory)
        {
            case ECkOptimizationDebugger_CleanupCategory::Unreferenced:
            {
                return FString{TEXT("Assets under /Game that no package references on disk AND no registered ")
                    TEXT("external-reference provider claims. Still presented for review: a reference made in code, ")
                    TEXT("in config or by a runtime-built path that no provider covers is invisible here.")};
            }
            case ECkOptimizationDebugger_CleanupCategory::Duplicates:
            {
                // The conservative rule, verbatim, where the reader is standing. Content is never compared.
                return FString{TEXT("Assets that share a name, a class and a disk size across different folders. ")
                    TEXT("Possible duplicates only — nothing here is claimed to be byte-identical.")};
            }
            case ECkOptimizationDebugger_CleanupCategory::NameCollisions:
            {
                return FString{TEXT("Assets under /Game that answer to the SAME name in different folders, whatever ")
                    TEXT("their class or size. Not duplicates — the fix is a rename, never a delete. Anything that ")
                    TEXT("resolves an asset by short name alone (code, a generated script accessor, a search) has to ")
                    TEXT("pick one of them, and which one it picks is not something this project decides.")};
            }
            case ECkOptimizationDebugger_CleanupCategory::Redirectors:
            {
                return FString{TEXT("Object redirectors left behind by renames and moves. Fixing them up rewrites ")
                    TEXT("the referencing packages and then removes the redirector.")};
            }
            case ECkOptimizationDebugger_CleanupCategory::DirtyPackages:
            {
                return FString{TEXT("Packages with unsaved changes in this session. Live editor state, not registry ")
                    TEXT("data — it is only as current as the last scan.")};
            }
            default:
            {
                return FString{};
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DuplicateGroupKey(
            const FString& InDisplayName,
            const FString& InClassName,
            int64 InDiskSizeBytes)
        -> FString
    {
        // Lower-cased on both text halves: the Content Browser treats `Rock` and `rock` as the same name, so a match
        // that split on case would miss exactly the pair a reader is most likely to have created by accident.
        return ck::Format_UE(TEXT("{}|{}|{}"),
            InDisplayName.ToLower(),
            InClassName.ToLower(),
            InDiskSizeBytes);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_NameCollisionGroupKey(
            const FString& InDisplayName)
        -> FString
    {
        // The name alone. Class and size are what make a DUPLICATE possible and are irrelevant to whether two assets
        // answer to one name — folding them in here would split exactly the collisions worth reporting, because an
        // `SM_Rock` and a `T_Rock` differ in both and collide anyway.
        return InDisplayName.ToLower();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_CleanupSearchText(
            const FCkOptimizationDebugger_CleanupRow& InRow)
        -> FString
    {
        // The size is deliberately absent, exactly as it is from the memory haystack: `"64"` would match a 64-byte
        // asset, a 64-kilobyte one and one whose path contains the number, which is three questions.
        return ck::Format_UE(TEXT("{} {} {} {}"),
            InRow.DisplayName,
            InRow.AssetPath,
            InRow.ClassName,
            InRow.Detail);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Matches_CleanupFilter(
            const FCkOptimizationDebugger_CleanupRow& InRow,
            const FString& InFilter)
        -> bool
    {
        // Answered before the haystack is built, for the same reason `Matches_MemoryFilter` is.
        if (InFilter.IsEmpty())
        { return true; }

        return Passes_TextFilter(Build_CleanupSearchText(InRow), InFilter);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SortedCleanupRows(
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows,
            ECkOptimizationDebugger_CleanupCategory InCategory,
            const FString& InFilter)
        -> TArray<FCkOptimizationDebugger_CleanupRow>
    {
        auto Rows = TArray<FCkOptimizationDebugger_CleanupRow>{};

        for (const auto& Row : InRows)
        {
            if (Row.Category != InCategory)
            { continue; }

            if (NOT Matches_CleanupFilter(Row, InFilter))
            { continue; }

            Rows.Add(Row);
        }

        Rows.Sort([](const FCkOptimizationDebugger_CleanupRow& InLhs, const FCkOptimizationDebugger_CleanupRow& InRhs)
        {
            if (InLhs.DiskSizeBytes != InRhs.DiskSizeBytes)
            { return InLhs.DiskSizeBytes > InRhs.DiskSizeBytes; }

            // The path breaks every tie. A cleanup census is full of rows that weigh the same — a folder of dirty
            // packages reports zero bytes on every one of them — so without this the list would reorder itself
            // between two scans that found exactly the same thing.
            return InLhs.AssetPath.Compare(InRhs.AssetPath, ESearchCase::CaseSensitive) < 0;
        });

        return Rows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CleanupGroups(
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows,
            ECkOptimizationDebugger_CleanupCategory InCategory,
            const FString& InFilter)
        -> TArray<FCkOptimizationDebugger_CleanupGroup>
    {
        auto GroupsByKey = TMap<FString, FCkOptimizationDebugger_CleanupGroup>{};

        // A flat category has no group key on its rows, so it would come back empty rather than wrong — but empty is
        // still an answer to a question nobody should be asking, and the caller that asked it has a bug.
        if (NOT Get_IsGroupedCleanupCategory(InCategory))
        { return {}; }

        for (const auto& Row : InRows)
        {
            if (Row.Category != InCategory)
            { continue; }

            if (Row.GroupKey.IsEmpty())
            { continue; }

            if (NOT Matches_CleanupFilter(Row, InFilter))
            { continue; }

            auto& Group = GroupsByKey.FindOrAdd(Row.GroupKey);

            if (Group.GroupKey.IsEmpty())
            {
                Group.GroupKey = Row.GroupKey;
                Group.DisplayName = Row.DisplayName;

                // A DUPLICATE group's members share a class and a size by construction — that IS the match — so the
                // first member's are the group's. A NAME-COLLISION group's members share only the name: an `SM_Rock`
                // and a `T_Rock` legitimately differ in both. Copying the first member's class and size onto the
                // group there would print one member's facts as the group's, which is a claim about the other members
                // that nothing checked. Left empty and zero instead, and the header prints the member count.
                const auto MembersShareClassAndSize =
                    InCategory == ECkOptimizationDebugger_CleanupCategory::Duplicates;

                if (MembersShareClassAndSize)
                {
                    Group.ClassName = Row.ClassName;
                    Group.DiskSizeBytes = Row.DiskSizeBytes;
                }
            }

            Group.Rows.Add(Row);
        }

        auto Groups = TArray<FCkOptimizationDebugger_CleanupGroup>{};
        Groups.Reserve(GroupsByKey.Num());

        for (auto& Entry : GroupsByKey)
        {
            // A filter can narrow a pair down to one row, and one asset neither duplicates nor collides with anything
            // — showing it under either heading would be the search box inventing a finding.
            if (Entry.Value.Rows.Num() < 2)
            { continue; }

            Entry.Value.Rows.Sort([](const FCkOptimizationDebugger_CleanupRow& InLhs,
                                     const FCkOptimizationDebugger_CleanupRow& InRhs)
            {
                return InLhs.AssetPath.Compare(InRhs.AssetPath, ESearchCase::CaseSensitive) < 0;
            });

            Groups.Add(MoveTemp(Entry.Value));
        }

        Groups.Sort([](const FCkOptimizationDebugger_CleanupGroup& InLhs,
                       const FCkOptimizationDebugger_CleanupGroup& InRhs)
        {
            // What keeping ONE copy would free, which is the number the reader is weighing: every member but one.
            //
            // A NAME-COLLISION group carries no size — its members are unrelated assets that merely answer to one
            // name, so they do not share a size and renaming one frees nothing. `DiskSizeBytes` is therefore zero
            // there by construction, which collapses this weight to zero for every collision group and hands the
            // ordering to the member-count tie-break below. That is the right ordering for it: the group most worth
            // looking at is the one the most assets are fighting over.
            const auto LhsWeight = InLhs.DiskSizeBytes * static_cast<int64>(InLhs.Rows.Num() - 1);
            const auto RhsWeight = InRhs.DiskSizeBytes * static_cast<int64>(InRhs.Rows.Num() - 1);

            if (LhsWeight != RhsWeight)
            { return LhsWeight > RhsWeight; }

            if (InLhs.Rows.Num() != InRhs.Rows.Num())
            { return InLhs.Rows.Num() > InRhs.Rows.Num(); }

            // The key breaks every tie, and it is unique per group by construction — `TMap` iteration order follows
            // its hash layout, so without this two identical scans would print the groups in different orders.
            return InLhs.GroupKey.Compare(InRhs.GroupKey, ESearchCase::CaseSensitive) < 0;
        });

        return Groups;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CleanupTotals(
            const TArray<FCkOptimizationDebugger_CleanupRow>& InRows)
        -> FCkOptimizationDebugger_CleanupTotals
    {
        auto Totals = FCkOptimizationDebugger_CleanupTotals{};

        // Every category gets an entry up front, empty or not — a caller indexing by enum value must always find one.
        for (const auto Category : Get_AllCleanupCategories())
        {
            auto CategoryTotals = FCkOptimizationDebugger_CleanupCategoryTotals{};
            CategoryTotals.Category = Category;

            Totals.Categories.Add(CategoryTotals);
        }

        for (const auto& Row : InRows)
        {
            const auto CategoryIndex = static_cast<int32>(Row.Category);

            if (NOT Totals.Categories.IsValidIndex(CategoryIndex))
            { continue; }

            auto& CategoryTotals = Totals.Categories[CategoryIndex];

            ++CategoryTotals.RowCount;
            CategoryTotals.TotalBytes += Row.DiskSizeBytes;

            ++Totals.RowCount;

            if (Row.Category != ECkOptimizationDebugger_CleanupCategory::Unreferenced)
            { continue; }

            Totals.ReclaimableBytes += Row.DiskSizeBytes;
        }

        return Totals;
    }
}

// --------------------------------------------------------------------------------------------------------------------
