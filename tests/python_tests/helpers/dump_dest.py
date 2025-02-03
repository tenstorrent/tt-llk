import pytest
import torch
import os
from ttlens.tt_lens_init import init_ttlens
from ttlens.tt_lens_lib import write_to_device, read_words_from_device, run_elf

#context = init_debuda()
read_data = read_words_from_device("0,0", 0x1a000, word_count=512)
print("read_data[0]", hex(read_data[0]))
print("read_data[1]", hex(read_data[1]))
print("read_data[2]", hex(read_data[2]))
print("read_data[3]", hex(read_data[3]))
print("read_data[4]", hex(read_data[4]))
print("read_data[5]", hex(read_data[5]))
print("read_data[6]", hex(read_data[6]))
print("read_data[7]", hex(read_data[7]))

RISCV_DEBUG_REG_CFGREG_RD_CNTL = 0xFFB12058
RISCV_DEBUG_REG_DBG_RD_DATA = 0xFFB1205C
RISCV_DEBUG_REG_CFGREG_RDDATA = 0xFFB12078
RISCV_DEBUG_REG_DBG_ARRAY_RD_EN = 0xFFB12060
RISCV_DEBUG_REG_DBG_ARRAY_RD_CMD = 0xFFB12064
RISCV_DEBUG_REG_DBG_ARRAY_RD_DATA = 0xFFB1206C

def read_config_register0(address, mask, shift):
    write_to_device("0,0", RISCV_DEBUG_REG_CFGREG_RD_CNTL, [address])
    a = read_words_from_device("0,0", RISCV_DEBUG_REG_CFGREG_RDDATA, word_count=1)
    return (a[0] & mask) >> shift

def flip_bfp16_bits(value):
    sign = (value & 0x8000) >> 15
    mantisa = (value & 0x7F00) >> 8
    exponent = value & 0xFF
    result = (sign << 15) | (exponent << 7) | mantisa
    return result

def flip_dest_bits(value):
    return flip_bfp16_bits(value & 0xffff) | (flip_bfp16_bits(value >> 16) << 16)

ALU_FORMAT_SPEC_REG2_Dstacc_ADDR32 = 1
ALU_FORMAT_SPEC_REG2_Dstacc_MASK = 0x1e000000
ALU_FORMAT_SPEC_REG2_Dstacc_SHAMT = 25

ALU_ACC_CTRL_Fp32_enabled_ADDR32 = 1
ALU_ACC_CTRL_Fp32_enabled_SHAMT = 29
ALU_ACC_CTRL_Fp32_enabled_MASK = 0x20000000

data_format = read_config_register0(ALU_FORMAT_SPEC_REG2_Dstacc_ADDR32, ALU_FORMAT_SPEC_REG2_Dstacc_MASK, ALU_FORMAT_SPEC_REG2_Dstacc_SHAMT)
print("data format:", data_format)
force_32bit_format = read_config_register0(ALU_ACC_CTRL_Fp32_enabled_ADDR32, ALU_ACC_CTRL_Fp32_enabled_MASK, ALU_ACC_CTRL_Fp32_enabled_SHAMT)
print("force 32bit float format:", force_32bit_format)

write_to_device("0,0", RISCV_DEBUG_REG_DBG_ARRAY_RD_EN, [0x1])
#for row in range(64):
for row in range(64):
    for i in range(8):
        dbg_array_rd_cmd = row + (i << 12) + (2 << 16)
        #print("dbg_array_rd_cmd", hex(dbg_array_rd_cmd), dbg_array_rd_cmd)
        write_to_device("0,0", RISCV_DEBUG_REG_DBG_ARRAY_RD_CMD, dbg_array_rd_cmd.to_bytes(4, byteorder='little'))
        rd_data = read_words_from_device("0,0", RISCV_DEBUG_REG_DBG_ARRAY_RD_DATA, word_count=1)

        demangled = flip_dest_bits(rd_data[0])

        print("rd_data", hex(demangled), " @ ", row,i)

write_to_device("0,0", RISCV_DEBUG_REG_DBG_ARRAY_RD_EN, [0x0])
write_to_device("0,0", RISCV_DEBUG_REG_DBG_ARRAY_RD_CMD, [0x0])


# THREAD_0_CFG = 0 # Thread 0 config
# THREAD_1_CFG = 1 # // Thread 1 config
# THREAD_2_CFG = 2#; // Thread 2 config
# HW_CFG_0 = 3#; // Hardware config state 0
# HW_CFG_1 = 4#; // Hardware config state 1
# HW_CFG_SIZE = 187
# THREAD_CFG_SIZE = THD_STATE_SIZE

# ALU_FORMAT_SPEC_REG2_Dstacc_ADDR32 = 1
# ALU_FORMAT_SPEC_REG2_Dstacc_MASK = 0x1e000000
# ALU_FORMAT_SPEC_REG2_Dstacc_SHAMT = 25


#uint32_t data_format_reg_field_value =  (dbg_read_cfgreg(ckernel::dbg_cfgreg::HW_CFG_0, ALU_FORMAT_SPEC_REG2_Dstacc_ADDR32) & ALU_FORMAT_SPEC_REG2_Dstacc_MASK) >> ALU_FORMAT_SPEC_REG2_Dstacc_SHAMT

#   inline std::uint32_t dbg_read_cfgreg(const uint32_t cfgreg_id, const uint32_t addr) {

#     uint32_t hw_base_addr = 0;

#     switch (cfgreg_id) {
#         case dbg_cfgreg::HW_CFG_1:
#             hw_base_addr = dbg_cfgreg::HW_CFG_SIZE;
#             break;
#         case dbg_cfgreg::THREAD_0_CFG:
#         case dbg_cfgreg::THREAD_1_CFG:
#         case dbg_cfgreg::THREAD_2_CFG:
#             hw_base_addr = 2*dbg_cfgreg::HW_CFG_SIZE + cfgreg_id*dbg_cfgreg::THREAD_CFG_SIZE;
#             break;
#         default:
#             break;

#     }

#     uint32_t hw_addr = hw_base_addr + (addr&0x7ff); // hw address is 4-byte aligned
#     reg_write(RISCV_DEBUG_REG_CFGREG_RD_CNTL, hw_addr);

#     wait(1);

#     return reg_read(RISCV_DEBUG_REG_CFGREG_RDDATA);


# }
  