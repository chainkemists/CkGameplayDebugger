#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CkGameplayDebugger/Category/CkDebugger_Category_Data.h"

#include "CkDebugger_Submenu_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_GameplayDebugger_DebugSubmenu_ShowState : uint8
{
    Visible,
    Hidden
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_GameplayDebugger_DebugSubmenu_ShowState);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DrawSubmenuData_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_DrawSubmenuData_Params);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Ck|GameplayDebugger|Submenu", meta = (AllowPrivateAccess = true))
    TObjectPtr<AActor> _CurrentlySelectedDebugActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Ck|GameplayDebugger|Submenu", meta = (AllowPrivateAccess = true))
    FCk_Payload_GameplayDebugger_OnDrawData _DrawData;

public:
    CK_PROPERTY_GET(_CurrentlySelectedDebugActor);
    CK_PROPERTY_GET(_DrawData);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_DrawSubmenuData_Params, _CurrentlySelectedDebugActor, _DrawData);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKGAMEPLAYDEBUGGER_API FCk_GameplayDebugger_DrawSubmenuData_Result
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_GameplayDebugger_DrawSubmenuData_Result);

private:
    // 1 per line
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              Category = "Ck|GameplayDebugger|Submenu", meta = (AllowPrivateAccess = true))
    TArray<FText> _DebugDataToDraw;

public:
    CK_PROPERTY_GET(_DebugDataToDraw);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_GameplayDebugger_DrawSubmenuData_Result, _DebugDataToDraw);
};

// --------------------------------------------------------------------------------------------------------------------
