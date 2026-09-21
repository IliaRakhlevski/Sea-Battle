cmake_minimum_required(VERSION 3.15)
project(sea_battle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Warning set.
#
# The compiler is the cheapest reviewer available: it never gets tired and it
# reads every line. Most of what it can catch is off by default, so it is
# turned on explicitly here.
# ---------------------------------------------------------------------------
add_library(seabattle_warnings INTERFACE)

option(SEABATTLE_WERROR "Treat warnings as errors" OFF)

if(MSVC)
    target_compile_options(seabattle_warnings INTERFACE
        /W4             # the highest sane level (/Wall is unusable noise)
        /permissive-    # standard conformance, not the historical MSVC dialect
        /utf-8          # sources are UTF-8 regardless of the system code page
        /w14242         # conversion loses data
        /w14254         # conversion loses bits
        /w14263         # member function does not override anything
        /w14265         # class has virtual functions but no virtual destructor
        /w14287         # unsigned/negative constant mismatch
        /we4289         # loop variable used outside the loop (error, not warning)
        /w14296         # comparison is always true/false
        /w14311         # pointer truncation
        /w14545         # expression before comma has no effect
        /w14546         # function call before comma has no effect
        /w14547         # operator before comma has no effect
        /w14549         # operator before comma has no effect
        /w14555         # expression has no effect
        /w14619         # #pragma warning for a warning number that does not exist
        /w14640         # non-thread-safe static in a member function
        /w14826         # sign-extending conversion, possible loss
        /w14905         # wide string literal cast to LPSTR
        /w14906         # string literal cast to LPWSTR
        /w14928         # illegal copy-initialization
    )
    if(SEABATTLE_WERROR)
        target_compile_options(seabattle_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(seabattle_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion            # implicit conversions that lose value
        -Wsign-conversion       # signed/unsigned conversions - matters with size_t
        -Wshadow                # a name hiding another name
        -Wunused                # anything declared and not used
        -Wnon-virtual-dtor      # polymorphic base without a virtual destructor
        -Wold-style-cast        # C casts in C++ code
        -Wcast-align            # a cast that increases alignment requirements
        -Woverloaded-virtual    # a member function hides a virtual one
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough  # a missing break in a switch
    )
    if(SEABATTLE_WERROR)
        target_compile_options(seabattle_warnings INTERFACE -Werror)
    endif()
endif()

# --- the game rules library: not a single line of output ---
add_library(seabattle INTERFACE)
target_include_directories(seabattle INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(seabattle INTERFACE seabattle_warnings)

# --- sanitizers: enable with -DSEABATTLE_SANITIZE=ON ---
option(SEABATTLE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(SEABATTLE_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()
add_subdirectory(tests)
add_subdirectory(app/console)
