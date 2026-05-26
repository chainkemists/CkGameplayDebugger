#include "CkInspector_EntityTag.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEntityTag/CkEntityTag_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityTag)

static const FLinearColor EntityTag_Color_Tag      = FLinearColor(0.6f, 0.85f, 0.55f);
static const FLinearColor EntityTag_Color_TagCount = FLinearColor(0.5f, 0.7f, 0.45f);

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
    Builder.AddRow(
        FText::FromString(TEXT("FName Tag Count:")),
        [N = Tags.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(N)); },
        EntityTag_Color_TagCount);

    // One row per FName tag — live presence check
    for (const auto& T : Tags)
    {
        const auto CapturedTag = T;
        Builder.AddConditionalRow(
            FText::FromName(T),
            [CapturedTag](const FCk_Handle& E)
            {
                return FText::FromString(
                    UCk_Utils_EntityTag_UE::Has(E, CapturedTag)
                        ? TEXT("✓")   // checkmark
                        : TEXT("✗")); // cross
            },
            [CapturedTag](const FCk_Handle& E)
            {
                return UCk_Utils_EntityTag_UE::Has(E, CapturedTag)
                    ? EntityTag_Color_Tag : CkDebugStyle::Err();
            });
    }

    // GameplayTag roots — explicit only (does NOT include parent-flattened FNames)
    if (NOT GtRoots.IsEmpty())
    {
        Builder.AddRow(
            FText::FromString(TEXT("GameplayTag Roots:")),
            [N = GtRoots.Num()](const FCk_Handle&) { return FText::FromString(FString::FromInt(N)); },
            EntityTag_Color_TagCount);

        for (const auto& GT : GtRoots)
        {
            Builder.AddRow(
                FText::FromName(GT.GetTagName()),
                [](const FCk_Handle&) { return FText::FromString(TEXT("✓")); },
                EntityTag_Color_Tag);
        }
    }

    return Builder.Build(Entity, InFilter);
}
