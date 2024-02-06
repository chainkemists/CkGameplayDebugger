#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkGameplayDebugger/CkDebugger_Common.h"

#include "CkGameplayDebugger/Category/CkDebugger_Category_Data.h"

#include "CkDebugger_Filter_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_GetSortedFilteredActors_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_GetSortedFilteredActors_Params);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_Payload_GameplayDebugger_OnDrawData _DrawData;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<AActor> _PreviouslySelectedActor;

public:
    CK_PROPERTY_GET(_DrawData);
    CK_PROPERTY(_PreviouslySelectedActor);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_GetSortedFilteredActors_Params, _DrawData);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_GatherAndFilterActors_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_GatherAndFilterActors_Params);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_Payload_GameplayDebugger_OnDrawData _DrawData;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    TWeakObjectPtr<AActor> _PreviouslySelectedActor;

public:
    CK_PROPERTY_GET(_DrawData);
    CK_PROPERTY(_PreviouslySelectedActor);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_GatherAndFilterActors_Params, _DrawData);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_GatherAndFilterActors_Result
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_GatherAndFilterActors_Result);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugActorList _FilteredDebugActors;

public:
    CK_PROPERTY_GET(_FilteredDebugActors);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_GatherAndFilterActors_Result, _FilteredDebugActors);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_SortFilteredActors_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_SortFilteredActors_Params);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_Payload_GameplayDebugger_OnDrawData _DrawData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugActorList _FilteredActors;

public:
    CK_PROPERTY_GET(_DrawData);
    CK_PROPERTY_GET(_FilteredActors);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_SortFilteredActors_Params, _DrawData, _FilteredActors);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_SortFilteredActors_Result
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_SortFilteredActors_Result);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugActorList _SortedFilteredActors;

public:
    CK_PROPERTY_GET(_SortedFilteredActors);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_SortFilteredActors_Result, _SortedFilteredActors);
};

// --------------------------------------------------------------------------------------------------------------------
