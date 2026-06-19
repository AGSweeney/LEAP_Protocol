################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/platform/subdir.mk
-include src/http/subdir.mk
-include src/core/subdir.mk
-include src/subdir.mk
-include overload/nbrtos/source/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := MODM7AE70
BUILD_ARTIFACT_EXTENSION := bin
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
USER_OBJS.elf \

ELF += \
MODM7AE70.elf \

S19FILE += \
MODM7AE70.s19 \


# All Target
all: main-build

# Main-build Target
main-build: MODM7AE70.bin

# Tool invocations
MODM7AE70.bin: $(ELF) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: NB Flashpack'
	flashpack -output-target bin -cflag S:0 -cflag S:1 -P MODM7AE70 -C -cflag S:3 -S -O "MODM7AE70.bin" $(ELF)
	@echo 'Finished building target: $@'
	@echo ' '

MODM7AE70.elf: $(OBJS) $(USER_OBJS) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GNU C/C++ Linker'
	arm-unknown-eabi-g++ $(OBJS) $(USER_OBJS) $(LIBS) -Wl,-Map="MODM7AE70.map" -nostartfiles -Wl,--start-group -lnetburner  -lstdc++ -Wl,--end-group -LC:/nburn/platform/MODM7AE70/original/lib -Lnblibs -Wl,-n -TC:/nburn/platform/MODM7AE70/lib/MODM7AE70.ld -Wl,-RC:/nburn/platform/MODM7AE70/lib/sys.ld -Wl,--cref -Wl,--gc-sections -mfpu=fpv5-d16 -mfloat-abi=softfp -mthumb -Wl,--thumb-entry=Reset_Handler -mcpu=cortex-m7 -o"MODM7AE70.elf"
	@echo 'Finished building: $@'
	@echo ' '

USER_OBJS.elf: MODM7AE70.elf $(OBJS) $(USER_OBJS)

MODM7AE70.s19: $(ELF) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GNU Objcopy'
	arm-unknown-eabi-objcopy --strip-all --output-target=srec $(ELF) "MODM7AE70.s19"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) MODM7AE70.bin MODM7AE70.elf MODM7AE70.s19 USER_OBJS.elf
	-@echo ' '

.PHONY: all clean dependents main-build

-include ../makefile.targets
