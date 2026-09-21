# Every test is an ordinary program that returns 0 when all its checks pass.
# One executable per test, so that a crash in one does not hide the others.

set(SEABATTLE_TESTS
    test_board_area
    test_placement
    test_own_board
    test_enemy_map
    test_session
)

foreach(t ${SEABATTLE_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE seabattle)
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# The console's pieces are tested without a console: their streams are
# constructor parameters, so typed input and drawn output are just strings.
add_executable(test_console test_console.cpp)
target_link_libraries(test_console PRIVATE seabattle_console_core)
add_test(NAME test_console COMMAND test_console)
