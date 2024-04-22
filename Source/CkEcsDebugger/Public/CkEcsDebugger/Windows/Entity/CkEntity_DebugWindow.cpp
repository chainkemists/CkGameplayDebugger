#include "CkEntity_DebugWindow.h"

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    Initialize()
    -> void
{
    Super::Initialize();
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    RenderHelp()
    -> void
{
    ImGui::Text("This window displays Entity basic info of the selected actor");
}

//--------------------------------------------------------------------------------------------------------------------------

auto
    FCk_EntityBasics_DebugWindow::
    RenderContent()
    -> void
{
    Super::RenderContent();

    if (const auto& SelectionEntity = Get_SelectionEntity(); ck::IsValid(SelectionEntity))
    {
        ImGui::Text(TCHAR_TO_ANSI(ck::Format(TEXT("Entity: {}"), SelectionEntity).c_str()));
    }
    else
    {
        ImGui::Text("Selection Actor is NOT Ecs Ready");
    }
}

// --------------------------------------------------------------------------------------------------------------------

