cmake_minimum_required(VERSION 3.8)

project(brepdb)

################################################################################
# Source groups
################################################################################
set(app
    "include/brepdb/IdVisitor.h"
    "include/brepdb/ObjVisitor.h"
)
source_group("app" FILES ${app})

set(rtree
    "include/brepdb/Index.h"
    "include/brepdb/Leaf.h"
    "include/brepdb/Node.h"
    "include/brepdb/RTree.h"
    "include/brepdb/Statistics.h"
    "source/Index.cpp"
    "source/Leaf.cpp"
    "source/Node.cpp"
    "source/RTree.cpp"
)
source_group("rtree" FILES ${rtree})

set(shape
    "include/brepdb/Edge.h"
    "include/brepdb/Face.h"
    "include/brepdb/Point.h"
    "include/brepdb/Region.h"
    "include/brepdb/ShapeType.h"
    "source/Edge.cpp"
    "source/Face.cpp"
    "source/Point.cpp"
    "source/Region.cpp"    
)
source_group("shape" FILES ${shape})

set(storage
    "include/brepdb/DiskStorageManager.h"
    "include/brepdb/MemoryStorageManager.h"
    "source/DiskStorageManager.cpp"
    "source/MemoryStorageManager.cpp"
)
source_group("storage" FILES ${storage})

set(tools
    "include/brepdb/Exception.h"
    "include/brepdb/Math.h"
    "include/brepdb/SpatialIndex.h"
    "include/brepdb/Tools.h"
    "include/brepdb/typedef.h"
    "source/Exception.cpp"
    "source/Math.cpp"
)
source_group("tools" FILES ${tools})

set(ALL_FILES
    ${app}
    ${rtree}
    ${shape}
    ${storage}
    ${tools}
)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(brepdb ${ALL_FILES})

target_compile_features(brepdb PRIVATE cxx_std_17)
