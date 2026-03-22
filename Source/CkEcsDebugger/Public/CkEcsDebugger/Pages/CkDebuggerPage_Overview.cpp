#include "CkDebuggerPage_Overview.h"

#include "CkCore/Validation/CkIsValid.h"

#include "Widgets/Layout/SBorder.h"

#include "CkEcsDebugger/Graph/CkEcsGraphModel.h"
#include "CkEcsDebugger/Graph/CkEcsGraphLayoutStrategy.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"
#include "CkEcsDebugger/Widgets/CkDebuggerWidget_GraphView.h"

FCkDebuggerPage_Overview::FCkDebuggerPage_Overview() = default;

FCkDebuggerPage_Overview::~FCkDebuggerPage_Overview()
{
    if (SelectionModel.IsValid() && SelectionChangedHandle.IsValid())
    {
        SelectionModel->OnSelectionChanged.Remove(SelectionChangedHandle);
    }
    if (WorldModel.IsValid() && WorldChangedHandle.IsValid())
    {
        WorldModel->OnWorldChanged.Remove(WorldChangedHandle);
    }
}

auto FCkDebuggerPage_Overview::Get_PageName() const -> FText
{
    return FText::FromString(TEXT("Overview"));
}

auto FCkDebuggerPage_Overview::Get_PageIcon() const -> const FSlateBrush*
{
    return nullptr;
}

auto FCkDebuggerPage_Overview::Build_Content(const FCkDebuggerPageContext& InContext) -> TSharedRef<SWidget>
{
    SelectionModel = InContext.SelectionModel;
    WorldModel = InContext.WorldModel;

    GraphModel = MakeUnique<FCkEcsGraphModel>();
    LayoutStrategy = MakeUnique<FCkDirectionalGraphLayout>();

    auto Result = SNew(SBorder)
        .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.Background"))
        .Padding(0.0f)
        [
            SAssignNew(GraphView, SCkDebuggerWidget_GraphView)
            .OnNodeClicked_Raw(this, &FCkDebuggerPage_Overview::OnNodeClicked)
        ];

    // Bind selection and world changes
    if (SelectionModel.IsValid())
    {
        SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnSelectionChanged);

        if (SelectionModel->Get_SelectionCount() > 0)
        {
            RebuildGraph();
        }
    }

    if (WorldModel.IsValid())
    {
        WorldChangedHandle = WorldModel->OnWorldChanged.AddRaw(
            this, &FCkDebuggerPage_Overview::OnWorldChanged);
    }

    return Result;
}

auto FCkDebuggerPage_Overview::Tick(float InDeltaTime) -> void
{
    if (NOT SelectionModel.IsValid() || NOT GraphModel.IsValid() || NOT GraphView.IsValid())
    { return; }

    if (SelectionModel->Get_SelectionCount() > 0)
    {
        const auto PrimaryEntity = SelectionModel->Get_PrimarySelection();
        if (ck::Is_NOT_Valid(PrimaryEntity))
        {
            // Entity was destroyed (e.g., PIE stop)
            GraphView->ClearGraph();
            GraphModel->Invalidate();
        }
        else
        {
            // Catch selections that happened before the delegate was wired
            // (e.g., auto-select of locally controlled character on PIE start).
            // Rebuild() returns false if nothing changed, so this is cheap.
            RebuildGraph();
        }
    }
}

auto FCkDebuggerPage_Overview::IsActive() const -> bool
{
    return IsActivePage;
}

auto FCkDebuggerPage_Overview::Set_IsActive(bool InIsActive) -> void
{
    IsActivePage = InIsActive;

    if (NOT InIsActive)
    {
        if (SelectionModel.IsValid() && SelectionChangedHandle.IsValid())
        {
            SelectionModel->OnSelectionChanged.Remove(SelectionChangedHandle);
            SelectionChangedHandle.Reset();
        }
    }
    else
    {
        if (SelectionModel.IsValid() && NOT SelectionChangedHandle.IsValid())
        {
            SelectionChangedHandle = SelectionModel->OnSelectionChanged.AddRaw(
                this, &FCkDebuggerPage_Overview::OnSelectionChanged);
            RebuildGraph();
        }
    }
}

auto FCkDebuggerPage_Overview::OnSelectionChanged(const TArray<FCk_Handle>& InEntities) -> void
{
    // Force rebuild since entity handles from a previous world are now invalid
    if (GraphModel.IsValid())
    {
        GraphModel->Invalidate();
    }
    RebuildGraph();
}

auto FCkDebuggerPage_Overview::OnWorldChanged(UWorld* InWorld) -> void
{
    // World changed (PIE start/stop) — old entity handles are invalid
    if (GraphView.IsValid())
    {
        GraphView->ClearGraph();
    }
    if (GraphModel.IsValid())
    {
        GraphModel->Invalidate();
    }
}

auto FCkDebuggerPage_Overview::OnNodeClicked(const FCk_Handle& InEntity) -> void
{
    if (SelectionModel.IsValid() && ck::IsValid(InEntity))
    {
        SelectionModel->Set_SelectedEntities({ InEntity });
    }
}

auto FCkDebuggerPage_Overview::RebuildGraph() -> void
{
    if (NOT GraphModel.IsValid() || NOT LayoutStrategy.IsValid() || NOT GraphView.IsValid())
    { return; }

    if (NOT SelectionModel.IsValid() || SelectionModel->Get_SelectionCount() == 0)
    {
        GraphView->ClearGraph();
        return;
    }

    const auto PrimaryEntity = SelectionModel->Get_PrimarySelection();

    if (ck::Is_NOT_Valid(PrimaryEntity))
    {
        GraphView->ClearGraph();
        return;
    }

    const auto bChanged = GraphModel->Rebuild(PrimaryEntity);
    if (bChanged)
    {
        GraphView->RebuildFromModel(*GraphModel, *LayoutStrategy);
    }
}
