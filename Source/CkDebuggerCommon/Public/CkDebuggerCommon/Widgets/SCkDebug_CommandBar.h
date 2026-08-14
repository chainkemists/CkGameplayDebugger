#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

enum class ECkDebug_CommandBarLane : uint8
{
    Invalid,
    Primary,
    Context,
};

// One semantic cluster of feature-owned controls. Common owns only presentation;
// the supplied widget retains its existing callbacks, state, and lifetime rules.
struct CKDEBUGGERCOMMON_API FCkDebug_CommandGroup
{
    FCkDebug_CommandGroup() = default;

    FCkDebug_CommandGroup(
        FName InId,
        FText InAccessibleLabel,
        ECkDebug_CommandBarLane InLane,
        TSharedRef<SWidget> InContent);

    static auto Primary(FName InId, FText InAccessibleLabel, TSharedRef<SWidget> InContent)
        -> FCkDebug_CommandGroup;
    static auto Context(FName InId, FText InAccessibleLabel, TSharedRef<SWidget> InContent)
        -> FCkDebug_CommandGroup;

    auto IsValid() const -> bool;

    FName Id;
    FText AccessibleLabel;
    ECkDebug_CommandBarLane Lane = ECkDebug_CommandBarLane::Invalid;
    TSharedPtr<SWidget> Content;
};

// Pure, testable partition produced before any Slate hierarchy is published.
// Invalid or duplicate descriptors reject atomically and leave both lanes empty.
struct CKDEBUGGERCOMMON_API FCkDebug_CommandBarLayout
{
    TArray<int32> PrimaryGroupIndices;
    TArray<int32> ContextGroupIndices;

    auto Get_SeparatorCount() const -> int32;

    static auto TryBuild(
        const TArray<FCkDebug_CommandGroup>& InGroups,
        FCkDebug_CommandBarLayout& OutLayout)
        -> bool;
};

// ====================================================================================================================

/**
 * Canonical top surface for standalone debugger tabs.
 *
 * Frequent actions occupy the primary row. Context groups occupy a second row
 * only when present. Both lanes remain single-line and scroll horizontally at
 * constrained widths. Utilities remain at the stable trailing edge of the
 * primary row. The owning tab is the sole debugger identity surface.
 */
class CKDEBUGGERCOMMON_API SCkDebug_CommandBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_CommandBar)
    {}
        SLATE_ARGUMENT(TArray<FCkDebug_CommandGroup>, Groups)
        SLATE_NAMED_SLOT(FArguments, UtilityContent)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_Lane(const TArray<int32>& InGroupIndices) const -> TSharedRef<SWidget>;
    auto Build_Group(const FCkDebug_CommandGroup& InGroup, bool InHasLeadingSeparator) const
        -> TSharedRef<SWidget>;
    auto Build_Separator() const -> TSharedRef<SWidget>;

    TArray<FCkDebug_CommandGroup> _Groups;
    FCkDebug_CommandBarLayout _Layout;
};

// ====================================================================================================================
