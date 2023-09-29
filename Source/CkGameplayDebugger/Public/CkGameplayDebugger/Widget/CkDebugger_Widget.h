#pragma once

#include "CkCore/Ensure/CkEnsure.h"
#include "CkGameplayDebugger/CkDebugger_Common.h"

#include <Blueprint/UserWidget.h>

#include "CkDebugger_Widget.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DebugWidget_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_DebugWidget_Params);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugNavControls _DebugNavControls;

public:
    CK_PROPERTY_GET(_DebugNavControls);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_DebugWidget_Params, _DebugNavControls);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(Abstract, BlueprintType, Blueprintable, meta = (DisableNativeTick))
class CKGAMEPLAYDEBUGGER_API UCk_GameplayDebugger_DebugWidget_UE : public UUserWidget
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_GameplayDebugger_DebugWidget_UE);

public:
    template <typename T_WidgetOwner>
    static auto Create(
        T_WidgetOwner* InWidgetOwner,
        TSubclassOf<ThisType> InDebugWidgetClass,
        const FCk_GameplayDebugger_DebugWidget_Params& InParams) -> ThisType*;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FCk_GameplayDebugger_DebugWidget_Params _Params;

public:
    CK_PROPERTY_GET(_Params);
};

// --------------------------------------------------------------------------------------------------------------------
// Definitions

template <typename T_WidgetOwner>
auto
    UCk_GameplayDebugger_DebugWidget_UE::
    Create(
        T_WidgetOwner* InWidgetOwner,
        TSubclassOf<ThisType> InDebugWidgetClass,
        const FCk_GameplayDebugger_DebugWidget_Params& InParams)
    -> ThisType*
{
    auto* createdWidget = Cast<ThisType>(CreateWidget(InWidgetOwner, InDebugWidgetClass));

    CK_ENSURE_IF_NOT(ck::IsValid(createdWidget), TEXT("Failed to create the Gameplay Debugger Debug Widget!"))
    { return {}; }

    createdWidget->_Params = InParams;
    return createdWidget;
}

// --------------------------------------------------------------------------------------------------------------------
