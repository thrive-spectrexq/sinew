add_executable(bench_wire bench_wire.cpp)
target_link_libraries(bench_wire PRIVATE sinew::sinew)

add_executable(bench_comparison bench_comparison.cpp)
target_link_libraries(bench_comparison PRIVATE sinew::sinew)

