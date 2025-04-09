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

int main() {

    // Use a volatile variable to prevent the compiler from optimizing away the loop
    volatile bool run = true;
    
    // Infinite loop
    while (run) { }
}