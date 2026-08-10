#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_FocusCardBudget.h"

namespace
{
    auto IsProtectedProvider(const FGameplayTag& InTag) -> bool
    {
        const auto Name = InTag.ToString();
        return Name.Contains(TEXT(".Goap")) || Name.Contains(TEXT(".Crowd")) ||
               Name.Contains(TEXT(".PathNetwork")) || Name.Contains(TEXT(".AStar"));
    }

    auto IsGenericAttributeProvider(const FGameplayTag& InTag) -> bool
    {
        return InTag.ToString().Contains(TEXT("Attributes"));
    }
}

auto ck_debugoverlay::Sort_SectionsForBudget(
    TArray<FCk_DebugOverlay_Section>& InOutSections) -> void
{
    InOutSections.Sort([](const FCk_DebugOverlay_Section& A, const FCk_DebugOverlay_Section& B)
    {
        const auto AProtected = IsProtectedProvider(A.ProviderTag);
        const auto BProtected = IsProtectedProvider(B.ProviderTag);
        if (AProtected != BProtected) { return AProtected; }
        const auto AGeneric = IsGenericAttributeProvider(A.ProviderTag);
        const auto BGeneric = IsGenericAttributeProvider(B.ProviderTag);
        if (AGeneric != BGeneric) { return !AGeneric; }
        if (A.SortPriority != B.SortPriority) { return A.SortPriority < B.SortPriority; }
        return A.SourceOrder < B.SourceOrder;
    });
}

auto ck_debugoverlay::Apply_FocusCardBudget(
    const FCk_DebugOverlay_EntityModel& InModel,
    const FCk_DebugOverlay_FocusCardBudget& InBudget) -> FCk_DebugOverlay_EntityModel
{
    auto Result = InModel;
    Sort_SectionsForBudget(Result.Sections);

    int32 Remaining = FMath::Max(0, InBudget.MaxRowsTotal);
    for (auto& Section : Result.Sections)
    {
        // ACCUMULATE, never reset: an earlier pre-render trim (distance LOD) may already have
        // shed rows from this section, and both counts belong in the same "+N more".
        const auto Allowed = FMath::Min(FMath::Max(0, InBudget.MaxRowsPerSection), Remaining);
        if (Section.Rows.Num() > Allowed)
        {
            Section.OmittedRowCount += Section.Rows.Num() - Allowed;
            Section.Rows.SetNum(Allowed);
        }
        Remaining -= Section.Rows.Num();
    }
    // Fully displaced sections are represented by the card-wide omission summary.
    // Keeping an empty section solely for its local summary would let a many-provider
    // entity overrun the plate even though the data-row budget was exhausted.
    Result.Sections.RemoveAll([](const FCk_DebugOverlay_Section& Section)
    { return Section.Rows.IsEmpty(); });
    return Result;
}
