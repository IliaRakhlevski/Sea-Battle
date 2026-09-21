# The console application. Everything that knows about a screen or a keyboard
# is here and nowhere else.

# Everything except main() goes into a library, so the tests can link the very
# same code the game runs.
add_library(seabattle_console_core STATIC
    console_view.cpp
    coord_format.cpp
    manual_placement.cpp
    manual_targeting.cpp
)
target_include_directories(seabattle_console_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(seabattle_console_core PUBLIC seabattle)

add_executable(seabattle_console main.cpp)
target_link_libraries(seabattle_console PRIVATE seabattle_console_core)
