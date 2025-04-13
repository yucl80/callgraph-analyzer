#include <clang-c/Index.h>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include "ASTSerializer.h"

std::string getCursorSpelling(CXCursor cursor) {
    CXString name = clang_getCursorSpelling(cursor);
    std::string result = clang_getCString(name);
    clang_disposeString(name);
    return result;
}

// 增强的类型系统处理
std::string getFullQualifiedName(CXCursor cursor) {
    std::string result;
    CXCursor parent = clang_getCursorSemanticParent(cursor);
    
    if (clang_isInvalid(clang_getCursorKind(parent))) {
        return getCursorSpelling(cursor);
    }
    
    if (clang_getCursorKind(parent) != CXCursor_TranslationUnit) {
        result = getFullQualifiedName(parent) + "::";
    }
    
    // 处理模板特化
    if (clang_getCursorKind(cursor) == CXCursor_FunctionTemplate || 
        clang_getCursorKind(cursor) == CXCursor_ClassTemplate) {
        CXType type = clang_getCursorType(cursor);
        CXString templateArgs = clang_getTypeSpelling(type);
        result += getCursorSpelling(cursor) + "<" + clang_getCString(templateArgs) + ">";
        clang_disposeString(templateArgs);
    } else {
        result += getCursorSpelling(cursor);
    }
    
    // 处理auto类型推导
    if (clang_getCursorKind(cursor) == CXCursor_VarDecl) {
        CXType varType = clang_getCursorType(cursor);
        if (varType.kind == CXType_Auto) {
            CXType deducedType = clang_getCanonicalType(clang_getCursorResultType(cursor));
            CXString typeStr = clang_getTypeSpelling(deducedType);
            result += "/* deduced as " + std::string(clang_getCString(typeStr)) + " */";
            clang_disposeString(typeStr);
        }
    }
    
    return result;
}

// 指针分析上下文
struct PointerContext {
    std::map<std::string, std::set<std::string>> pointerAliases;
    std::map<std::string, std::string> pointerAssignments;
    
    void addAlias(const std::string& ptr1, const std::string& ptr2) {
        pointerAliases[ptr1].insert(ptr2);
        pointerAliases[ptr2].insert(ptr1);
    }
    
    void addAssignment(const std::string& lhs, const std::string& rhs) {
        pointerAssignments[lhs] = rhs;
    }
    
    std::set<std::string> resolvePointer(const std::string& ptr) {
        std::set<std::string> result;
        if (pointerAliases.count(ptr)) {
            for (const auto& alias : pointerAliases[ptr]) {
                result.insert(alias);
                if (pointerAssignments.count(alias)) {
                    result.insert(pointerAssignments[alias]);
                }
            }
        }
        if (pointerAssignments.count(ptr)) {
            result.insert(pointerAssignments[ptr]);
        }
        return result;
    }
};

CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    static PointerContext ptrContext;
    
    // 跨文件分析时不跳过非主文件
    CXSourceLocation location = clang_getCursorLocation(cursor);
    if (clang_Location_isInSystemHeader(location)) {
        return CXChildVisit_Continue;
    }

    CXCursorKind kind = clang_getCursorKind(cursor);
    
    // Handle macro expansions
    if (kind == CXCursor_MacroExpansion) {
        CXString macroName = clang_getCursorSpelling(cursor);
        CXSourceLocation loc = clang_getCursorLocation(cursor);
        
        // Get macro definition location
        CXCursor defCursor = clang_getCursorDefinition(cursor);
        CXSourceLocation defLoc = clang_getCursorLocation(defCursor);
        
        // Get file and line info
        CXFile file;
        unsigned line, column;
        clang_getFileLocation(loc, &file, &line, &column, nullptr);
        CXString fileName = clang_getFileName(file);
        
        CXFile defFile;
        unsigned defLine, defColumn;
        clang_getFileLocation(defLoc, &defFile, &defLine, &defColumn, nullptr);
        CXString defFileName = clang_getFileName(defFile);
        
        std::cout << "[macro] " << clang_getCString(macroName) 
                  << "\n  Defined at: " << clang_getCString(defFileName) << ":" << defLine
                  << "\n  Expanded at: " << clang_getCString(fileName) << ":" << line << std::endl;
                  
        clang_disposeString(macroName);
        clang_disposeString(fileName);
        clang_disposeString(defFileName);
        
        // Visit macro expansion tokens
        clang_visitChildren(cursor, visitor, client_data);
        return CXChildVisit_Continue;
    }
    
    // Handle constructor/destructor calls
    if (kind == CXCursor_Constructor || kind == CXCursor_Destructor) {
        std::string type = (kind == CXCursor_Constructor) ? "[constructor]" : "[destructor]";
        std::string name = getFullQualifiedName(cursor);
        std::cout << type << " " << name << std::endl;
    }
    // Handle exception flow
    else if (kind == CXCursor_CXXTryStmt) {
        std::cout << "[try-block]" << std::endl;
    }
    else if (kind == CXCursor_CXXCatchStmt) {
        CXString exceptionType = clang_getCursorSpelling(cursor);
        std::cout << "[catch] " << clang_getCString(exceptionType) << std::endl;
        clang_disposeString(exceptionType);
    }
    else if (kind == CXCursor_CXXThrowExpr) {
        std::cout << "[throw]" << std::endl;
    }
    // Handle async calls
    // Handle reflection calls
    else if (kind == CXCursor_CXXDynamicCastExpr || 
             kind == CXCursor_CXXTypeidExpr) {
        std::string type = (kind == CXCursor_CXXDynamicCastExpr) ? 
                          "[dynamic_cast]" : "[typeid]";
        CXType exprType = clang_getCursorType(cursor);
        CXString typeStr = clang_getTypeSpelling(exprType);
        std::cout << type << " " << clang_getCString(typeStr) << std::endl;
        clang_disposeString(typeStr);
    }
    // Handle potential incomplete call chains
    else if (kind == CXCursor_CallExpr) {
        // Check for indirect calls through function pointers
        if (clang_Cursor_isDynamicCall(cursor)) {
            std::cout << "[warning] Dynamic call - call chain may be incomplete" << std::endl;
        }
        
        // Check for calls through virtual tables
        if (clang_CXXMethod_isVirtual(cursor)) {
            std::cout << "[warning] Virtual call - runtime target may vary" << std::endl;
        }
        CXString callExpr = clang_getCursorSpelling(cursor);
        std::string callStr = clang_getCString(callExpr);
        clang_disposeString(callExpr);
        
        if (callStr.find("std::async") != std::string::npos || 
            callStr.find("std::thread") != std::string::npos) {
            std::cout << "[async] " << callStr << std::endl;
            // Capture calling context
            CXSourceLocation loc = clang_getCursorLocation(cursor);
            CXFile file;
            unsigned line, column;
            clang_getFileLocation(loc, &file, &line, &column, nullptr);
            CXString fileName = clang_getFileName(file);
            std::cout << "  Called from: " << clang_getCString(fileName) 
                      << ":" << line << std::endl;
            clang_disposeString(fileName);
        }
        CXCursor referenced = clang_getCursorReferenced(cursor);
        if (clang_isInvalid(clang_getCursorKind(referenced))) {
            return CXChildVisit_Continue;
        }

        std::string caller = getFullQualifiedName(parent);
        std::string callee = getFullQualifiedName(referenced);

        // Handle inheritance and virtual calls
        if (clang_getCursorKind(referenced) == CXCursor_CXXMethod) {
            CXType type = clang_getCursorType(referenced);
            if (type.kind == CXType_FunctionProto) {
                // Get full inheritance chain
                CXCursor baseCursor = clang_getCursorDefinition(referenced);
                std::string inheritanceChain;
                
                // Walk up the inheritance hierarchy
                CXCursor current = baseCursor;
                while (!clang_Cursor_isNull(current)) {
                    CXCursor semanticParent = clang_getCursorSemanticParent(current);
                    if (clang_getCursorKind(semanticParent) == CXCursor_ClassDecl ||
                        clang_getCursorKind(semanticParent) == CXCursor_StructDecl) {
                        
                        CXString parentName = clang_getCursorSpelling(semanticParent);
                        inheritanceChain = std::string(clang_getCString(parentName)) + "::" + inheritanceChain;
                        clang_disposeString(parentName);
                        
                        // Check for virtual
                        if (clang_CXXMethod_isVirtual(current)) {
                            inheritanceChain = "[virtual] " + inheritanceChain;
                        }
                    }
                    current = semanticParent;
                }
                
                callee = inheritanceChain + callee;
            }
        }
        // Handle function pointer calls with pointer analysis
        if (clang_getCursorKind(referenced) == CXCursor_DeclRefExpr) {
            CXType type = clang_getCursorType(referenced);
            if (type.kind == CXType_FunctionProto || type.kind == CXType_Pointer) {
                std::string ptrName = getCursorSpelling(referenced);
                auto targets = ptrContext.resolvePointer(ptrName);
                
                if (!targets.empty()) {
                    callee = "[resolved function pointer] ";
                    for (const auto& target : targets) {
                        callee += target + "|";
                    }
                    callee.pop_back(); // Remove last |
                } else {
                    callee = "[function pointer] " + ptrName;
                }
            } else {
                callee = getFullQualifiedName(referenced);
            }
        }
        // Track pointer assignments
        else if (kind == CXCursor_BinaryOperator && 
                clang_getCursorKind(parent) == CXCursor_DeclStmt) {
            std::string lhs = getCursorSpelling(cursor);
            if (clang_getCursorType(cursor).kind == CXType_Pointer) {
                CXCursor rhsCursor = clang_Cursor_getArgument(cursor, 1);
                std::string rhs = getCursorSpelling(rhsCursor);
                ptrContext.addAssignment(lhs, rhs);
            }
        }
        // Handle lambda expressions
        else if (clang_getCursorKind(referenced) == CXCursor_LambdaExpr) {
            callee = "[lambda] " + getCursorSpelling(parent);
        }
        // Handle template instantiations
        else if (clang_getCursorKind(referenced) == CXCursor_FunctionTemplate ||
                 clang_getCursorKind(referenced) == CXCursor_ClassTemplate) {
            CXString templName = clang_getCursorSpelling(referenced);
            callee = "[template] " + std::string(clang_getCString(templName));
            clang_disposeString(templName);
            
            // Get template arguments
            CXType type = clang_getCursorType(referenced);
            if (type.kind == CXType_FunctionProto || type.kind == CXType_Unexposed) {
                CXCursor templCursor = clang_getSpecializedCursorTemplate(referenced);
                if (!clang_Cursor_isNull(templCursor)) {
                    CXString args = clang_getTypeSpelling(type);
                    callee += "<" + std::string(clang_getCString(args)) + ">";
                    clang_disposeString(args);
                }
            }
        }
        // Handle operator overloads
        else if (clang_getCursorKind(referenced) == CXCursor_BinaryOperator ||
                 clang_getCursorKind(referenced) == CXCursor_UnaryOperator ||
                 clang_getCursorKind(referenced) == CXCursor_CompoundAssignOperator ||
                 clang_getCursorKind(referenced) == CXCursor_CXXMethod) {
            CXString opName = clang_getCursorSpelling(referenced);
            callee = "[operator] " + std::string(clang_getCString(opName));
            clang_disposeString(opName);
        }
        else {
            callee = getFullQualifiedName(referenced);
        }
        
        std::cout << caller << " -> " << callee << std::endl;
    }
    
    return CXChildVisit_Recurse;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <source-file>" << std::endl;
        return 1;
    }

    // 创建索引时启用跨文件分析
    CXIndex index = clang_createIndex(1, 1);
    
    // 解析翻译单元时包含所有头文件
    const char* args[] = {
        "-I/usr/include",
        "-I/usr/include/c++/13",
        "-I/usr/include/x86_64-linux-gnu/c++/13"
    };
    
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index,
        argv[1],
        args, sizeof(args)/sizeof(args[0]),
        nullptr, 0,
        CXTranslationUnit_DetailedPreprocessingRecord |
        CXTranslationUnit_KeepGoing);
        
    if (unit == nullptr) {
        std::cerr << "Unable to parse translation unit" << std::endl;
        return 1;
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, visitor, nullptr);

    // Save call graph to database
    ASTSerializer serializer("callgraph.db");
    if (!serializer.serializeTranslationUnit(unit)) {
        std::cerr << "Failed to serialize translation unit" << std::endl;
    }
    if (!serializer.saveToDatabase()) {
        std::cerr << "Failed to save call graph to database" << std::endl;
    }

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    
    return 0;
}
