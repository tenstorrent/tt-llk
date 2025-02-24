#include "tensix.h"
#include "ckernel_instr_params.h"
#include "ckernel_ops.h"
#include "risc_attribs.h"
#include "ckernel_structs.h"
#include "dev_mem_map.h"

using namespace ckernel;

constexpr uint32_t RISCV_IC_BRISC_MASK = 0x1;
constexpr uint32_t RISCV_IC_NCRISC_MASK = 0x10;
constexpr uint32_t RISCV_IC_TRISC0_MASK = 0x2;
constexpr uint32_t RISCV_IC_TRISC1_MASK = 0x4;
constexpr uint32_t RISCV_IC_TRISC2_MASK = 0x8;
constexpr uint32_t RISCV_IC_TRISC_ALL_MASK = RISCV_IC_TRISC0_MASK | RISCV_IC_TRISC1_MASK | RISCV_IC_TRISC2_MASK;

inline void WRITE_REG(uint32_t addr, uint32_t val) {
    volatile tt_reg_ptr uint32_t* ptr = reinterpret_cast<uint32_t*>(addr);
    ptr[0] = val;
}

inline void set_deassert_addresses() {
    volatile tt_reg_ptr uint32_t* cfg_regs = reinterpret_cast<uint32_t*>(TENSIX_CFG_BASE);

#ifdef ARCH_BLACKHOLE
    WRITE_REG(RISCV_DEBUG_REG_NCRISC_RESET_PC, MEM_NCRISC_FIRMWARE_BASE);
    WRITE_REG(RISCV_DEBUG_REG_TRISC0_RESET_PC, MEM_TRISC0_FIRMWARE_BASE);
    WRITE_REG(RISCV_DEBUG_REG_TRISC1_RESET_PC, MEM_TRISC1_FIRMWARE_BASE);
    WRITE_REG(RISCV_DEBUG_REG_TRISC2_RESET_PC, MEM_TRISC2_FIRMWARE_BASE);
    WRITE_REG(RISCV_DEBUG_REG_TRISC_RESET_PC_OVERRIDE, 0b111);
    WRITE_REG(RISCV_DEBUG_REG_NCRISC_RESET_PC_OVERRIDE, 0x1);
#else
    // cfg_regs[NCRISC_RESET_PC_PC_ADDR32] = MEM_NCRISC_FIRMWARE_BASE;
    // cfg_regs[TRISC_RESET_PC_SEC0_PC_ADDR32] = MEM_TRISC0_FIRMWARE_BASE;
    // cfg_regs[TRISC_RESET_PC_SEC1_PC_ADDR32] = MEM_TRISC1_FIRMWARE_BASE;
    // cfg_regs[TRISC_RESET_PC_SEC2_PC_ADDR32] = MEM_TRISC2_FIRMWARE_BASE;
    // cfg_regs[TRISC_RESET_PC_OVERRIDE_Reset_PC_Override_en_ADDR32] = 0b111;
    // cfg_regs[NCRISC_RESET_PC_OVERRIDE_Reset_PC_Override_en_ADDR32] = 0x1;
#endif
}

inline void initialize_tensix_semaphores() {

    TTI_SEMINIT(1,0,ckernel::semaphore::UNPACK_TO_DEST);
    TTI_SEMINIT(1,0,ckernel::semaphore::MATH_DONE);
}

void device_setup() {

    volatile tt_reg_ptr uint32_t* cfg_regs = reinterpret_cast<uint32_t*>(TENSIX_CFG_BASE);

#ifdef ARCH_BLACKHOLE
    *((uint32_t volatile*)RISCV_DEBUG_REG_DEST_CG_CTRL) = 0;
#endif

    #ifdef ARCH_BLACKHOLE // TODO see what happens with WH
    WRITE_REG(RISCV_TDMA_REG_CLK_GATE_EN, 0x3f);  // Enable clock gating
    set_deassert_addresses();
    // Invalidate tensix icache for all 4 risc cores
    cfg_regs[RISCV_IC_INVALIDATE_InvalidateAll_ADDR32] = RISCV_IC_BRISC_MASK | RISCV_IC_TRISC_ALL_MASK | RISCV_IC_NCRISC_MASK;
    #endif

    // Clear destination registers
    #ifdef ARCH_BLACKHOLE
    TTI_ZEROACC(p_zeroacc::CLR_ALL, 0, 0, 1, 0);
    #else
    TTI_ZEROACC(p_zeroacc::CLR_ALL, 0, 0);
    #endif

    // Enable CC stack
	TTI_SFPENCC(3,0,0,10);
	TTI_NOP;

    // Set default sfpu constant register state
	TTI_SFPLOADI(p_sfpu::LREG0,0xA,0xbf80); // -1.0f -> LREG0
	TTI_SFPCONFIG(0, 11, 0); // LREG0 -> LREG11

    initialize_tensix_semaphores();

}

int main(){
    device_setup();
    for(;;){}
}