#pragma once
#include <sqlite3.h>
#include <string>
#include "ASTSerializer.h"

class ProjectDB {
public:
    ProjectDB(const std::string& dbPath);
    ~ProjectDB();

    bool initializeSchema();
    bool storeFunction(const ASTSerializer::FunctionInfo& func);
    bool storeClass(const ASTSerializer::ClassInfo& cls); 
    bool storeCallRelation(const std::string& caller,
                         const std::string& callee);

private:
    sqlite3* db_;

    bool executeSQL(const std::string& sql);
};
