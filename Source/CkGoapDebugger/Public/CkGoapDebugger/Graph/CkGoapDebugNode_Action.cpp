#if WITH_EDITOR

#include "CkGoapDebugNode_Action.h"

// ====================================================================================================================

auto
    UCkGoapDebugNode_Action::
    AllocateDefaultPins()
    -> void
{
    // One input pin per Precondition, named by the tag so the connection
    // policy can match producers/consumers by key during graph rebuild.
    for (const auto& Pre : _Snapshot.Preconditions)
    {
        const auto PinName = FName(*Pre.Key.ToString());
        CreatePin(EGPD_Input, TEXT("GoapCondition"), PinName);
    }

    // One output pin per Effect.
    for (const auto& Eff : _Snapshot.Effects)
    {
        const auto PinName = FName(*Eff.Key.ToString());
        CreatePin(EGPD_Output, TEXT("GoapCondition"), PinName);
    }

    // Always provide a fallback default input/output pin so the node has
    // at least one pin per side — required by SGraphEditor's geometry
    // resolution when a node has no preconditions or no effects.
    if (_Snapshot.Preconditions.Num() == 0)
    { CreatePin(EGPD_Input, TEXT("GoapDefault"), NAME_None); }

    if (_Snapshot.Effects.Num() == 0)
    { CreatePin(EGPD_Output, TEXT("GoapDefault"), NAME_None); }

    // Dedicated tree pins for parent → child edges (U11.7-D). Hidden from the
    // Slate visual (SGraphNode_GoapAction only renders a single anchor pin
    // and the connection policy resolves geometry node-to-node), but the
    // connection policy detects them via Get_TreePinNames so it can apply
    // dashed wire styling distinct from dependency edges.
    CreatePin(EGPD_Input,  TEXT("GoapTree"), GetTreeInPinName())->bHidden  = true;
    CreatePin(EGPD_Output, TEXT("GoapTree"), GetTreeOutPinName())->bHidden = true;
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Action::
    Get_TreeInPin() const
    -> UEdGraphPin*
{
    const auto Target = GetTreeInPinName();
    for (auto* Pin : Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == Target)
        { return Pin; }
    }
    return nullptr;
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Action::
    Get_TreeOutPin() const
    -> UEdGraphPin*
{
    const auto Target = GetTreeOutPinName();
    for (auto* Pin : Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && Pin->PinName == Target)
        { return Pin; }
    }
    return nullptr;
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Action::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(_Snapshot.ClassName);
}

// ====================================================================================================================

auto
    UCkGoapDebugNode_Action::
    PopulateFromActionInfo(
        const FCkGoapDebugger_ActionInfo& InAction)
    -> void
{
    _Snapshot = InAction;
}

// ====================================================================================================================

#endif
