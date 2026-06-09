#pragma once

#include "CkCore/Types/DataAsset/CkDataAsset.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"

#include "CkDebugOverlay_LayoutAsset.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType)
class CKENTITYDEBUGOVERLAY_API UCk_DebugOverlay_Layout_PDA : public UCk_DataAsset_PDA
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_DebugOverlay_Layout_PDA);

private:
    UPROPERTY(EditDefaultsOnly, meta=(AllowPrivateAccess=true))
    FCk_DebugOverlay_Layout _Layout;

public:
    CK_PROPERTY_GET(_Layout);
};

// --------------------------------------------------------------------------------------------------------------------
