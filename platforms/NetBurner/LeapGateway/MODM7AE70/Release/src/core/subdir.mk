################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/core/core_state.cpp \
../src/core/mapping_state.cpp 

CPP_DEPS += \
./src/core/core_state.d \
./src/core/mapping_state.d 

OBJS += \
./src/core/core_state.o \
./src/core/mapping_state.o 


# Each subdirectory must supply rules for building sources it contributes
src/core/%.o: ../src/core/%.cpp src/core/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	arm-unknown-eabi-g++ -std=gnu++17 -I"D:\LEAP_Protocol\platforms\NetBurner\LeapGateway\MOD54417_Gateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MODM7AE70/include -IC:/nburn/arch/cortex-m7/include -IC:/nburn/arch/cortex-m7/cpu/SAME70/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -fno-use-cxa-atexit -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -falign-functions=4 -fasynchronous-unwind-tables -mcpu=cortex-m7 -DMODM7AE70 -DSAME70 -DCORTEX_M7 -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-core

clean-src-2f-core:
	-$(RM) ./src/core/core_state.d ./src/core/core_state.o ./src/core/mapping_state.d ./src/core/mapping_state.o

.PHONY: clean-src-2f-core

