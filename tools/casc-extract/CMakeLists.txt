# Standalone CLI that pulls files out of the game's CASC storage (UI layouts,
# string tables) into d2r-access/game-data so the layout indexer can run offline.
# Rerun after every game patch. CascLib is fetched at configure time.
cmake_minimum_required(VERSION 3.28)
project(casc_extract CXX)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
set(CASC_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(CASC_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
# CascLib's own cmake_minimum_required predates CMake 4.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(CascLib GIT_REPOSITORY https://github.com/ladislav-zezula/CascLib.git GIT_TAG master GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(CascLib)

add_executable(casc_extract casc_extract.cpp)
target_include_directories(casc_extract PRIVATE "${casclib_SOURCE_DIR}/src")
target_link_libraries(casc_extract casc_static)
