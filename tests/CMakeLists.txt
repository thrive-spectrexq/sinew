add_executable(sinew_tests
    test_main.cpp
    test_types.cpp
    test_crc32.cpp
    test_wire.cpp
    test_ring_buffer.cpp
)

target_link_libraries(sinew_tests PRIVATE sinew::sinew)
target_include_directories(sinew_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME SinewAllTests COMMAND sinew_tests)
