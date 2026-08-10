#include "CkInspector_TagSet.h"

#include "CkCore/Format/CkFormat.h"
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
    Builder.SetEditGuard(Get_EditGuard());

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

    // ---- Write surface ----
    //
    // AUTHORITY NOTE — the Utils are INCONSISTENT here: Request_RemoveTag/Request_RemoveTags carry
    // BlueprintAuthorityOnly (CkTagSet_Utils.h:178,189) while Request_AddTag/Request_AddTags sit under
    // the same "Requests (Authority Only)" section header WITHOUT the specifier (:157,:167). The
    // section header is the stated contract, so both are gated AuthorityOnly here — the conservative
    // read. If Add is genuinely meant to be client-callable, the specifier is what should change.
    const auto CapturedTagSet = TagSetHandle;

    Builder.AddTagEntryRow(
        FText::FromString(TEXT("Add Tag:")),
        TAttribute<FText>{},
        [CapturedTagSet](FGameplayTag InTag)
        {
            auto MutableTagSet = CapturedTagSet;
            if (ck::Is_NOT_Valid(MutableTagSet)) { return; }
            UCk_Utils_TagSet_UE::Request_AddTag(MutableTagSet, InTag, {});
        },
        ECk_DebugRequest_Requirement::AuthorityOnly);

    // One remove button per listed tag, from the SAME Get_Tags snapshot the chips row above is built
    // from — so the two always agree, and both refresh on the panel's normal rebuild.
    for (const auto& Tag : Tags)
    {
        Builder.AddActionRow(
            FText::FromString(Tag.ToString()),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Remove")),
                    FText::FromString(ck::Format_UE(TEXT("UCk_Utils_TagSet_UE::Request_RemoveTag({})"), Tag.ToString())),
                    [CapturedTagSet, Tag]()
                    {
                        auto MutableTagSet = CapturedTagSet;
                        if (ck::Is_NOT_Valid(MutableTagSet)) { return; }
                        UCk_Utils_TagSet_UE::Request_RemoveTag(MutableTagSet, Tag, {});
                    },
                    ECk_DebugRequest_Requirement::AuthorityOnly
                },
            });
    }

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_TagSet::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed
}
