#if WITH_EDITOR

#include "CkEcsDebugNode_Entity.h"

#include "EdGraph/EdGraphPin.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugNode_Entity::
    AllocateDefaultPins()
    -> void
{
    CreatePin(EGPD_Input, TEXT("Entity"), TEXT("In"));
    CreatePin(EGPD_Output, TEXT("Entity"), TEXT("Out"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugNode_Entity::
    GetNodeTitle(
        ENodeTitleType::Type InTitleType) const
    -> FText
{
    return FText::FromString(_DisplayName);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCkEcsDebugNode_Entity::
    Populate(
        const FCk_Handle& InEntity,
        const FString& InDisplayName,
        const FLinearColor& InAccentColor,
        bool InIsCenterNode,
        ECkEcsDebugEdgeType InEdgeType)
    -> void
{
    _Entity = InEntity;
    _DisplayName = InDisplayName;
    _AccentColor = InAccentColor;
    _IsCenterNode = InIsCenterNode;
    _EdgeType = InEdgeType;
}

// --------------------------------------------------------------------------------------------------------------------

#endif
