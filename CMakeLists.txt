cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ENABLE_DEVELOPER_MODE TRUE CACHE BOOL "Enable 'developer mode'")
set(OPT_WARNINGS_AS_ERRORS_DEVELOPER_DEFAULT TRUE)

# ToDo: project_options, project_warnings

project(
    tasksystem
    VERSION 0.0.1
    DESCRIPTION "Async Task System"
    HOMEPAGE_URL "https://github.com/jonathanhiggs/TaskSystem"
    LANGUAGES CXX C)

enable_testing()

find_package(fmt           CONFIG REQUIRED)
find_package(GTest         CONFIG REQUIRED)

add_subdirectory(projects)