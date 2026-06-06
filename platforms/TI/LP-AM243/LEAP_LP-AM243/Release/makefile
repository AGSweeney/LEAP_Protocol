################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./enet_layer2_icssg.o" \
"./syscfg/ti_dpl_config.o" \
"./syscfg/ti_drivers_config.o" \
"./syscfg/ti_drivers_open_close.o" \
"./syscfg/ti_pinmux_config.o" \
"./syscfg/ti_power_clock_config.o" \
"./syscfg/ti_board_config.o" \
"./syscfg/ti_board_open_close.o" \
"./syscfg/ti_enet_config.o" \
"./syscfg/ti_enet_init.o" \
"./syscfg/ti_enet_dma_init.o" \
"./syscfg/ti_enet_open_close.o" \
"./syscfg/ti_enet_soc.o" \
"./syscfg/ti_enet_lwipif.o" \
"./syscfg/ti_usb_descriptor.o" \
"./leap_crc.o" \
"./leap_device_stack.o" \
"./leap_diag_device.o" \
"./leap_dir_device.o" \
"./leap_disc_device.o" \
"./leap_frame.o" \
"./leap_mgmt_device.o" \
"./leap_mgmt_process.o" \
"./leap_pd_common.o" \
"./leap_pd_device.o" \
"./main.o" \
"./leap_device/leap_host.o" \
"./leap_device/leap_hw.o" \
"../linker.cmd" \
$(GEN_CMDS__FLAG) \
-Wl,-lfreertos.am243x.r5f.ti-arm-clang.release.lib \
-Wl,-ldrivers.am243x.r5f.ti-arm-clang.freertos.release.lib \
-Wl,-lenet-icssg.am243x.r5f.ti-arm-clang.freertos.release.lib \
-Wl,-lboard.am243x.r5f.ti-arm-clang.freertos.release.lib \
-Wl,-llibc.a \
-Wl,-llibsysbm.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include leap_device/subdir_vars.mk
-include subdir_rules.mk
-include leap_device/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
LEAP_LP-AM243.out 

EXE_OUTPUTS__QUOTED += \
"LEAP_LP-AM243.out" 


# All Target
all: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "LEAP_LP-AM243.out"

# Tool invocations
LEAP_LP-AM243.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: Arm Linker'
	"C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mlittle-endian -mthumb -Oz -flto -DSOC_AM243X -DENET_ENABLE_PER_ICSSG=1 -DOS_FREERTOS -g -Wall -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-function -Wl,-m"enet_layer2_icssg.Release.map" -Wl,-i"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/kernel/freertos/lib" -Wl,-i"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/drivers/lib" -Wl,-i"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/board/lib" -Wl,-i"C:/ti/mcu_plus_sdk_am243x_12_00_00_27/source/networking/enet/lib" -Wl,-i"C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/lib" -Wl,-i"D:/LEAP_Protocol/platforms/TI/LP-AM243/LEAP_LP-AM243/Release/syscfg" -Wl,--reread_libs -Wl,--diag_suppress=10063 -Wl,--diag_wrap=off -Wl,--display_error_number -Wl,--warn_sections -Wl,--gen_xml_func_hash -Wl,--xml_link_info="LEAP_LP-AM243_linkInfo.xml" -Wl,--ram_model -Wl,--zero_init=on -Wl,--use_memcpy=fast -Wl,--use_memset=fast -o "LEAP_LP-AM243.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(GEN_MISC_FILES__QUOTED)$(GEN_FILES__QUOTED)$(EXE_OUTPUTS__QUOTED)
	-$(RM) "enet_layer2_icssg.o" "syscfg\ti_dpl_config.o" "syscfg\ti_drivers_config.o" "syscfg\ti_drivers_open_close.o" "syscfg\ti_pinmux_config.o" "syscfg\ti_power_clock_config.o" "syscfg\ti_board_config.o" "syscfg\ti_board_open_close.o" "syscfg\ti_enet_config.o" "syscfg\ti_enet_init.o" "syscfg\ti_enet_dma_init.o" "syscfg\ti_enet_open_close.o" "syscfg\ti_enet_soc.o" "syscfg\ti_enet_lwipif.o" "syscfg\ti_usb_descriptor.o" "leap_crc.o" "leap_device_stack.o" "leap_diag_device.o" "leap_dir_device.o" "leap_disc_device.o" "leap_frame.o" "leap_mgmt_device.o" "leap_mgmt_process.o" "leap_pd_common.o" "leap_pd_device.o" "main.o" "leap_device\leap_host.o" "leap_device\leap_hw.o" 
	-$(RM) "enet_layer2_icssg.d" "syscfg\ti_dpl_config.d" "syscfg\ti_drivers_config.d" "syscfg\ti_drivers_open_close.d" "syscfg\ti_pinmux_config.d" "syscfg\ti_power_clock_config.d" "syscfg\ti_board_config.d" "syscfg\ti_board_open_close.d" "syscfg\ti_enet_config.d" "syscfg\ti_enet_init.d" "syscfg\ti_enet_dma_init.d" "syscfg\ti_enet_open_close.d" "syscfg\ti_enet_soc.d" "syscfg\ti_enet_lwipif.d" "syscfg\ti_usb_descriptor.d" "leap_crc.d" "leap_device_stack.d" "leap_diag_device.d" "leap_dir_device.d" "leap_disc_device.d" "leap_frame.d" "leap_mgmt_device.d" "leap_mgmt_process.d" "leap_pd_common.d" "leap_pd_device.d" "main.d" "leap_device\leap_host.d" "leap_device\leap_hw.d" 
	-$(RMDIR) $(GEN_MISC_DIRS__QUOTED)
	-@echo 'Finished clean'
	-@echo ' '

post-build:
	-$(MAKE) -C D:\LEAP_Protocol\platforms\TI\LP-AM243\LEAP_LP-AM243 -f makefile_ccs_bootimage_gen OUTNAME=LEAP_LP-AM243 PROFILE=Release MCU_PLUS_SDK_PATH=C:/ti/mcu_plus_sdk_am243x_12_00_00_27 CG_TOOL_ROOT=C:/ti/ccs2040/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS CCS_INSTALL_DIR=C:\ti\ccs2040\ccs\ CCS_IDE_MODE=desktop DEVICE=am243x
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

