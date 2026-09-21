# One executable per test, so that a crash in one does not hide the others.
set(SEABATTLE_TESTS
    test_own_board
)

foreach(t ${SEABATTLE_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE seabattle)
    add_test(NAME ${t} COMMAND ${t})
endforeach()
