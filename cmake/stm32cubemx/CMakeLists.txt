cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	USE_NUCLEO_64 
	USE_HAL_DRIVER 
	STM32WB55xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Utilities/lpm/tiny_lpm
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/template
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Inc
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Src
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/P-NUCLEO-WB55.Nucleo
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32WBxx/Include
    ${CMAKE_SOURCE_DIR}/Utilities/sequencer
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_SOURCE_DIR}/Src/main.c
    ${CMAKE_SOURCE_DIR}/Src/app_entry.c
    ${CMAKE_SOURCE_DIR}/Src/app_debug.c
    ${CMAKE_SOURCE_DIR}/Src/hw_timerserver.c
    ${CMAKE_SOURCE_DIR}/Src/hw_uart.c
    ${CMAKE_SOURCE_DIR}/Src/stm32_lpm_if.c
    ${CMAKE_SOURCE_DIR}/Src/app_ble.c
    ${CMAKE_SOURCE_DIR}/Src/p2p_client_app.c
    ${CMAKE_SOURCE_DIR}/Src/hw_ipcc.c
    ${CMAKE_SOURCE_DIR}/Src/stm32wbxx_it.c
    ${CMAKE_SOURCE_DIR}/Src/stm32wbxx_hal_msp.c
    ${CMAKE_SOURCE_DIR}/Src/sysmem.c
    ${CMAKE_SOURCE_DIR}/Src/syscalls.c
    ${CMAKE_SOURCE_DIR}/startup_stm32wb55xx_cm4.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_SOURCE_DIR}/Src/system_stm32wbxx.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_hsem.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_ll_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_flash_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_dma_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_pwr_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_exti.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_ipcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_uart_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rng.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_rtc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_usart.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32WBxx_HAL_Driver/Src/stm32wbxx_hal_usart_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/P-NUCLEO-WB55.Nucleo/stm32wbxx_nucleo.c
)

# Drivers Midllewares


set(Utilities_Src
    ${CMAKE_SOURCE_DIR}/Utilities/lpm/tiny_lpm/stm32_lpm.c
    ${CMAKE_SOURCE_DIR}/Utilities/sequencer/stm32_seq.c
)
set(STM32_WPAN_Src
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/tl_mbox.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/shci/shci.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities/dbg_trace.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities/otp.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities/stm_list.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/utilities/stm_queue.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/template/osal.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gap_aci.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_gatt_aci.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hal_aci.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_hci_le.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/core/auto/ble_l2cap_aci.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/hci_tl_if.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/interface/patterns/ble_thread/tl/shci_tl_if.c
    ${CMAKE_SOURCE_DIR}/Middlewares/ST/STM32_WPAN/ble/svc/Src/svc_ctl.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS 
    STM32_Drivers
    Utilities	STM32_WPAN	
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create Utilities static library
add_library(Utilities OBJECT)
target_sources(Utilities PRIVATE ${Utilities_Src})
target_link_libraries(Utilities PUBLIC stm32cubemx)

# Create STM32_WPAN static library
add_library(STM32_WPAN OBJECT)
target_sources(STM32_WPAN PRIVATE ${STM32_WPAN_Src})
target_link_libraries(STM32_WPAN PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
