#include "CkSaveDebugger_Visualizer.h"

#if WITH_EDITOR

#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"
#include "CkCore/Macros/CkMacros.h"

#include "Editor.h"
#include "EditorModeManager.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_visualizer
{
    const FEditorModeID VisualizerModeId = TEXT("Ck.SaveDebugger.Visualizer");

    auto GRows = ck::save_debugger_viz::FRowsPtr{};
    auto GSelectedSavedId = ck::snapshot::k_NoSavedEntity;
    auto GOnRowClicked = ck::save_debugger_viz::FOnRowClicked{};
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::save_debugger_viz
{
    auto
        Publish_Rows(
            const FRowsPtr& InRows)
        -> void
    {
        ck_save_debugger_visualizer::GRows = InRows;
        UCk_Utils_EditorOnly_UE::Request_RedrawLevelEditingViewports();
    }

    auto
        Clear_Rows()
        -> void
    {
        ck_save_debugger_visualizer::GRows.Reset();
        ck_save_debugger_visualizer::GSelectedSavedId = ck::snapshot::k_NoSavedEntity;
        UCk_Utils_EditorOnly_UE::Request_RedrawLevelEditingViewports();
    }

    auto
        Get_Rows()
        -> FRowsPtr
    {
        return ck_save_debugger_visualizer::GRows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Set_SelectedSavedId(
            uint32 InSavedId)
        -> void
    {
        if (ck_save_debugger_visualizer::GSelectedSavedId == InSavedId)
        { return; }

        ck_save_debugger_visualizer::GSelectedSavedId = InSavedId;
        UCk_Utils_EditorOnly_UE::Request_RedrawLevelEditingViewports();
    }

    auto
        Get_SelectedSavedId()
        -> uint32
    {
        return ck_save_debugger_visualizer::GSelectedSavedId;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Register_OnRowClicked(
            FOnRowClicked InHandler)
        -> void
    {
        ck_save_debugger_visualizer::GOnRowClicked = MoveTemp(InHandler);
    }

    auto
        Unregister_OnRowClicked()
        -> void
    {
        ck_save_debugger_visualizer::GOnRowClicked = {};
    }

    auto
        Notify_RowClicked(
            uint32 InSavedId)
        -> void
    {
        if (ck_save_debugger_visualizer::GOnRowClicked)
        { ck_save_debugger_visualizer::GOnRowClicked(InSavedId); }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Set_VisualizerEnabled(
            bool InEnabled)
        -> bool
    {
        if (GEditor == nullptr || GEditor->PlayWorld != nullptr)
        { return false; }

        auto& ModeTools = GLevelEditorModeTools();

        if (InEnabled)
        { ModeTools.ActivateMode(ck_save_debugger_visualizer::VisualizerModeId); }
        else
        { ModeTools.DeactivateMode(ck_save_debugger_visualizer::VisualizerModeId); }

        GEditor->RedrawLevelEditingViewports();

        return ModeTools.IsModeActive(ck_save_debugger_visualizer::VisualizerModeId) == InEnabled;
    }

    auto
        Get_IsVisualizerEnabled()
        -> bool
    {
        if (GEditor == nullptr)
        { return false; }

        return GLevelEditorModeTools().IsModeActive(ck_save_debugger_visualizer::VisualizerModeId);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Frame_SelectedRow()
        -> bool
    {
        if (GEditor == nullptr || NOT Get_IsVisualizerEnabled())
        { return false; }

        const auto Rows = Get_Rows();
        if (NOT Rows.IsValid())
        { return false; }

        const auto SelectedSavedId = Get_SelectedSavedId();

        for (const auto& Row : *Rows)
        {
            if (Row.SavedId != SelectedSavedId)
            { continue; }

            const auto Center = Row.WorldTransform.GetLocation();
            constexpr auto FrameHalfExtent = 200.0f;
            constexpr auto ActiveViewportOnly = false;

            GEditor->MoveViewportCamerasToBox(
                FBox{Center - FVector{FrameHalfExtent}, Center + FVector{FrameHalfExtent}},
                ActiveViewportOnly);
            return true;
        }

        return false;
    }
}

#endif // WITH_EDITOR
