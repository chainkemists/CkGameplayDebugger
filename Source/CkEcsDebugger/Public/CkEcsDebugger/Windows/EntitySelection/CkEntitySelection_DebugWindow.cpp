#include "CkEntitySelection_DebugWindow.h"

#include "CkEcs/OwningActor/CkOwningActor_Utils.h"

#include "CkEcsDebugger/Subsystem/CkEcsDebugger_Subsystem.h"
#include "CkEcsDebugger/Windows/CkEcs_DebugWindowWidgets.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();

    bHasMenu = true;

    Config = GetConfig<UCk_DebugWindowConfig_EntitySelection>();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window allows picking an Entity for other windows to inspect");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntitySelection_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Only Root Entities", &Config->OnlyRootEntities);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    FCkEcs_DebugWindowWidgets::EntitiesListWithFilters(*GetWorld(), Config->OnlyRootEntities, &Filter, nullptr);
}

auto
    FCk_EntitySelection_DebugWindow::
    RenderTick(
        float InDeltaT)
    -> void
{
    FCk_Ecs_DebugWindow::RenderTick(InDeltaT);
}

// --------------------------------------------------------------------------------------------------------------------

