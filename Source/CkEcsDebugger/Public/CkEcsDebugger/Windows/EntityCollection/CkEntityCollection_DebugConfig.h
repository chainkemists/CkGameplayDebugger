#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CogCommonConfig.h"

#include "CkEntityCollection_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_EntityCollection_DebugWindowConfig : public UCogCommonConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_EntityCollection_DebugWindowConfig);

public:
    auto Reset() -> void override;

public:
    UPROPERTY(Config)
    bool DisplayContentOnSingleLine = false;

    UPROPERTY(Config)
    bool DisplayContentFullName = true;

    UPROPERTY(Config)
    bool RenderCollectionsWithNoContent = true;
};

//--------------------------------------------------------------------------------------------------------------------------
