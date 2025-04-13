#include "ProjectDB.h"
#include <iostream>

ProjectDB::ProjectDB(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
    }
}

ProjectDB::~ProjectDB() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool ProjectDB::initializeSchema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS functions (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            qualified_name TEXT NOT NULL,
            return_type TEXT NOT NULL,
            file_path TEXT NOT NULL,
            line INTEGER NOT NULL,
            column INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            qualified_name TEXT NOT NULL,
            file_path TEXT NOT NULL,
            line INTEGER NOT NULL,
            column INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS inheritance (
            derived_id INTEGER NOT NULL,
            base_id INTEGER NOT NULL,
            PRIMARY KEY (derived_id, base_id),
            FOREIGN KEY (derived_id) REFERENCES classes(id),
            FOREIGN KEY (base_id) REFERENCES classes(id)
        );

        CREATE TABLE IF NOT EXISTS calls (
            caller_id INTEGER NOT NULL,
            callee_id INTEGER NOT NULL,
            PRIMARY KEY (caller_id, callee_id),
            FOREIGN KEY (caller_id) REFERENCES functions(id),
            FOREIGN KEY (callee_id) REFERENCES functions(id)
        );
    )";

    return executeSQL(sql);
}

bool ProjectDB::storeFunction(const ASTSerializer::FunctionInfo& func) {
    // Implementation...
    return true;
}

bool ProjectDB::storeClass(const ASTSerializer::ClassInfo& cls) {
    // Implementation...
    return true;
}

bool ProjectDB::storeCallRelation(const std::string& caller, 
                                const std::string& callee) {
    // Implementation...
    return true;
}

bool ProjectDB::executeSQL(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}
