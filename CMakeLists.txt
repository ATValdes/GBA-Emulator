cmake_minimum_required(VERSION 3.20)
project(gba_emu CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Núcleo del emulador (todo menos main.cpp) ---
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(emu_core ${CORE_SOURCES})
target_include_directories(emu_core PUBLIC include src)

if(MSVC)
  target_compile_options(emu_core PUBLIC /W4 /utf-8)
endif()

# --- Ejecutable ---
add_executable(gba_emu main.cpp)
target_link_libraries(gba_emu PRIVATE emu_core)

# --- Tests ---
option(BUILD_TESTS "Compilar tests" ON)
if(BUILD_TESTS)
  enable_testing()

  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
  )
  FetchContent_MakeAvailable(Catch2)

  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(tests ${TEST_SOURCES})
  target_link_libraries(tests PRIVATE emu_core Catch2::Catch2WithMain)

  include(CTest)
  include(Catch)
  catch_discover_tests(tests)
endif()