################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../overload/nbrtos/source/netrx.cpp 

CPP_DEPS += \
./overload/nbrtos/source/netrx.d 

OBJS += \
./overload/nbrtos/source/netrx.o 


# Each subdirectory must supply rules for building sources it contributes
overload/nbrtos/source/%.o: ../overload/nbrtos/source/%.cpp overload/nbrtos/source/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C++ Compiler'
	m68k-unknown-elf-g++ -std=gnu++17 -I"D:\LEAP_Protocol\platforms\NetBurner\LeapGateway\MOD54417_Gateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MOD5441X/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -fno-exceptions -fno-rtti -Wno-write-strings -fno-omit-frame-pointer -falign-functions=4 -fasynchronous-unwind-tables -mcpu=54415 -DMOD5441X -DMCF5441X -DCOLDFIRE -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-overload-2f-nbrtos-2f-source

clean-overload-2f-nbrtos-2f-source:
	-$(RM) ./overload/nbrtos/source/netrx.d ./overload/nbrtos/source/netrx.o

.PHONY: clean-overload-2f-nbrtos-2f-source

