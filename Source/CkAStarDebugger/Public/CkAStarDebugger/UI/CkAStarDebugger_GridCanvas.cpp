#include "CkAStarDebugger/UI/CkAStarDebugger_GridCanvas.h"

#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"

#include "Framework/Application/SlateApplication.h"

namespace ck_astar_debugger_grid_canvas
{
    class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FPreparedUpdate(
            TSharedRef<SCkDebug_PaneHost> InHost,
            TWeakPtr<SCkAStarDebugger_GridView> InGrid,
            TAttribute<bool> InCanDispatchEvents)
            : Host(MoveTemp(InHost)), Grid(MoveTemp(InGrid)), CanDispatchEvents(MoveTemp(InCanDispatchEvents))
        {
        }

        virtual void Commit() noexcept override
        {
            Host->SetEnabled(CanDispatchEvents);
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->SetCanDispatchEvents(CanDispatchEvents); }
        }

    private:
        TSharedRef<SCkDebug_PaneHost> Host;
        TWeakPtr<SCkAStarDebugger_GridView> Grid;
        TAttribute<bool> CanDispatchEvents;
    };

    class FComponent final : public ICkUiRetainedWidget
    {
    public:
        FComponent(
            const TSharedRef<SWidget>& InCanvas,
            TWeakPtr<SCkAStarDebugger_GridView> InGrid,
            const TAttribute<bool>& InCanDispatchEvents)
            : Canvas(InCanvas)
            , Grid(MoveTemp(InGrid))
            , Host(SNew(SCkDebug_PaneHost)
                .Tag(SCkAStarDebugger_GridView::AuthoredHostTag)
                .IsEnabled(InCanDispatchEvents)
                .ContentMode(ECkDebugPaneContent::OpaqueRenderer)
                [InCanvas])
        {
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->SetCanDispatchEvents(InCanDispatchEvents); }
        }

        virtual auto GetWidget() const -> TSharedRef<SWidget> override
        { return Host; }

        virtual auto GetPointerCaptures() const -> TArray<FCkUiPointerCapture> override
        {
            const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin();
            if (NOT PinnedGrid.IsValid() || NOT PinnedGrid->HasMouseCapture()) { return {}; }
            return {{SlateUserIndex, FSlateApplication::CursorPointerIndex, PinnedGrid}};
        }

        virtual void ReleaseTransientInteraction() override
        {
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->CancelTransientInteraction(); }
        }

        virtual void ReleaseOwnerInteraction() override
        {
            ReleaseTransientInteraction();
            Host->SetEnabled(false);
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->SetCanDispatchEvents(false); }
        }

        virtual void BeginPointerCaptureTransfer(const FCkUiPointerCapture&) noexcept override
        {
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->BeginPointerCaptureTransfer(); }
        }

        virtual void EndPointerCaptureTransfer(const FCkUiPointerCapture&, bool InRestored) noexcept override
        {
            if (const TSharedPtr<SCkAStarDebugger_GridView> PinnedGrid = Grid.Pin(); PinnedGrid.IsValid())
            { PinnedGrid->EndPointerCaptureTransfer(InRestored); }
        }

        virtual auto PrepareReload(
            const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            if (InArguments.Slots.FindRef(TEXT("canvas")) != Canvas)
            {
                OutFailure = TEXT("astar-grid requires its stable grid canvas mount.");
                return {};
            }
            return MakeUnique<FPreparedUpdate>(Host, Grid, InArguments.CanDispatchEvents);
        }

        int32 SlateUserIndex = INDEX_NONE;

    private:
        TSharedRef<SWidget> Canvas;
        TWeakPtr<SCkAStarDebugger_GridView> Grid;
        TSharedRef<SCkDebug_PaneHost> Host;
    };
}

auto FCkAStarDebugger_GridCanvas::Register(
    FCkUiWidgetRegistry& InRegistry,
    TWeakPtr<SCkAStarDebugger_GridView> InGrid) -> FCkUiLoadResult
{
    auto Registration = FCkUiCustomWidgetRegistration{};
    Registration.Schema.Tag = TEXT("astar-grid");
    Registration.Schema.Slots = {{TEXT("canvas"), true}};
    Registration.RetainedFactory = [InGrid = MoveTemp(InGrid)](
        const FCkUiCustomWidgetArguments& InArguments,
        FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
    {
        const TSharedPtr<SWidget> Canvas = InArguments.Slots.FindRef(TEXT("canvas"));
        const TSharedPtr<SCkAStarDebugger_GridView> Grid = InGrid.Pin();
        if (NOT Canvas.IsValid() || NOT Grid.IsValid())
        {
            OutFailure = TEXT("astar-grid requires a canvas slot and a live production grid.");
            return {};
        }

        const TSharedRef<ck_astar_debugger_grid_canvas::FComponent> Component =
            MakeShared<ck_astar_debugger_grid_canvas::FComponent>(
            Canvas.ToSharedRef(), InGrid,
            InArguments.CanDispatchEvents);
        Component->SlateUserIndex = InArguments.SlateUserIndex;
        return Component;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
