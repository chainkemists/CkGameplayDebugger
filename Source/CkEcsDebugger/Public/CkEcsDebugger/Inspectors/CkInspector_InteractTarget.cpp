#include "CkInspector_InteractTarget.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_InteractTarget)

// =====================================================================================================================

auto FCkInspector_InteractTarget::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Interact Targets"));
}

auto FCkInspector_InteractTarget::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && Entity.Has<ck::FFragment_RecordOfInteractTargets>();
}

auto FCkInspector_InteractTarget::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return SNullWidget::NullWidget;
}

// =====================================================================================================================

auto FCkInspector_InteractTarget::Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection>
{
    auto Sections = TArray<FInspectorSection>{};

    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [this, &Sections](FCk_Handle_InteractTarget InTarget)
        {
            if (ck::Is_NOT_Valid(InTarget))
            { return; }

            const auto& Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget);
            const auto SectionName = Channel.IsValid()
                ? Channel.GetTagName().ToString()
                : TEXT("Unknown Channel");

            Sections.Add(FInspectorSection
            {
                FText::FromString(SectionName),
                BuildTargetWidget(InTarget)
            });
        });

    _LastTargetCount = Sections.Num();

    return Sections;
}

// =====================================================================================================================

auto FCkInspector_InteractTarget::BuildTargetWidget(const FCk_Handle_InteractTarget& InTarget) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    const auto CapturedTarget = InTarget;

    // Channel
    Builder.AddRow(
        FText::FromString(TEXT("Channel:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            const auto& Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(CapturedTarget);
            return FText::FromString(Channel.IsValid() ? Channel.GetTagName().ToString() : TEXT("None"));
        },
        FCkDebuggerStyle::Color_Value_Tag);

    // Enabled
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Enabled:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            const auto Enabled = UCk_Utils_InteractTarget_UE::Get_Enabled(CapturedTarget);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Enabled));
        },
        [CapturedTarget](const FCk_Handle& E) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FCkDebuggerStyle::Color_None; }
            const auto Enabled = UCk_Utils_InteractTarget_UE::Get_Enabled(CapturedTarget);
            return Enabled == ECk_EnableDisable::Enable
                ? FCkDebuggerStyle::Color_Status_Active
                : FCkDebuggerStyle::Color_Status_Failed;
        });

    // Completion Policy
    Builder.AddRow(
        FText::FromString(TEXT("Completion:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            const auto Policy = UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(CapturedTarget);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Policy));
        },
        FCkDebuggerStyle::Color_Value_Enum);

    // Duration (only meaningful when Timed)
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Duration:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            const auto Policy = UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(CapturedTarget);
            if (Policy != ECk_Interaction_CompletionPolicy::Timed)
            { return FText::FromString(TEXT("N/A")); }
            const auto Duration = UCk_Utils_InteractTarget_UE::Get_InteractionDuration(CapturedTarget);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Duration));
        },
        [CapturedTarget](const FCk_Handle& E) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FCkDebuggerStyle::Color_None; }
            const auto Policy = UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(CapturedTarget);
            return Policy == ECk_Interaction_CompletionPolicy::Timed
                ? FCkDebuggerStyle::Color_Value_Numeric
                : FCkDebuggerStyle::Color_None;
        });

    // Concurrent Policy
    Builder.AddRow(
        FText::FromString(TEXT("Concurrent:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            const auto Policy = UCk_Utils_InteractTarget_UE::Get_ConcurrentInteractionsPolicy(CapturedTarget);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Policy));
        },
        FCkDebuggerStyle::Color_Value_Enum);

    // Current Interactions count
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Interactions:")),
        [CapturedTarget](const FCk_Handle& E)
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FText::FromString(TEXT("--")); }
            auto MutableTarget = CapturedTarget;
            const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);
            return FText::FromString(ck::Format_UE(TEXT("{}"), Interactions.Num()));
        },
        [CapturedTarget](const FCk_Handle& E) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedTarget)) { return FCkDebuggerStyle::Color_None; }
            auto MutableTarget = CapturedTarget;
            const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);
            return Interactions.Num() > 0
                ? FCkDebuggerStyle::Color_Status_Active
                : FCkDebuggerStyle::Color_Text_Secondary;
        });

    return Builder.Build(InTarget, FString());
}

// =====================================================================================================================

auto FCkInspector_InteractTarget::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT Entity.Has<ck::FFragment_RecordOfInteractTargets>())
    { return; }

    auto CurrentCount = int32{ 0 };
    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [&CurrentCount](FCk_Handle_InteractTarget InTarget) { CurrentCount++; });

    if (CurrentCount != _LastTargetCount)
    {
        RequestRebuild();
    }
}
