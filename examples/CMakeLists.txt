add_executable(imu_telemetry imu_telemetry.cpp)
target_link_libraries(imu_telemetry PRIVATE sinew::sinew)

add_executable(ipc_telemetry ipc_telemetry.cpp)
target_link_libraries(ipc_telemetry PRIVATE sinew::sinew)

