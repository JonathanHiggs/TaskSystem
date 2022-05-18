add_library(tasksystem STATIC)

file(GLOB_RECURSE files CONFIGURE_DEPENDS
    "src/*.hpp"
    "src/*.cpp"
)

target_sources(tasksystem PRIVATE ${files})

target_compile_options(tasksystem PRIVATE
    "/std:c++20"                    # c++ standard
    "/bigobj"                       # increases the number of sections in .obj files
    "/FC"                           # display full path in diagnostics
    "/WX"                           # warnings as errors
    "/W4"                           # warning level [0,4]
    "/wd4099"                       # exclude: type first seen using #
    "/wd4100"                       # exclude: unreferenced parameter
    "/wd4201"                       # exclude: nameless struct/union
    "$<$<CONFIG:DEBUG>:/Oi>"        # replace calls with intrinsics
    "$<$<CONFIG:DEBUG>:/Zi>"        # generate complete debug info
    "$<$<CONFIG:DEBUG>:/JMC>"       # just my code
    "$<$<CONFIG:RELEASE>:/Ot>"      # prefer fast optimizations
)

target_include_directories(tasksystem PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(tasksystem PUBLIC
    fmt::fmt
)