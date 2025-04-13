cmake_minimum_required(VERSION 3.20)
project(callgraph_analyzer)

# Find LLVM and Clang packages
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVM_CONFIG: ${LLVM_DIR}")
message(STATUS "Using Clang libs: ${CLANG_LIBS}")

# Set include directories with proper paths
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${LLVM_MAIN_INCLUDE_DIR}
    /usr/lib/llvm-${LLVM_VERSION_MAJOR}/include
    /usr/include/clang-${LLVM_VERSION_MAJOR}
    /usr/include/x86_64-linux-gnu
)

# Set version-specific paths
set(LLVM_VERSION "18.1.3")
set(LLVM_MAJOR_VERSION "18")
message(STATUS "Configuring for LLVM version: ${LLVM_VERSION}")

# Set clang-c include path
set(CLANG_C_INCLUDE_DIR "/usr/lib/llvm-${LLVM_MAJOR_VERSION}/include/clang-c")
message(STATUS "Using clang-c headers from: ${CLANG_C_INCLUDE_DIR}")

# Verify header exists
if(NOT EXISTS "${CLANG_C_INCLUDE_DIR}/Index.h")
    message(FATAL_ERROR "clang-c/Index.h not found at ${CLANG_C_INCLUDE_DIR}")
endif()

# Set all include directories
include_directories(
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${CLANG_C_INCLUDE_DIR}
    "/usr/lib/llvm-${LLVM_MAJOR_VERSION}/include"
    "/usr/include/clang-${LLVM_MAJOR_VERSION}"
    "/usr/include/x86_64-linux-gnu"
)

# Print all include paths for verification
message(STATUS "Final include paths:")
message(STATUS "  - LLVM: ${LLVM_INCLUDE_DIRS}")
message(STATUS "  - Clang: ${CLANG_INCLUDE_DIRS}")
message(STATUS "  - clang-c: ${CLANG_C_INCLUDE_DIR}")

# Add compiler definitions
add_definitions(${LLVM_DEFINITIONS})

add_executable(callgraph_analyzer
    main.cpp
    ASTSerializer.cpp
    ProjectDB.cpp
)

# Set compiler flags with all required definitions
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fPIC -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS")

# Verify and print include paths
message(STATUS "LLVM Include Paths: ${LLVM_INCLUDE_DIRS}")
message(STATUS "Clang Include Paths: ${CLANG_INCLUDE_DIRS}")
message(STATUS "SQLite3 Include Path: ${SQLite3_INCLUDE_DIRS}")

# Get all required LLVM components
llvm_map_components_to_libnames(REQUIRED_LLVM_LIBS
    core
    support
    demangle
)

# Use Clang's provided library list
set(REQUIRED_CLANG_LIBS
    clangFrontend
    clangDriver
    clangSerialization
    clangParse
    clangSema
    clangAnalysis
    clangAST
    clangLex
    clangBasic
    clangTooling
    clangIndex
)

# Verify libraries exist
foreach(lib ${REQUIRED_CLANG_LIBS})
    find_library(CLANG_${lib}_LIBRARY
        NAMES ${lib}
        PATHS ${LLVM_LIBRARY_DIRS}
        REQUIRED
    )
    list(APPEND CLANG_LIBRARIES ${CLANG_${lib}_LIBRARY})
endforeach()

# Add system libraries
find_package(Threads REQUIRED)

# Use LLVM and Clang targets directly
target_link_libraries(callgraph_analyzer
    ${REQUIRED_LLVM_LIBS}
    ${CLANG_LIBRARIES}
    ${SQLite3_LIBRARIES}
    clang-cpp
    /usr/lib/x86_64-linux-gnu/libclang-18.so.18
    ${CMAKE_THREAD_LIBS_INIT}
    dl
    z
)

# Add LLVM lib directory to link path
if(LLVM_LIBRARY_DIRS)
    link_directories(${LLVM_LIBRARY_DIRS})
endif()

# Add compiler flags for Clang
target_compile_options(callgraph_analyzer PRIVATE
    -fno-rtti
    -fPIC
)
