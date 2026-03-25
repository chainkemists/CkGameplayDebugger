#include "CkSmDebugNode_Compound.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Compound::
    AllocateDefaultPins()
    -> void
{
    CreatePin(EGPD_Input, TEXT("Transition"), TEXT("CompoundIn"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Compound::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(_CompoundLabel);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Compound::
    Populate(
        int32 InParentStateIndex,
        const FString& InLabel,
        float InWidth,
        float InHeight,
        const TArray<int32>& InSubSmStateIndices)
    -> void
{
    _ParentStateIndex = InParentStateIndex;
    _CompoundLabel = InLabel;
    _CompoundWidth = InWidth;
    _CompoundHeight = InHeight;
    _SubSmStateIndices = InSubSmStateIndices;
}

// --------------------------------------------------------------------------------------------------------------------
