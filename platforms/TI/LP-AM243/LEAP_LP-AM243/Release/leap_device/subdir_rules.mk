################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
leap_device/%.o: ../leap_device/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mlittle-endian -mthumb -Oz -flto -I"C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/include/c" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/board/ethphy/enet/rtos_drivers/include" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/board/ethphy/port" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/kernel/freertos/FreeRTOS-Kernel/include" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/kernel/freertos/portable/TI_ARM_CLANG/ARM_CR5F" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/kernel/freertos/config/am243x/r5f" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/utils" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/utils/include" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/utils/V3" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/include" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/include/phy" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/core/include/core" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/soc/k3/am64x_am243x" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/hw_include" -I"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/hw_include/mdio/V4" -I"D:/LEAP_Protocol/leap_core/inc" -I"D:/LEAP_Protocol/platforms/TI/LP-AM243/LEAP_LP-AM243/leap_device" -DSOC_AM243X -DENET_ENABLE_PER_ICSSG=1 -DOS_FREERTOS -g -Wall -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -MMD -MP -MF"leap_device/$(basename $(<F)).d_raw" -MT"$(@)" -I"D:/LEAP_Protocol/platforms/TI/LP-AM243/LEAP_LP-AM243/Release/syscfg"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


