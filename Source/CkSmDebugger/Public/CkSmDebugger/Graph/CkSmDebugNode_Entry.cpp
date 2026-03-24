#include "CkSmDebugNode_Entry.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Entry::
    AllocateDefaultPins()
    -> void
{
    CreatePin(EGPD_Output, TEXT("Transition"), TEXT("Out"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkSmDebugNode_Entry::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(TEXT("Entry"));
}

// --------------------------------------------------------------------------------------------------------------------
