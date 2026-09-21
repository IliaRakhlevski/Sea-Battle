# The console application. Everything that knows about a screen or a keyboard
# is here and nowhere else.
add_executable(seabattle_console
    main.cpp
    console_view.cpp
    coord_format.cpp
    manual_placement.cpp
    manual_targeting.cpp
)

target_link_libraries(seabattle_console PRIVATE seabattle)
