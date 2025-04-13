#pragma once
#include <clang-c/Index.h>
#include <clang-c/CXString.h>
#include <clang-c/CXSourceLocation.h>
#include <string>
#include <vector>
#include <sqlite3.h>

class ASTSerializer {
public:
    struct FunctionInfo {
        std::string name;
        std::string qualifiedName;
        std::string returnType;
        std::vector<std::string> parameters;
        std::string filePath;
        unsigned line;
        unsigned column;
    };

    struct ClassInfo {
        std::string name;
        std::string qualifiedName;
        std::vector<std::string> baseClasses;
        std::string filePath;
        unsigned line;
        unsigned column;
    };

    struct CallInfo {
        std::string caller;
        std::string callee;
        std::string filePath;
        unsigned line;
        unsigned column;
        bool isMacroExpansion;
        std::string macroDefinitionFile;
        unsigned macroDefinitionLine;
        bool isVirtualCall;
        bool isTemplateInstantiation;
        bool isExceptionPath;
        std::vector<std::string> contextStack;
    };

    ASTSerializer(const std::string& dbPath);
    ~ASTSerializer();

    bool serializeTranslationUnit(CXTranslationUnit tu);
    bool saveToDatabase();

private:
    sqlite3* db_;
    std::vector<FunctionInfo> functions_;
    std::vector<ClassInfo> classes_;
    std::vector<CallInfo> calls_;
    std::vector<std::string> currentContextStack_;

    void traverseAST(CXCursor cursor);
    void processFunctionDecl(CXCursor cursor);
    void processClassDecl(CXCursor cursor);
    void processCallExpr(CXCursor cursor, const std::vector<std::string>& contextStack = {});
    void processInheritance(CXCursor cursor);

    static std::string getCursorLocation(CXCursor cursor);
    static std::string getTypeSpelling(CXType type);
};
