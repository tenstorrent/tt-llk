TOOL_PATH?=sfpi/compiler/bin
BUILD_DIR ?=build

# Tool paths
GXX=$(TOOL_PATH)/riscv32-unknown-elf-g++
OBJDUMP=$(TOOL_PATH)/riscv32-unknown-elf-objdump
OBJCOPY=$(TOOL_PATH)/riscv32-unknown-elf-objcopy
READELF=$(TOOL_PATH)/riscv32-unknown-elf-readelf

# GCC options
OPTIONS_ALL=-O3 -mabi=ilp32 -std=c++17 -g -ffast-math -flto
OPTIONS_COMPILE=-fno-use-cxa-atexit -Wall -fno-exceptions -fno-rtti -Werror -Wno-unknown-pragmas -Wno-error=multistatement-macros -Wno-error=parentheses -Wno-error=unused-but-set-variable -Wno-unused-variable -DTENSIX_FIRMWARE #-DDEBUG_PRINT_ENABLED

ifeq ($(CHIP_ARCH),wormhole)
	OPTIONS_ALL+=-mcpu=tt-wh
	OPTIONS_COMPILE+=-DARCH_WORMHOLE
	ARCH_LLK_ROOT:=tt_llk_wormhole_b0
else ifeq ($(CHIP_ARCH),blackhole)
	OPTIONS_ALL+=-mcpu=tt-bh
	OPTIONS_COMPILE+=-DARCH_BLACKHOLE
	ARCH_LLK_ROOT:=tt_llk_blackhole
else
    $(info CHIP_ARCH is neither wormhole nor blackhole.)
endif

OPTIONS_LINK=-fexceptions -Wl,-z,max-page-size=16 -Wl,-z,common-page-size=16 -nostartfiles -Wl,--trace
INCLUDES = -I../$(ARCH_LLK_ROOT)/llk_lib -I../$(ARCH_LLK_ROOT)/common/inc -I../$(ARCH_LLK_ROOT)/common/inc/sfpu
INCLUDES += -Ifirmware/riscv/common -Ifirmware/riscv/$(CHIP_ARCH)/ -Isfpi/include -Ihelpers/include

FORMAT_ARG:=-D$(format) 

ifeq ($(mathop),)
    MATHOP_ARG =
else
	MATHOP_ARG:=-D$(mathop) -DAPPROX_MODE=$(approx_mode) -DMATH_FIDELITY=$(math_fidelity)
endif

# If test needs to run muliple consecutive operations it needs some additional
# files to be linked to it
ifeq ($(testname), multiple_tiles_eltwise_test)
	MULTIPLE_OPS:=-DMULTIPLE_OPS
	OPTIONS_UNPACK:=-DKERN_CNT=$(kern_cnt) 
	OPTIONS_MATH:=-DKERN_CNT=$(kern_cnt) 
	OPTIONS_PACK:=-DKERN_CNT=$(kern_cnt) 
	OPTIONS_PACK+=-DPACK_ADDR_CNT=$(pack_addr_cnt) 
	OPTIONS_PACK+=-DPACK_ADDRS=$(pack_addrs) 
else	
	MULTIPLE_OPS= 
	LINK_OBJECTS= 
	OPTIONS_UNPACK= 
	OPTIONS_MATH= 
	OPTIONS_PACK= 
endif

OPTIONS_COMPILE+=$(FORMAT_ARG) $(MATHOP_ARG) -DTILE_SIZE_CNT=0x1000
OPTIONS_COMPILE+=$(INCLUDES)

ifeq ($(dest_acc), DEST_ACC) 
	OPTIONS_COMPILE+=-DDEST_ACC
endif

# Define project paths
HELPERS=helpers
RISCV_SOURCES=$(HELPERS)/src
LINKER_SCRIPTS=$(HELPERS)/ld
OUTPUT_ELFS = $(BUILD_DIR)/elf

# Define targets
.PHONY: all clean

all: $(BUILD_DIR) $(OUTPUT_ELFS) $(OUTPUT_ELFS)/$(testname)_trisc0.elf $(OUTPUT_ELFS)/$(testname)_trisc1.elf $(OUTPUT_ELFS)/$(testname)_trisc2.elf $(OUTPUT_ELFS)/brisc.elf
dis: $(BUILD_DIR)/$(testname)_trisc0.dis $(BUILD_DIR)/$(testname)_trisc1.dis $(BUILD_DIR)/$(testname)_trisc2.dis

