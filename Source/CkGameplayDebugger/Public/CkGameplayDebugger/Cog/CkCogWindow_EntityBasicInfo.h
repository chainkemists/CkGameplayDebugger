#pragma once

#include "CoreMinimal.h"
#include "CogCommon.h"

#include "CogWindow.h"

#include "CkCogWindow_EntityBasicInfo.generated.h"

// --------------------------------------------------------------------------------------------------------------------

class FCkCogWindow_EntityBasicInfo : public FCogWindow
{
    typedef FCogWindow Super;

public:

    auto
    Initialize() -> void override;

protected:

    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(BlueprintType, Blueprintable)
class CKGAMEPLAYDEBUGGER_API ACkCogWindowManager : public AActor
{
    GENERATED_BODY()

protected:
    auto
    BeginPlay() -> void override;

public:
    auto
    Tick(
        float DeltaSeconds) -> void override;

private:
    UPROPERTY()
    TObjectPtr<UObject> _CogWindowManagerRef = nullptr;

#if ENABLE_COG
    TObjectPtr<UCogWindowManager> _CogWindowManager = nullptr;
#endif //ENABLE_COG
};
