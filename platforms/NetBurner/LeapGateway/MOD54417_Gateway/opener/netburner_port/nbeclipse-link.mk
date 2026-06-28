# OpENer static libraries for this NBEclipse project.
# Include from makefile.targets after objects.mk has set LIBS.

OPENER_ROOT        ?= ../../../../../third_party/OpENer_uC-NetBurner
OPENER_BUILD       := $(OPENER_ROOT)/build-nb-$(PLATFORM)
OPENER_LIB         := $(OPENER_BUILD)/lib
OPENER_LIBS_STAMP  := $(OPENER_BUILD)/.opener-core-built

OPENER_COLDFIRE_PLATFORMS := MOD5441X NANO54415 SB800EX

ifeq ($(filter $(PLATFORM),$(OPENER_COLDFIRE_PLATFORMS)),$(PLATFORM))
OPENER_TOOLCHAIN   := $(OPENER_ROOT)/source/buildsupport/Toolchain/Toolchain-NetBurner-ColdFire.cmake
OPENER_NNDK_ARCH   := coldfire
OPENER_NNDK_CPU    := MCF5441X
OPENER_FLOAT_ABI_ARG :=
else
OPENER_TOOLCHAIN   := $(OPENER_ROOT)/source/buildsupport/Toolchain/Toolchain-NetBurner-NNDK.cmake
OPENER_NNDK_ARCH   := cortex-m7
ifeq ($(PLATFORM),SOMRT1061)
OPENER_NNDK_CPU    := MIMXRT10xx
else ifeq ($(PLATFORM),MODRT1171)
OPENER_NNDK_CPU    := MIMXRT11xx
else
OPENER_NNDK_CPU    := SAME70
endif
OPENER_FLOAT_ABI_ARG := -DOPENER_FLOAT_ABI=softfp
endif

OPENER_ARCHIVES := \
	$(OPENER_LIB)/libCIP.a \
	$(OPENER_LIB)/libENET_ENCAP.a \
	$(OPENER_LIB)/libUtils.a \
	$(OPENER_LIB)/libPLATFORM_GENERIC.a \
	$(OPENER_LIB)/libOPENER_HAL.a

LIBS += $(OPENER_ARCHIVES)

.PHONY: opener-nb-libs opener-nb-clean

opener-nb-libs: $(OPENER_LIBS_STAMP)

opener-nb-clean:
	@echo "Cleaning OpENer Core libraries for $(PLATFORM)..."
	@$(RM) -r "$(OPENER_BUILD)"

.NOTPARALLEL: $(OPENER_LIBS_STAMP)

$(OPENER_ARCHIVES): $(OPENER_LIBS_STAMP)
	@test -f '$@'

$(OPENER_LIBS_STAMP): $(OPENER_ROOT)/source/CMakeLists.txt
	@echo "Building OpENer Core libraries for $(PLATFORM) from $(OPENER_ROOT)..."
	+@PATH="$(NNDK_ROOT)/gcc/bin:$$PATH" ; \
	cmake -S "$(OPENER_ROOT)/source" -B "$(OPENER_BUILD)" \
		-DCMAKE_TOOLCHAIN_FILE="$(OPENER_TOOLCHAIN)" \
		-G "Unix Makefiles" \
		-DOPENER_NNDK_ROOT="$(NNDK_ROOT)" \
		-DOPENER_NNDK_PLATFORM="$(PLATFORM)" \
		-DOPENER_NNDK_ARCH="$(OPENER_NNDK_ARCH)" \
		-DOPENER_NNDK_CPU="$(OPENER_NNDK_CPU)" \
		-DOPENER_NET_BACKEND=netburner \
		-DOPENER_BUILD_NETWORK_LAYER=ON \
		-DOPENER_BUILD_PROFILE=core \
		$(OPENER_FLOAT_ABI_ARG) && \
	cmake --build "$(OPENER_BUILD)"
	@touch "$(OPENER_LIBS_STAMP)"

ifneq ($(strip $(ELF)),)
.SECONDEXPANSION:
$(firstword $(ELF)): $(OPENER_LIBS_STAMP)
endif

# Hook project/library clean targets so a normal Clean forces OpENer rebuild.
clean: opener-nb-clean
clean-nblibs: opener-nb-clean
clean-system-library: opener-nb-clean