$(BUILD_DIR)/%.dis :  $(OUTPUT_ELFS)/%.elf
	$(OBJDUMP) -xsD $< > $@
	$(OBJDUMP) -t $< | sort >> $@

$(BUILD_DIR):
	mkdir -p $@

$(OUTPUT_ELFS): $(BUILD_DIR)
	mkdir -p $@

# Buiding .elf files for every TRISC core
$(OUTPUT_ELFS)/$(testname)_trisc0.elf :$(BUILD_DIR)/tmu-crt0.o $(BUILD_DIR)/main_unpack.o $(BUILD_DIR)/$(testname)_unpack.o $(LINK_OBJECTS)
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_LINK) $^ -T$(LINKER_SCRIPTS)/memory.$(CHIP_ARCH).ld -T$(LINKER_SCRIPTS)/trisc0.ld -T$(LINKER_SCRIPTS)/sections.ld -o $@
$(OUTPUT_ELFS)/$(testname)_trisc1.elf: $(BUILD_DIR)/tmu-crt0.o $(BUILD_DIR)/main_math.o $(BUILD_DIR)/$(testname)_math.o $(LINK_OBJECTS)
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_LINK) $^ -T$(LINKER_SCRIPTS)/memory.$(CHIP_ARCH).ld -T$(LINKER_SCRIPTS)/trisc1.ld -T$(LINKER_SCRIPTS)/sections.ld -o $@
$(OUTPUT_ELFS)/$(testname)_trisc2.elf: $(BUILD_DIR)/tmu-crt0.o $(BUILD_DIR)/main_pack.o $(BUILD_DIR)/$(testname)_pack.o $(LINK_OBJECTS)
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_LINK) $^ -T$(LINKER_SCRIPTS)/memory.$(CHIP_ARCH).ld -T$(LINKER_SCRIPTS)/trisc2.ld -T$(LINKER_SCRIPTS)/sections.ld -o $@

# Building brisc.elf for BRISC core
$(OUTPUT_ELFS)/brisc.elf: $(BUILD_DIR)/tmu-crt0.o $(BUILD_DIR)/brisc.o
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_LINK) $^ -T$(LINKER_SCRIPTS)/memory.$(CHIP_ARCH).ld -T$(LINKER_SCRIPTS)/brisc.ld -T$(LINKER_SCRIPTS)/sections.ld -o $@

#compiling _test.cc to .o
$(BUILD_DIR)/$(testname)_unpack.o: sources/$(testname).cc
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_UNPACK) -DLLK_TRISC_UNPACK -c -o $@ $<
$(BUILD_DIR)/$(testname)_math.o: sources/$(testname).cc 
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_MATH) -DLLK_TRISC_MATH -c -o $@ $<
$(BUILD_DIR)/$(testname)_pack.o: sources/$(testname).cc 
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_PACK) -DLLK_TRISC_PACK -c -o $@ $<

#compiling main for every TRISC core
$(BUILD_DIR)/main_unpack.o : $(RISCV_SOURCES)/trisc.cc
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_UNPACK) $(MULTIPLE_OPS) -DLLK_TRISC_UNPACK -c -o $@ $<
$(BUILD_DIR)/main_math.o : $(RISCV_SOURCES)/trisc.cc
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_MATH) $(MULTIPLE_OPS) -DLLK_TRISC_MATH -c -o $@ $<
$(BUILD_DIR)/main_pack.o : $(RISCV_SOURCES)/trisc.cc
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) $(OPTIONS_PACK) $(MULTIPLE_OPS) -DLLK_TRISC_PACK -c -o $@ $<	

$(BUILD_DIR)/tmu-crt0.o: $(HELPERS)/tmu-crt0.S | $(BUILD_DIR)
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) -c -o $@ $<

$(BUILD_DIR)/brisc.o: $(RISCV_SOURCES)/brisc.cc | $(BUILD_DIR)
	$(GXX) $(OPTIONS_ALL) $(OPTIONS_COMPILE) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
	rm -rf __pycache__
	rm -rf .pytest_cache
	$(MAKE) -C python_tests clean