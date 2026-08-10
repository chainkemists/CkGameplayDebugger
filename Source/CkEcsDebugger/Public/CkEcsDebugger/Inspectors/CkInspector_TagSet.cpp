#include "CkInspector_TagSet.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkTagSet/CkTagSet_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkEditorTools/Style/CkStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_TagSet)

// =====================================================================================================================

auto FCkInspector_TagSet::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Tag Set"));
}

auto FCkInspector_TagSet::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_TagSet_UE::Has(Entity);
}

auto FCkInspector_TagSet::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildTagSetGrid(Entity, FString());
}

auto FCkInspector_TagSet::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildTagSetGrid(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_TagSet::BuildTagSetGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    auto TagSetHandle = UCk_Utils_TagSet_UE::Cast(MutableEntity);
    if (ck::Is_NOT_Valid(TagSetHandle))
    {
        return Builder.Build(Entity, InFilter);
    }

    // Tag count header
    const auto NumTags = UCk_Utils_TagSet_UE::Get_NumTags(TagSetHandle);
    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Count:")),
        NumTags,
        ECk_Tone::Info,
        FText::FromString(TEXT("tags")));

    // The set as chips instead of one tick/cross row per tag. Those rows were never a live view of
    // the set: they were built from this same Get_Tags snapshot, so a tag ADDED after the build was
    // invisible and only a REMOVED one could ever flip to a cross. Chips lose that one half-signal
    // and gain a set that reads as a set. Refresh is the panel's normal rebuild - re-selection or a
    // filter keystroke (the panel deliberately ignores RequestRebuild from Tick,
    // CkDebuggerPanel_Inspector.cpp "POLICY").
    //
    // Full tag paths (not shortened): the chips row's filter value is the concatenated chip text, so
    // typing any segment of a tag still keeps this row through the inspector filter.
    const auto Tags = UCk_Utils_TagSet_UE::Get_Tags(TagSetHandle);

    auto Chips = TArray<FCkInspector_Chip>{};
    Chips.Reserve(Tags.Num());

    for (const auto& Tag : Tags)
    {
        Chips.Add(FCkInspector_Chip{ FText::FromString(Tag.ToString()), ECk_Tone::Neutral });
    }

    if (NOT Chips.IsEmpty())
    {
        Builder.AddChipsRow(FText::FromString(TEXT("Tags:")), Chips);
    }

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_TagSet::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed
}
