#include "CkInspector_EntityCollections.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEntityCollection/CkEntityCollection_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

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

        const auto CapturedCollection = InCollection;

        // The value IS a count, so it takes the count badge — the old amber literal only encoded
        // "this is a collection", which the row's label already says.
        Builder.AddCountBadgeRow(
            FText::FromString(CollectionName),
            TAttribute<int32>::CreateLambda([CapturedCollection]()
            {
                if (ck::Is_NOT_Valid(CapturedCollection)) { return 0; }
                return UCk_Utils_EntityCollection_UE::Get_NumEntitiesInCollection(CapturedCollection);
            }),
            ECk_Tone::Info,
            FText::FromString(TEXT("entities")),
            [WeakSelectionModel, CollectionHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(CollectionHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ CollectionHandle });
                }
            });

        // One removal row per MEMBER. The count badge above says how many there are; this is the only
        // place the individual handles were ever addressable, so the list has to be walked here. The
        // rows are a compose-time snapshot of membership — the collection's own count badge stays live,
        // and a membership change lands on the next inspector rebuild.
        if (ck::IsValid(CapturedCollection))
        {
            for (const auto& Member : UCk_Utils_EntityCollection_UE::Get_EntitiesInCollection(CapturedCollection).Get_EntitiesInCollection())
            {
                if (ck::Is_NOT_Valid(Member)) { continue; }

                Builder.AddActionRow(
                    FText::FromString(ck::Format_UE(TEXT("  {}"), Member.ToString())),
                    {
                        FCkInspector_Action
                        {
                            FText::FromString(TEXT("Remove")),
                            FText::FromString(TEXT("Request_RemoveEntities with just this entity — the collection itself and the entity both survive.")),
                            [CapturedCollection, Member]()
                            {
                                auto Mutable = CapturedCollection;
                                if (ck::Is_NOT_Valid(Mutable)) { return; }

                                UCk_Utils_EntityCollection_UE::Request_RemoveEntities(Mutable,
                                    FCk_Request_EntityCollection_RemoveEntities{TArray<FCk_Handle>{Member}}, {});
                            }
                        },
                    });
            }
        }
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_EntityCollections::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}
