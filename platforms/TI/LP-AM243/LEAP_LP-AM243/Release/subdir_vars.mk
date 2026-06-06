################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Add inputs and outputs from these tool invocations to the build variables 
CMD_SRCS += \
../linker.cmd 

SYSCFG_SRCS += \
../example.syscfg 

C_SRCS += \
../enet_layer2_icssg.c \
./syscfg/ti_dpl_config.c \
./syscfg/ti_drivers_config.c \
./syscfg/ti_drivers_open_close.c \
./syscfg/ti_pinmux_config.c \
./syscfg/ti_power_clock_config.c \
./syscfg/ti_board_config.c \
./syscfg/ti_board_open_close.c \
./syscfg/ti_enet_config.c \
./syscfg/ti_enet_init.c \
./syscfg/ti_enet_dma_init.c \
./syscfg/ti_enet_open_close.c \
./syscfg/ti_enet_soc.c \
./syscfg/ti_enet_lwipif.c \
./syscfg/ti_usb_descriptor.c \
D:/LEAP_Protocol/leap_core/src/crc/leap_crc.c \
D:/LEAP_Protocol/leap_core/src/leap_device_stack.c \
D:/LEAP_Protocol/leap_core/src/services/diag/leap_diag_device.c \
D:/LEAP_Protocol/leap_core/src/services/dir/leap_dir_device.c \
D:/LEAP_Protocol/leap_core/src/services/disc/leap_disc_device.c \
D:/LEAP_Protocol/leap_core/src/frame/leap_frame.c \
D:/LEAP_Protocol/leap_core/src/services/mgmt/leap_mgmt_device.c \
D:/LEAP_Protocol/leap_core/src/services/mgmt/leap_mgmt_process.c \
D:/LEAP_Protocol/leap_core/src/services/pd/leap_pd_common.c \
D:/LEAP_Protocol/leap_core/src/services/pd/leap_pd_device.c \
../main.c 

GEN_FILES += \
./syscfg/ti_dpl_config.c \
./syscfg/ti_drivers_config.c \
./syscfg/ti_drivers_open_close.c \
./syscfg/ti_pinmux_config.c \
./syscfg/ti_power_clock_config.c \
./syscfg/ti_board_config.c \
./syscfg/ti_board_open_close.c \
./syscfg/ti_enet_config.c \
./syscfg/ti_enet_init.c \
./syscfg/ti_enet_dma_init.c \
./syscfg/ti_enet_open_close.c \
./syscfg/ti_enet_soc.c \
./syscfg/ti_enet_lwipif.c \
./syscfg/ti_usb_descriptor.c 

GEN_MISC_DIRS += \
./syscfg 

C_DEPS += \
./enet_layer2_icssg.d \
./syscfg/ti_dpl_config.d \
./syscfg/ti_drivers_config.d \
./syscfg/ti_drivers_open_close.d \
./syscfg/ti_pinmux_config.d \
./syscfg/ti_power_clock_config.d \
./syscfg/ti_board_config.d \
./syscfg/ti_board_open_close.d \
./syscfg/ti_enet_config.d \
./syscfg/ti_enet_init.d \
./syscfg/ti_enet_dma_init.d \
./syscfg/ti_enet_open_close.d \
./syscfg/ti_enet_soc.d \
./syscfg/ti_enet_lwipif.d \
./syscfg/ti_usb_descriptor.d \
./leap_crc.d \
./leap_device_stack.d \
./leap_diag_device.d \
./leap_dir_device.d \
./leap_disc_device.d \
./leap_frame.d \
./leap_mgmt_device.d \
./leap_mgmt_process.d \
./leap_pd_common.d \
./leap_pd_device.d \
./main.d 

OBJS += \
./enet_layer2_icssg.o \
./syscfg/ti_dpl_config.o \
./syscfg/ti_drivers_config.o \
./syscfg/ti_drivers_open_close.o \
./syscfg/ti_pinmux_config.o \
./syscfg/ti_power_clock_config.o \
./syscfg/ti_board_config.o \
./syscfg/ti_board_open_close.o \
./syscfg/ti_enet_config.o \
./syscfg/ti_enet_init.o \
./syscfg/ti_enet_dma_init.o \
./syscfg/ti_enet_open_close.o \
./syscfg/ti_enet_soc.o \
./syscfg/ti_enet_lwipif.o \
./syscfg/ti_usb_descriptor.o \
./leap_crc.o \
./leap_device_stack.o \
./leap_diag_device.o \
./leap_dir_device.o \
./leap_disc_device.o \
./leap_frame.o \
./leap_mgmt_device.o \
./leap_mgmt_process.o \
./leap_pd_common.o \
./leap_pd_device.o \
./main.o 

GEN_MISC_FILES += \
./syscfg/ti_dpl_config.h \
./syscfg/ti_drivers_config.h \
./syscfg/ti_drivers_open_close.h \
./syscfg/ti_board_config.h \
./syscfg/ti_board_open_close.h \
./syscfg/ti_enet_config.h \
./syscfg/ti_enet_dma_init.h \
./syscfg/ti_enet_open_close.h \
./syscfg/ti_enet_lwipif.h \
./syscfg/ti_usb_config.h \
./syscfg/linker_defines.h 

GEN_MISC_DIRS__QUOTED += \
"syscfg" 

