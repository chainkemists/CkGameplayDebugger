#include "CkInspector_TagSet.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkTagSet/CkTagSet_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_TagSet)

static const FLinearColor Color_Tag = FLinearColor(0.6f, 0.85f, 0.55f);
static const FLinearColor Color_TagCount = FLinearColor(0.5f, 0.7f, 0.45f);

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
    Builder.AddRow(
        FText::FromString(TEXT("Count:")),
        [NumTags](const FCk_Handle& E) { return FText::FromString(ck::Format_UE(TEXT("{}"), NumTags)); },
        Color_TagCount);

    // Individual tags
    const auto Tags = UCk_Utils_TagSet_UE::Get_Tags(TagSetHandle);
    for (const auto& Tag : Tags)
    {
        const auto TagName = Tag.ToString();
        const auto CapturedTag = Tag;

        Builder.AddConditionalRow(
            FText::FromString(TagName),
            [CapturedTag](const FCk_Handle& E)
            {
                // Show presence check as live value
                auto MutableE = E;
                const auto TS = UCk_Utils_TagSet_UE::Cast(MutableE);
                if (ck::Is_NOT_Valid(TS)) { return FText::FromString(TEXT("--")); }
                return FText::FromString(
                    UCk_Utils_TagSet_UE::HasTagExact(TS, CapturedTag)
                        ? TEXT("\u2713")   // checkmark
                        : TEXT("\u2717")); // cross
            },
            [CapturedTag](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto TS = UCk_Utils_TagSet_UE::Cast(MutableE);
                if (ck::Is_NOT_Valid(TS)) { return CkDebugStyle::None(); }
                return UCk_Utils_TagSet_UE::HasTagExact(TS, CapturedTag)
                    ? Color_Tag : CkDebugStyle::Err();
            });
    }

    return Builder.Build(Entity, InFilter);
}

// =====================================================================================================================

auto FCkInspector_TagSet::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed
}
