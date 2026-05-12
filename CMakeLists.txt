cmake_minimum_required(VERSION 3.16)

project(spatialdb)

################################################################################
# Source groups
################################################################################
set(app
    "include/spatialdb/IdVisitor.h"
    "include/spatialdb/ObjVisitor.h"
)
source_group("app" FILES ${app})

set(rtree
    "include/spatialdb/Index.h"
    "include/spatialdb/Leaf.h"
    "include/spatialdb/Node.h"
    "include/spatialdb/RTree.h"
    "include/spatialdb/Statistics.h"
    "source/Index.cpp"
    "source/Leaf.cpp"
    "source/Node.cpp"
    "source/RTree.cpp"
)
source_group("rtree" FILES ${rtree})

set(shape
    "include/spatialdb/Edge.h"
    "include/spatialdb/Face.h"
    "include/spatialdb/Point.h"
    "include/spatialdb/Region.h"
    "include/spatialdb/ShapeType.h"
    "source/Edge.cpp"
    "source/Face.cpp"
    "source/Point.cpp"
    "source/Region.cpp"    
)
source_group("shape" FILES ${shape})

set(storage
    "include/spatialdb/DiskStorageManager.h"
    "include/spatialdb/MemoryStorageManager.h"
    "source/DiskStorageManager.cpp"
    "source/MemoryStorageManager.cpp"
)
source_group("storage" FILES ${storage})

set(tools
    "include/spatialdb/Exception.h"
    "include/spatialdb/Math.h"
    "include/spatialdb/SpatialIndex.h"
    "include/spatialdb/Tools.h"
    "include/spatialdb/typedef.h"
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

add_library(spatialdb ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC include)

target_compile_features(spatialdb PRIVATE cxx_std_17)
