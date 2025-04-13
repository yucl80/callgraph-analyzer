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
            id INTEGER PRIMARY KEY,
            caller_id INTEGER NOT NULL,
            callee_id INTEGER NOT NULL,
            call_file TEXT NOT NULL,
            call_line INTEGER NOT NULL,
            call_column INTEGER NOT NULL,
            context_stack TEXT,
            is_macro_expansion BOOLEAN DEFAULT 0,
            macro_definition_file TEXT,
            macro_definition_line INTEGER,
            FOREIGN KEY (caller_id) REFERENCES functions(id),
            FOREIGN KEY (callee_id) REFERENCES functions(id)
        );

        CREATE TABLE IF NOT EXISTS call_contexts (
            call_id INTEGER NOT NULL,
            context_func_id INTEGER NOT NULL,
            depth INTEGER NOT NULL,
            PRIMARY KEY (call_id, context_func_id),
            FOREIGN KEY (call_id) REFERENCES calls(id),
            FOREIGN KEY (context_func_id) REFERENCES functions(id)
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

bool ProjectDB::storeCallRelation(const ASTSerializer::CallInfo& call) {
    // First insert the call record
    std::string sql = R"(
        INSERT INTO calls (caller_id, callee_id, call_file, call_line, call_column, 
                          is_macro_expansion, macro_definition_file, macro_definition_line)
        VALUES (
            (SELECT id FROM functions WHERE qualified_name = ?),
            (SELECT id FROM functions WHERE qualified_name = ?),
            ?, ?, ?, ?, ?, ?
        )
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, call.caller.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, call.callee.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, call.filePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, call.line);
    sqlite3_bind_int(stmt, 5, call.column);
    sqlite3_bind_int(stmt, 6, call.isMacroExpansion ? 1 : 0);
    sqlite3_bind_text(stmt, 7, call.macroDefinitionFile.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, call.macroDefinitionLine);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    if (!result) return false;

    // Get the last inserted call ID
    sqlite3_int64 callId = sqlite3_last_insert_rowid(db_);

    // Insert context stack relationships
    for (size_t i = 0; i < call.contextStack.size(); i++) {
        sql = R"(
            INSERT INTO call_contexts (call_id, context_func_id, depth)
            VALUES (?, (SELECT id FROM functions WHERE qualified_name = ?), ?)
        )";

        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, callId);
        sqlite3_bind_text(stmt, 2, call.contextStack[i].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, static_cast<int>(i));

        result = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        if (!result) return false;
    }

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
