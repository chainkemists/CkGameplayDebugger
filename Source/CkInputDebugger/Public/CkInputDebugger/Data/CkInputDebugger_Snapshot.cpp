#include "CkInputDebugger/Data/CkInputDebugger_Snapshot.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Format/CkFormat.h"

#include "EnhancedInputSubsystems.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedPlayerInput.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"

#include "UObject/UObjectIterator.h"

// --------------------------------------------------------------------------------------------------------------------
// Formatting helpers
// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_debugger_detail
{
    static auto ValueTypeToString(EInputActionValueType InType) -> FString
    {
        switch (InType)
        {
            case EInputActionValueType::Boolean: return TEXT("bool");
            case EInputActionValueType::Axis1D:  return TEXT("Axis1D");
            case EInputActionValueType::Axis2D:  return TEXT("Axis2D");
            case EInputActionValueType::Axis3D:  return TEXT("Axis3D");
            default:                             return TEXT("?");
        }
    }

    static auto FormatActionValue(const FInputActionValue& InValue) -> FString
    {
        switch (InValue.GetValueType())
        {
            case EInputActionValueType::Boolean:
                return InValue.Get<bool>() ? TEXT("true") : TEXT("false");
            case EInputActionValueType::Axis1D:
                return ck::Format_UE(TEXT("{:.3f}"), InValue.Get<float>());
            case EInputActionValueType::Axis2D:
            {
                const auto V = InValue.Get<FVector2D>();
                return ck::Format_UE(TEXT("({:.3f}, {:.3f})"), V.X, V.Y);
            }
            case EInputActionValueType::Axis3D:
            {
                const auto V = InValue.Get<FVector>();
                return ck::Format_UE(TEXT("({:.3f}, {:.3f}, {:.3f})"), V.X, V.Y, V.Z);
            }
            default:
                return TEXT("?");
        }
    }

    static auto TriggerEventToString(ETriggerEvent InEvent) -> FString
    {
        switch (InEvent)
        {
            case ETriggerEvent::Triggered: return TEXT("Triggered");
            case ETriggerEvent::Started:   return TEXT("Started");
            case ETriggerEvent::Ongoing:   return TEXT("Ongoing");
            case ETriggerEvent::Canceled:  return TEXT("Canceled");
            case ETriggerEvent::Completed: return TEXT("Completed");
            case ETriggerEvent::None:      return TEXT("None");
            default:                       return TEXT("None");
        }
    }

    static auto TriggerEventToActivity(ETriggerEvent InEvent) -> ECkInputDebugger_ActionActivity
    {
        switch (InEvent)
        {
            case ETriggerEvent::Triggered:
            case ETriggerEvent::Started:   return ECkInputDebugger_ActionActivity::Active;
            case ETriggerEvent::Ongoing:   return ECkInputDebugger_ActionActivity::Ongoing;
            case ETriggerEvent::Canceled:  return ECkInputDebugger_ActionActivity::Canceled;
            case ETriggerEvent::Completed: return ECkInputDebugger_ActionActivity::Completed;
            default:                       return ECkInputDebugger_ActionActivity::Idle;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Gather (structural)
// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_Snapshot::
    Gather(
        UEnhancedInputLocalPlayerSubsystem* InSubsystem,
        const FString& InPlayerLabel)
    -> FCkInputDebugger_Snapshot
{
    using namespace ck_input_debugger_detail;

    auto Snapshot = FCkInputDebugger_Snapshot{};
    Snapshot.PlayerLabel = InPlayerLabel;

    if (ck::Is_NOT_Valid(InSubsystem))
    { return Snapshot; }

    Snapshot.HasPlayer = true;

    // ---- Applied mapping-context stack ----
    //
    // The applied-context map on UEnhancedPlayerInput is protected, so we discover
    // the stack through the public subsystem API: walk every loaded mapping context
    // (an applied IMC is necessarily loaded — the subsystem holds a hard ref) and
    // keep the ones HasMappingContext reports as applied, reading their priority out.

    for (TObjectIterator<UInputMappingContext> It; It; ++It)
    {
        auto* Imc = *It;

        if (ck::Is_NOT_Valid(Imc))
        { continue; }

        auto Priority = 0;

        if (NOT InSubsystem->HasMappingContext(Imc, Priority))
        { continue; }

        auto Row = FCkInputDebugger_ContextRow{};
        Row.Context     = Imc;
        Row.ContextName = Imc->GetName();
        Row.ContextPath = Imc->GetPathName();
        Row.Priority    = Priority;

        for (const auto& Mapping : Imc->GetMappings())
        {
            const auto* Action = Mapping.Action.Get();

            auto MRow = FCkInputDebugger_MappingRow{};
            MRow.Action        = Action;
            MRow.ActionName    = ck::IsValid(Action) ? Action->GetName() : FString(TEXT("<None>"));
            MRow.Key           = Mapping.Key;
            MRow.KeyName       = Mapping.Key.GetFName().ToString();
            MRow.ValueType     = ck::IsValid(Action) ? ValueTypeToString(Action->ValueType) : FString{};
            MRow.TriggerCount  = Mapping.Triggers.Num();
            MRow.ModifierCount = Mapping.Modifiers.Num();

            Row.Mappings.Add(MoveTemp(MRow));
        }

        Row.Mappings.Sort([](const FCkInputDebugger_MappingRow& A, const FCkInputDebugger_MappingRow& B)
        {
            if (A.ActionName != B.ActionName) { return A.ActionName < B.ActionName; }
            return A.KeyName < B.KeyName;
        });

        Snapshot.Contexts.Add(MoveTemp(Row));
    }

    // Stack order: highest priority on top. Tie-break by name for stable ordering.
    Snapshot.Contexts.Sort([](const FCkInputDebugger_ContextRow& A, const FCkInputDebugger_ContextRow& B)
    {
        if (A.Priority != B.Priority) { return A.Priority > B.Priority; }
        return A.ContextName < B.ContextName;
    });

    // ---- Resolved (flattened) action bindings ----
    //
    // Unique actions across the applied stack, with the keys the subsystem reports
    // as currently mapped to each (post priority-resolution) via the public query.

    for (const auto& Ctx : Snapshot.Contexts)
    {
        for (const auto& M : Ctx.Mappings)
        {
            const auto* Action = M.Action.Get();

            if (ck::Is_NOT_Valid(Action))
            { continue; }

            const auto AlreadyAdded = Snapshot.ResolvedActions.ContainsByPredicate(
                [Action](const FCkInputDebugger_ActionRow& InRow)
                {
                    return InRow.Action.Get() == Action;
                });

            if (AlreadyAdded)
            { continue; }

            auto ARow = FCkInputDebugger_ActionRow{};
            ARow.Action     = Action;
            ARow.ActionName = Action->GetName();
            ARow.ValueType  = ValueTypeToString(Action->ValueType);

            const auto Keys = InSubsystem->QueryKeysMappedToAction(Action);

            for (auto KeyIdx = 0; KeyIdx < Keys.Num(); ++KeyIdx)
            {
                if (KeyIdx > 0) { ARow.KeysJoined += TEXT(", "); }
                ARow.KeysJoined += Keys[KeyIdx].GetFName().ToString();
            }

            Snapshot.ResolvedActions.Add(MoveTemp(ARow));
        }
    }

    Snapshot.ResolvedActions.Sort([](const FCkInputDebugger_ActionRow& A, const FCkInputDebugger_ActionRow& B)
    {
        return A.ActionName < B.ActionName;
    });

    // ---- Structural signature ----

    auto Signature = FString{};
    Signature.Reserve(512);
    Signature += InPlayerLabel + TEXT("|");

    for (const auto& Ctx : Snapshot.Contexts)
    {
        Signature += ck::Format_UE(TEXT("C:{}#{}m{};"),
            Ctx.ContextName, Ctx.Priority, Ctx.Mappings.Num());

        for (const auto& M : Ctx.Mappings)
        {
            Signature += M.ActionName + TEXT("=") + M.KeyName + TEXT(",");
        }
    }

    Signature += TEXT("|R:");

    for (const auto& A : Snapshot.ResolvedActions)
    {
        Signature += A.ActionName + TEXT("[") + A.KeysJoined + TEXT("]");
    }

    Snapshot.StructuralSignature = MoveTemp(Signature);

    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
// Gather (live runtime state for a single action)
// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInputDebugger_Snapshot::
    Gather_LiveActionState(
        UEnhancedInputLocalPlayerSubsystem* InSubsystem,
        const UInputAction* InAction)
    -> FCkInputDebugger_LiveActionState
{
    using namespace ck_input_debugger_detail;

    auto State = FCkInputDebugger_LiveActionState{};

    if (ck::Is_NOT_Valid(InSubsystem) || ck::Is_NOT_Valid(InAction))
    { return State; }

    auto* PlayerInput = InSubsystem->GetPlayerInput();

    if (ck::Is_NOT_Valid(PlayerInput))
    { return State; }

    const auto* Instance = PlayerInput->FindActionInstanceData(InAction);

    if (Instance == nullptr)
    {
        State.Activity = ECkInputDebugger_ActionActivity::NoInstance;
        State.ValueText = TEXT("—");
        State.TriggerEventText = TEXT("—");
        return State;
    }

    const auto Value = Instance->GetValue();
    const auto Event = Instance->GetTriggerEvent();

    State.ValueText        = FormatActionValue(Value);
    State.Magnitude        = FMath::Sqrt(Value.GetMagnitudeSq());
    State.TriggerEventText = TriggerEventToString(Event);
    State.Activity         = TriggerEventToActivity(Event);
    State.ElapsedTime      = Instance->GetElapsedTime();

    return State;
}

// --------------------------------------------------------------------------------------------------------------------
