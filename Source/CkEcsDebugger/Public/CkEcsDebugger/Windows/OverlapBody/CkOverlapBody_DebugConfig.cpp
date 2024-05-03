#include "CkOverlapBody_DebugConfig.h"

//--------------------------------------------------------------------------------------------------------------------------
void
    UCk_OverlapBody_DebugWindowConfig::Reset()
{
    Super::Reset();

    SortByName = false;
    ShowActive = true;
    ShowInactive = true;

    ActiveColor = FVector4f{0.0f, 1.0f, 0.5f, 1.0f};
    InactiveColor = FVector4f{1.0f, 1.0f, 1.0f, 1.0f};
}
