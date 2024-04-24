#pragma once

#include "CkCore/Macros/CkMacros.h"

#include "CogWindowConfig.h"

#include "CkAbilityOwnerTags_DebugConfig.generated.h"

//--------------------------------------------------------------------------------------------------------------------------

UCLASS(Config = CkEcsDebugger)
class UCk_AbilityOwnerTags_DebugWindowConfig : public UCogWindowConfig
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_AbilityOwnerTags_DebugWindowConfig);

public:
    auto Reset() -> void override;

public:
    UPROPERTY(Config)
    bool SortByName = true;

    // TODO: GroupBySource (EntityExtensions)
};

//--------------------------------------------------------------------------------------------------------------------------
