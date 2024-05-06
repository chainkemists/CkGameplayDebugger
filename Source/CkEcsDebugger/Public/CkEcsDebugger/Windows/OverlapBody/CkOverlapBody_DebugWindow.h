#pragma once

#include "CkOverlapBody_DebugConfig.h"

#include "CkEcsDebugger/Windows/CkEcs_DebugWindow.h"

#include "CkOverlapBody/Sensor/CkSensor_Fragment_Data.h"
#include "CkOverlapBody/Sensor/CkSensor_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

struct FCk_Handle_Marker;

// --------------------------------------------------------------------------------------------------------------------

template <typename T_HandleType, typename T_UtilsType>
class FCk_OverlapBody_DebugWindow : public FCk_Ecs_DebugWindow
{
    using Super = FCk_Ecs_DebugWindow;

public:
    auto
    Initialize() -> void override;

protected:
    auto
    RenderHelp() -> void override;

    auto
    RenderContent() -> void override;

    auto
    RenderMenu() -> void;

    virtual auto
    Get_Title() -> FName = 0;

    virtual auto
    Get_NumOverlaps(
        const T_HandleType&) -> FName
    { return TEXT("X"); };

    virtual auto
    Get_NumWorldOverlaps(
        const T_HandleType&) -> FName
    { return TEXT("X"); };

private:
    auto
    RenderTable(
        const FCk_Handle& InMarkerOwner) -> void;

private:
    ImGuiTextFilter _Filter;
    TObjectPtr<UCk_OverlapBody_DebugWindowConfig> _Config;
};

// --------------------------------------------------------------------------------------------------------------------

class FCk_Marker_DebugWindow : public FCk_OverlapBody_DebugWindow<FCk_Handle_Marker, UCk_Utils_Marker_UE>
{
protected:
    virtual auto
    Get_Title() -> FName override;
};

// --------------------------------------------------------------------------------------------------------------------

class FCk_Sensor_DebugWindow : public FCk_OverlapBody_DebugWindow<FCk_Handle_Sensor, UCk_Utils_Sensor_UE>
{
protected:
    virtual auto
    Get_Title() -> FName override;

    virtual auto
    Get_NumOverlaps(
        const FCk_Handle_Sensor& InSensor) -> FName override;

    virtual auto
    Get_NumWorldOverlaps(
        const FCk_Handle_Sensor& InSensor) -> FName override;
};

// --------------------------------------------------------------------------------------------------------------------

