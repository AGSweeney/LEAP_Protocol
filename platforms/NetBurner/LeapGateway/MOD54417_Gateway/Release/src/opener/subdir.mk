################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/opener/leap_gateway_eip.c \
../src/opener/leapgateway.c \
../src/opener/networkconfig.c \
../src/opener/opener.c 

C_DEPS += \
./src/opener/leap_gateway_eip.d \
./src/opener/leapgateway.d \
./src/opener/networkconfig.d \
./src/opener/opener.d 

OBJS += \
./src/opener/leap_gateway_eip.o \
./src/opener/leapgateway.o \
./src/opener/networkconfig.o \
./src/opener/opener.o 


# Each subdirectory must supply rules for building sources it contributes
src/opener/%.o: ../src/opener/%.c src/opener/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU C Compiler'
	m68k-unknown-elf-gcc --std=gnu17 -I"D:\LEAP_Protocol\platforms\NetBurner\LeapGateway\MOD54417_Gateway\src" -IC:/nburn/nbrtos/include -IC:/nburn/platform/MOD5441X/include -IC:/nburn/arch/coldfire/include -IC:/nburn/arch/coldfire/cpu/MCF5441X/include -IC:/nburn/libraries/include -O2 -Wall -c -fmessage-length=0 -fdata-sections -ffunction-sections -gdwarf-2 -mcpu=54415 -DMOD5441X -DCOLDFIRE -DMCF5441X -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src-2f-opener

clean-src-2f-opener:
	-$(RM) ./src/opener/leap_gateway_eip.d ./src/opener/leap_gateway_eip.o ./src/opener/leapgateway.d ./src/opener/leapgateway.o ./src/opener/networkconfig.d ./src/opener/networkconfig.o ./src/opener/opener.d ./src/opener/opener.o

.PHONY: clean-src-2f-opener

