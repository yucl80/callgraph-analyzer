#include "ASTSerializer.h"
#include "ProjectDB.h"
#include <clang-c/CXString.h>
#include <clang-c/CXSourceLocation.h>
#include <iostream>

ASTSerializer::ASTSerializer(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db_) << std::endl;
    }
}

ASTSerializer::~ASTSerializer() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool ASTSerializer::serializeTranslationUnit(CXTranslationUnit tu) {
    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    traverseAST(cursor);
    return true;
}

void ASTSerializer::traverseAST(CXCursor cursor) {
    currentContextStack_.clear();
    CXCursorKind kind = clang_getCursorKind(cursor);
    
    // Update context stack for function declarations
    if (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod) {
        CXString name = clang_getCursorSpelling(cursor);
        currentContextStack_.push_back(clang_getCString(name));
        clang_disposeString(name);
    }

    switch (kind) {
        case CXCursor_FunctionDecl:
        case CXCursor_CXXMethod:
            processFunctionDecl(cursor);
            break;
        case CXCursor_ClassDecl:
        case CXCursor_StructDecl:
            processClassDecl(cursor);
            break;
        case CXCursor_CallExpr:
            processCallExpr(cursor, currentContextStack_);
            break;
        case CXCursor_CXXBaseSpecifier:
            processInheritance(cursor);
            break;
        default:
            break;
    }

    clang_visitChildren(cursor, 
        [](CXCursor c, CXCursor parent, CXClientData client_data) {
            auto* self = static_cast<ASTSerializer*>(client_data);
            self->traverseAST(c);
            return CXChildVisit_Continue;
        }, this);
}

void ASTSerializer::processFunctionDecl(CXCursor cursor) {
    FunctionInfo info;

    // Get function name
    CXString name = clang_getCursorSpelling(cursor);
    info.name = clang_getCString(name);
    clang_disposeString(name);

    // Get qualified name
    CXString qualifiedName = clang_getCursorDisplayName(cursor);
    info.qualifiedName = clang_getCString(qualifiedName);
    clang_disposeString(qualifiedName);

    // Get return type
    CXType returnType = clang_getCursorResultType(cursor);
    info.returnType = getTypeSpelling(returnType);

    // Get parameters
    int numArgs = clang_Cursor_getNumArguments(cursor);
    for (int i = 0; i < numArgs; i++) {
        CXCursor arg = clang_Cursor_getArgument(cursor, i);
        CXType argType = clang_getCursorType(arg);
        info.parameters.push_back(getTypeSpelling(argType));
    }

    // Get location
    info.filePath = getCursorLocation(cursor);
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    clang_getExpansionLocation(loc, nullptr, &info.line, &info.column, nullptr);

    functions_.push_back(info);
}

void ASTSerializer::processClassDecl(CXCursor cursor) {
    ClassInfo info;

    // Get class name
    CXString name = clang_getCursorSpelling(cursor);
    info.name = clang_getCString(name);
    clang_disposeString(name);

    // Get qualified name
    CXString qualifiedName = clang_getCursorDisplayName(cursor);
    info.qualifiedName = clang_getCString(qualifiedName);
    clang_disposeString(qualifiedName);

    // Get location
    info.filePath = getCursorLocation(cursor);
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    clang_getExpansionLocation(loc, nullptr, &info.line, &info.column, nullptr);

    classes_.push_back(info);
}

void ASTSerializer::processCallExpr(CXCursor cursor, const std::vector<std::string>& contextStack) {
    CallInfo call;
    
    // Get caller (current function)
    if (!contextStack.empty()) {
        call.caller = contextStack.back();
    }

    // Get callee
    CXCursor referenced = clang_getCursorReferenced(cursor);
    if (clang_isInvalid(clang_getCursorKind(referenced))) {
        return;
    }
    CXString calleeName = clang_getCursorSpelling(referenced);
    call.callee = clang_getCString(calleeName);
    clang_disposeString(calleeName);

    // Get call location
    call.filePath = getCursorLocation(cursor);
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    clang_getExpansionLocation(loc, nullptr, &call.line, &call.column, nullptr);

    // Check for macro expansion
    call.isMacroExpansion = (clang_getCursorKind(cursor) == CXCursor_MacroExpansion);
    if (call.isMacroExpansion) {
        CXCursor defCursor = clang_getCursorDefinition(referenced);
        if (!clang_isInvalid(clang_getCursorKind(defCursor))) {
            call.macroDefinitionFile = getCursorLocation(defCursor);
            CXSourceLocation defLoc = clang_getCursorLocation(defCursor);
            clang_getExpansionLocation(defLoc, nullptr, &call.macroDefinitionLine, nullptr, nullptr);
        }
    }

    // Store context stack
    call.contextStack = contextStack;

    // Store call info
    calls_.push_back(call);
}

void ASTSerializer::processInheritance(CXCursor cursor) {
    CXCursor derivedCursor = clang_getCursorSemanticParent(cursor);
    CXCursor baseCursor = clang_getTypeDeclaration(clang_getCursorType(cursor));

    // Get derived class name
    CXString derivedName = clang_getCursorDisplayName(derivedCursor);
    std::string derived = clang_getCString(derivedName);
    clang_disposeString(derivedName);

    // Get base class name
    CXString baseName = clang_getCursorDisplayName(baseCursor);
    std::string base = clang_getCString(baseName);
    clang_disposeString(baseName);

    // Store inheritance relationship
    for (auto& cls : classes_) {
        if (cls.qualifiedName == derived) {
            cls.baseClasses.push_back(base);
            break;
        }
    }
}

std::string ASTSerializer::getCursorLocation(CXCursor cursor) {
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned line, column, offset;
    clang_getExpansionLocation(loc, &file, &line, &column, &offset);

    CXString fileName = clang_getFileName(file);
    std::string result = clang_getCString(fileName);
    clang_disposeString(fileName);

    return result;
}

std::string ASTSerializer::getTypeSpelling(CXType type) {
    CXString typeName = clang_getTypeSpelling(type);
    std::string result = clang_getCString(typeName);
    clang_disposeString(typeName);
    return result;
}

bool ASTSerializer::saveToDatabase() {
    ProjectDB db("callgraph.db");
    if (!db.initializeSchema()) {
        return false;
    }

    // Store functions
    for (const auto& func : functions_) {
        if (!db.storeFunction(func)) {
            return false;
        }
    }

    // Store classes
    for (const auto& cls : classes_) {
        if (!db.storeClass(cls)) {
            return false;
        }
    }

    // Store calls
    for (const auto& call : calls_) {
        if (!db.storeCallRelation(call)) {
            return false;
        }
    }

    return true;
}
