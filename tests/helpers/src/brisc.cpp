//#include "ckernel.h"
//#include "ckernel_helper.h"
#include "ckernel_main.h"
#include "tensix.h"
#include "ckernel_instr_params.h"
#include "ckernel_ops.h"
#include "risc_attribs.h"
#include "ckernel_structs.h"
#include "dev_mem_map.h"

using namespace ckernel;

static constexpr uint32_t core_idx = 3;

static constexpr uint32_t timestamp_address  = 0x00019000;
static constexpr uint32_t start_address      = 0x00019FF0;
static constexpr uint32_t buffer_address     = 0x00020000;
static constexpr uint32_t bucket_address     = 0x00030000;
static constexpr uint32_t wall_clock_address = 0xFFB121F0;
static constexpr uint32_t regfile_address    = 0xFFE00000;
static constexpr uint32_t pc_buf_address     = 0xFFE80000;

static constexpr uint32_t buffer_size      = 32;
static constexpr uint32_t num_cores        = 4; 
static constexpr uint32_t pipeline_factor  = 1;
static constexpr uint32_t version          = 0;

static constexpr uint32_t buffer_per_core  = buffer_size / num_cores;
static constexpr uint32_t outer_iterations = buffer_per_core / pipeline_factor;
static constexpr uint32_t timestamp_base   = timestamp_address +               4 * sizeof(uint32_t) * core_idx;
static constexpr uint32_t buffer_base      = buffer_address    + buffer_per_core * sizeof(uint32_t) * core_idx;

inline void start_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i" (wall_clock_address): "a3");
    asm("la a4, %0" : : "i" (timestamp_base) : "a4");
    asm("lw a5, 0(a3)");
    asm("sw a5, 0(a4)");
    asm("lw a5, 8(a3)");
    asm("sw a5, 4(a4)");
    asm("fence");
}

inline void stop_measuring()
{
    asm("fence");
    asm("la a3, %0" : : "i" (wall_clock_address): "a3");
    asm("la a4, %0" : : "i" (timestamp_base) : "a4");
    asm("lw a5,  0(a3)");
    asm("sw a5,  8(a4)");
    asm("lw a5,  8(a3)");
    asm("sw a5, 12(a4)");
    asm("fence");
}

constexpr uint32_t ITERATIONS = 1000;

int main() {

    volatile uint32_t a  = 123;
    volatile uint32_t b  = 4574579;
    volatile uint32_t c  = 1234;
    volatile uint32_t d  = 45714579;
    volatile uint32_t res1;
    volatile uint32_t res2;

    start_measuring();
    for (uint32_t j = 0; j < ITERATIONS; j++)
    {
        res1 = a*b;
        res2 = c*d;
    }
    stop_measuring();

    res1 = 20;
    res2 = 20;
    
    // Infinite loop

    volatile bool run = true;

    while (run) { }
}