OBJS__QUOTED += \
"enet_layer2_icssg.o" \
"syscfg\ti_dpl_config.o" \
"syscfg\ti_drivers_config.o" \
"syscfg\ti_drivers_open_close.o" \
"syscfg\ti_pinmux_config.o" \
"syscfg\ti_power_clock_config.o" \
"syscfg\ti_board_config.o" \
"syscfg\ti_board_open_close.o" \
"syscfg\ti_enet_config.o" \
"syscfg\ti_enet_init.o" \
"syscfg\ti_enet_dma_init.o" \
"syscfg\ti_enet_open_close.o" \
"syscfg\ti_enet_soc.o" \
"syscfg\ti_enet_lwipif.o" \
"syscfg\ti_usb_descriptor.o" \
"leap_crc.o" \
"leap_device_stack.o" \
"leap_diag_device.o" \
"leap_dir_device.o" \
"leap_disc_device.o" \
"leap_frame.o" \
"leap_mgmt_device.o" \
"leap_mgmt_process.o" \
"leap_pd_common.o" \
"leap_pd_device.o" \
"main.o" 

GEN_MISC_FILES__QUOTED += \
"syscfg\ti_dpl_config.h" \
"syscfg\ti_drivers_config.h" \
"syscfg\ti_drivers_open_close.h" \
"syscfg\ti_board_config.h" \
"syscfg\ti_board_open_close.h" \
"syscfg\ti_enet_config.h" \
"syscfg\ti_enet_dma_init.h" \
"syscfg\ti_enet_open_close.h" \
"syscfg\ti_enet_lwipif.h" \
"syscfg\ti_usb_config.h" \
"syscfg\linker_defines.h" 

C_DEPS__QUOTED += \
"enet_layer2_icssg.d" \
"syscfg\ti_dpl_config.d" \
"syscfg\ti_drivers_config.d" \
"syscfg\ti_drivers_open_close.d" \
"syscfg\ti_pinmux_config.d" \
"syscfg\ti_power_clock_config.d" \
"syscfg\ti_board_config.d" \
"syscfg\ti_board_open_close.d" \
"syscfg\ti_enet_config.d" \
"syscfg\ti_enet_init.d" \
"syscfg\ti_enet_dma_init.d" \
"syscfg\ti_enet_open_close.d" \
"syscfg\ti_enet_soc.d" \
"syscfg\ti_enet_lwipif.d" \
"syscfg\ti_usb_descriptor.d" \
"leap_crc.d" \
"leap_device_stack.d" \
"leap_diag_device.d" \
"leap_dir_device.d" \
"leap_disc_device.d" \
"leap_frame.d" \
"leap_mgmt_device.d" \
"leap_mgmt_process.d" \
"leap_pd_common.d" \
"leap_pd_device.d" \
"main.d" 

GEN_FILES__QUOTED += \
"syscfg\ti_dpl_config.c" \
"syscfg\ti_drivers_config.c" \
"syscfg\ti_drivers_open_close.c" \
"syscfg\ti_pinmux_config.c" \
"syscfg\ti_power_clock_config.c" \
"syscfg\ti_board_config.c" \
"syscfg\ti_board_open_close.c" \
"syscfg\ti_enet_config.c" \
"syscfg\ti_enet_init.c" \
"syscfg\ti_enet_dma_init.c" \
"syscfg\ti_enet_open_close.c" \
"syscfg\ti_enet_soc.c" \
"syscfg\ti_enet_lwipif.c" \
"syscfg\ti_usb_descriptor.c" 

C_SRCS__QUOTED += \
"../enet_layer2_icssg.c" \
"./syscfg/ti_dpl_config.c" \
"./syscfg/ti_drivers_config.c" \
"./syscfg/ti_drivers_open_close.c" \
"./syscfg/ti_pinmux_config.c" \
"./syscfg/ti_power_clock_config.c" \
"./syscfg/ti_board_config.c" \
"./syscfg/ti_board_open_close.c" \
"./syscfg/ti_enet_config.c" \
"./syscfg/ti_enet_init.c" \
"./syscfg/ti_enet_dma_init.c" \
"./syscfg/ti_enet_open_close.c" \
"./syscfg/ti_enet_soc.c" \
"./syscfg/ti_enet_lwipif.c" \
"./syscfg/ti_usb_descriptor.c" \
"D:/LEAP_Protocol/leap_core/src/crc/leap_crc.c" \
"D:/LEAP_Protocol/leap_core/src/leap_device_stack.c" \
"D:/LEAP_Protocol/leap_core/src/services/diag/leap_diag_device.c" \
"D:/LEAP_Protocol/leap_core/src/services/dir/leap_dir_device.c" \
"D:/LEAP_Protocol/leap_core/src/services/disc/leap_disc_device.c" \
"D:/LEAP_Protocol/leap_core/src/frame/leap_frame.c" \
"D:/LEAP_Protocol/leap_core/src/services/mgmt/leap_mgmt_device.c" \
"D:/LEAP_Protocol/leap_core/src/services/mgmt/leap_mgmt_process.c" \
"D:/LEAP_Protocol/leap_core/src/services/pd/leap_pd_common.c" \
"D:/LEAP_Protocol/leap_core/src/services/pd/leap_pd_device.c" \
"../main.c" 

SYSCFG_SRCS__QUOTED += \
"../example.syscfg" 


