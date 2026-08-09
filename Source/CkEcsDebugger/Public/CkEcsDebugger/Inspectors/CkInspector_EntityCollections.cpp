#include "CkInspector_EntityCollections.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_EntityCollections)

auto FCkInspector_EntityCollections::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Collections"));
}

auto FCkInspector_EntityCollections::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_EntityCollection_UE::Has_Any(Entity);
}

auto FCkInspector_EntityCollections::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return BuildCollectionGrid(Entity, FString());
}

auto FCkInspector_EntityCollections::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    return BuildCollectionGrid(Entity, InFilter);
}

auto FCkInspector_EntityCollections::BuildCollectionGrid(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    UCk_Utils_EntityCollection_UE::ForEach_EntityCollection(MutableEntity, [&Builder, WeakSelectionModel](FCk_Handle_EntityCollection InCollection)
    {
        const auto CollectionTag = UCk_Utils_GameplayLabel_UE::Get_Label(InCollection);
        const auto CollectionName = CollectionTag.IsValid()
            ? CollectionTag.ToString()
            : TEXT("Unnamed");

        const auto CollectionHandle = FCk_Handle(InCollection);
        const auto CapturedTag = CollectionTag;

        Builder.AddClickableRow(
            FText::FromString(CollectionName),
            [CapturedTag](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto Collection = UCk_Utils_EntityCollection_UE::TryGet_EntityCollection(MutableE, CapturedTag);
                if (ck::Is_NOT_Valid(Collection)) { return FText::GetEmpty(); }

                const auto Count = UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(Collection);
                return FText::FromString(ck::Format_UE(TEXT("{} entities"), Count));
            },
            // The value IS a count, so it takes the numeric value role — the old amber literal only encoded
            // "this is a collection", which the row's label already says.
            CkStyle::Value_Numeric(),
            [WeakSelectionModel, CollectionHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(CollectionHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ CollectionHandle });
                }
            });
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_EntityCollections::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
