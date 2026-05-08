#include "CkInspector_InteractTarget.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
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
    _TargetStates.Reset();

    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [this, &Sections](FCk_Handle_InteractTarget InTarget)
        {
            if (ck::Is_NOT_Valid(InTarget))
            { return; }

            const auto& Channel = UCk_Utils_InteractTarget_UE::Get_InteractionChannel(InTarget);
            const auto SectionName = Channel.IsValid()
                ? Channel.GetTagName().ToString()
                : TEXT("Unknown Channel");

            auto& State = _TargetStates.AddDefaulted_GetRef();
            State.Handle = InTarget;

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
        CkDebugStyle::Value_Tag());

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
            if (ck::Is_NOT_Valid(CapturedTarget)) { return CkDebugStyle::None(); }
            const auto Enabled = UCk_Utils_InteractTarget_UE::Get_Enabled(CapturedTarget);
            return Enabled == ECk_EnableDisable::Enable
                ? CkDebugStyle::Status_Active()
                : CkDebugStyle::Status_Failed();
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
        CkDebugStyle::Value_Enum());

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
            if (ck::Is_NOT_Valid(CapturedTarget)) { return CkDebugStyle::None(); }
            const auto Policy = UCk_Utils_InteractTarget_UE::Get_InteractionCompletionPolicy(CapturedTarget);
            return Policy == ECk_Interaction_CompletionPolicy::Timed
                ? CkDebugStyle::Value_Numeric()
                : CkDebugStyle::None();
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
        CkDebugStyle::Value_Enum());

    // Current Interactions — live badge box
    {
        auto MutableTarget = InTarget;
        auto InteractionHandles = TArray<FCk_Handle>{};

        const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);
        for (const auto& Interaction : Interactions)
        {
            if (ck::IsValid(Interaction))
            {
                InteractionHandles.Add(Interaction);
            }
        }

        // Find the matching target state and store the badge box
        for (auto& State : _TargetStates)
        {
            if (State.Handle == InTarget)
            {
                State.LastInteractionCount = InteractionHandles.Num();
                State.InteractionsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(InteractionHandles);
                Builder.AddWidgetRow(FText::FromString(TEXT("Interactions:")), State.InteractionsBox.ToSharedRef());
                break;
            }
        }
    }

    return Builder.Build(InTarget, FString());
}

// =====================================================================================================================

auto FCkInspector_InteractTarget::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT Entity.Has<ck::FFragment_RecordOfInteractTargets>())
    { return; }

    // ---- Check for structural changes (target count) ----

    auto CurrentCount = int32{0};
    UCk_Utils_InteractTarget_UE::ForEach_InteractTarget(Entity,
        [&CurrentCount](FCk_Handle_InteractTarget InTarget) { CurrentCount++; });

    if (CurrentCount != _LastTargetCount)
    {
        RequestRebuild();
        return;
    }

    // ---- In-place update: refresh interaction badge boxes ----

    for (auto& State : _TargetStates)
    {
        if (ck::Is_NOT_Valid(State.Handle) || NOT State.InteractionsBox.IsValid())
        { continue; }

        auto MutableTarget = State.Handle;
        const auto Interactions = UCk_Utils_InteractTarget_UE::Get_CurrentInteractions(MutableTarget);

        if (Interactions.Num() != State.LastInteractionCount)
        {
            State.LastInteractionCount = Interactions.Num();

            auto InteractionHandles = TArray<FCk_Handle>{};
            for (const auto& Interaction : Interactions)
            {
                if (ck::IsValid(Interaction))
                {
                    InteractionHandles.Add(Interaction);
                }
            }
            FCkInspectorWidgetBuilder::PopulateBadgeBox(*State.InteractionsBox, InteractionHandles);
        }
    }
}
