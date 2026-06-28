################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/platform/subdir.mk
-include src/opener/subdir.mk
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


BUILD_ARTIFACT_NAME := MOD54417_Gateway
BUILD_ARTIFACT_EXTENSION := bin
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
USER_OBJS.elf \

ELF += \
MOD54417_Gateway.elf \

S19FILE += \
MOD54417_Gateway.s19 \

NBECLIPSE_TOOLCHAIN_TC30_TOOL_NB_FLASHPACK_OUTPUT_OUTPUTS += \
default.bin \


# All Target
all: main-build

# Main-build Target
main-build: MOD54417_Gateway.bin

# Tool invocations
MOD54417_Gateway.bin: $(S19FILE) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Building target: $@'
	@echo 'Invoking: NB Compcode'
	compcode $(S19FILE) "MOD54417_Gateway.bin" 0xC0040000 0xC1FC0000 -PMOD5441X -B -R -M MOD54417_Gateway.map
	@echo 'Finished building target: $@'
	@echo ' '

MOD54417_Gateway.elf: $(OBJS) $(USER_OBJS) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GNU C/C++ Linker'
	m68k-unknown-elf-g++ $(OBJS) $(USER_OBJS) $(LIBS) -Wl,-Map="MOD54417_Gateway.map" -Wl,--start-group -lnetburner  -lstdc++ -Wl,--end-group -LC:/nburn/platform/MOD5441X/original/lib -Lnblibs -Wl,-n -TC:/nburn/platform/MOD5441X/lib/MOD5441X.ld -Wl,-RC:/nburn/platform/MOD5441X/lib/sys.ld -Wl,--cref -Wl,--gc-sections -mcpu=54415 -o"MOD54417_Gateway.elf"
	@echo 'Finished building: $@'
	@echo ' '

USER_OBJS.elf: MOD54417_Gateway.elf $(OBJS) $(USER_OBJS)

MOD54417_Gateway.s19: $(ELF) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: GNU Objcopy'
	m68k-unknown-elf-objcopy --strip-all --output-target=srec $(ELF) "MOD54417_Gateway.s19"
	@echo 'Finished building: $@'
	@echo ' '

default.bin: $(ELF) makefile objects.mk $(OPTIONAL_TOOL_DEPS)
	@echo 'Invoking: NB Flashpack'
	flashpack -output-target bin -cflag S:0 -cflag S:1 -P MOD5441X -C -cflag S:3 -S -O "default.bin" $(ELF)
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) MOD54417_Gateway.bin MOD54417_Gateway.elf MOD54417_Gateway.s19 USER_OBJS.elf default.bin
	-@echo ' '

.PHONY: all clean dependents main-build

-include ../makefile.targets
