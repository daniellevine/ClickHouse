#include <Databases/IDatabase.h>
#include <DataTypes/DataTypeString.h>
#include <Interpreters/Context.h>
#include <Access/AccessRightsContext.h>
#include <Storages/System/StorageSystemDatabases.h>


namespace DB
{

NamesAndTypesList StorageSystemDatabases::getNamesAndTypes()
{
    return {
        {"name", std::make_shared<DataTypeString>()},
        {"engine", std::make_shared<DataTypeString>()},
        {"data_path", std::make_shared<DataTypeString>()},
        {"metadata_path", std::make_shared<DataTypeString>()},
    };
}

void StorageSystemDatabases::fillData(MutableColumns & res_columns, const Context & context, const SelectQueryInfo &) const
{
    const auto access_rights = context.getAccessRights();
    const bool check_access_for_databases = !access_rights->isGranted(AccessType::SHOW);

    auto databases = DatabaseCatalog::instance().getDatabases();
    for (const auto & database : databases)
    {
        if (check_access_for_databases && !access_rights->isGranted(AccessType::SHOW, database.first))
            continue;

        res_columns[0]->insert(database.first);
        res_columns[1]->insert(database.second->getEngineName());
        res_columns[2]->insert(context.getPath() + database.second->getDataPath());
        res_columns[3]->insert(database.second->getMetadataPath());
   }
}

}
