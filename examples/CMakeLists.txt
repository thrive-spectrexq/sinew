add_executable(imu_telemetry imu_telemetry.cpp)
target_link_libraries(imu_telemetry PRIVATE sinew::sinew sinew_internal_warnings)

add_executable(ipc_telemetry ipc_telemetry.cpp)
target_link_libraries(ipc_telemetry PRIVATE sinew::sinew sinew_internal_warnings)

add_executable(mcu_freestanding mcu_freestanding.cpp)
target_link_libraries(mcu_freestanding PRIVATE sinew::sinew sinew_internal_warnings)
if(MSVC)
    target_compile_options(mcu_freestanding PRIVATE /GR- /EHs-c-)
else()
    target_compile_options(mcu_freestanding PRIVATE -fno-exceptions -fno-rtti)
endif()


