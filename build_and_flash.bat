rmdir /S /Q build
mkdir build
cd build
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake ..
ninja
STM32_Programmer_CLI -c port=JLINK -d STM32_WB55_BLE_V2.elf -v -rst
@REM STM32_Programmer_CLI -c port=SWD sn=0670FF3332584B3043195621 -w STM32_WB55_BLE_V2.elf -v -rst