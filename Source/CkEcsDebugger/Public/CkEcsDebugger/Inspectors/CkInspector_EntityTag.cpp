#include "CkInspector_EntityTag.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEntityTag/CkEntityTag_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTag)

// =====================================================================================================================

auto FCkInspector_EntityTag::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Tag"));
}

auto FCkInspector_EntityTag::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return UCk_Utils_EntityTag_UE::Has_AnyTag(Entity);
}

auto FCkInspector_EntityTag::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildGrid(Entity, FString());
}

auto FCkInspector_EntityTag::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildGrid(Entity, InFilter);
}

auto FCkInspector_EntityTag::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================

auto FCkInspector_EntityTag::BuildGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    const auto Tags      = UCk_Utils_EntityTag_UE::Get_AllTags(Entity);
    const auto GtRoots   = UCk_Utils_EntityTag_UE::Get_AllTagsAsContainer(Entity);

    // Header — FName tag count
    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("FName Tag Count:")),
        Tags.Num(),
        ECk_Tone::Info,
        FText::FromString(TEXT("tags")));

    // The flattened FName set as chips. Same snapshot semantics as the rows they replace — the
    // previous per-tag ✓/✗ was checked against this very Get_AllTags snapshot, so it could only
    // ever report a removal, never an addition. Entity tags are set-once per store in practice, so
    // the set is structural: the panel's normal rebuild (re-selection / filter keystroke) is the
    // refresh point.
    if (NOT Tags.IsEmpty())
    {
        auto TagChips = TArray<FCkInspector_Chip>{};
        TagChips.Reserve(Tags.Num());

        for (const auto& T : Tags)
        {
            TagChips.Add(FCkInspector_Chip{ FText::FromName(T), ECk_Tone::Neutral });
        }

        Builder.AddChipsRow(FText::FromString(TEXT("FName Tags:")), TagChips);
    }

    // GameplayTag roots — explicit only (does NOT include parent-flattened FNames)
    if (NOT GtRoots.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("GameplayTag Roots:")),
            [N = GtRoots.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(N)); },
            CkStyle::Value_Numeric());

        auto RootChips = TArray<FCkInspector_Chip>{};
        RootChips.Reserve(GtRoots.Num());

        for (const auto& GT : GtRoots)
        {
            RootChips.Add(FCkInspector_Chip{ FText::FromName(GT.GetTagName()), ECk_Tone::Neutral });
        }

        Builder.AddChipsRow(FText::FromString(TEXT("Roots:")), RootChips);
    }

    return Builder.Build(Entity, InFilter);
}
