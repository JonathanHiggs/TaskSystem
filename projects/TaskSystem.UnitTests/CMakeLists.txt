include(GoogleTest)

add_executable(tasksystem_unittests)

file(GLOB_RECURSE files CONFIGURE_DEPENDS
    "src/*.hpp"
    "src/*.cpp"
)

target_sources(tasksystem_unittests PRIVATE ${files})

set_target_properties(tasksystem_unittests PROPERTIES OUTPUT_NAME "TaskSystem.UnitTests")

target_compile_options(tasksystem_unittests PRIVATE
    "/std:c++20"                    # c++ standard
    "/bigobj"                       # increases the number of sections in .obj files
    "/FC"                           # display full path in diagnostics
    "/WX"                           # warnings as errors
    "/W4"                           # warning level [0,4]
    "/wd4099"                       # exclude: type first seen using #
    "/wd4100"                       # exclude: unreferenced parameter
    "/wd4201"                       # exclude: nameless struct/union
    "/wd4834"                       # exclude: discarding a nodiscard value
    "$<$<CONFIG:DEBUG>:/Oi>"        # replace calls with intrinsics
    "$<$<CONFIG:DEBUG>:/Zi>"        # generate complete debug info
    "$<$<CONFIG:DEBUG>:/JMC>"       # just my code
    "$<$<CONFIG:RELEASE>:/Ot>"      # prefer fast optimizations
)

target_include_directories(tasksystem_unittests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(tasksystem_unittests PRIVATE
    tasksystem
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME tasksystem_unittests COMMAND tasksystem_unittests)

gtest_discover_tests(tasksystem_unittests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} DISCOVERY_MODE PRE_TEST)