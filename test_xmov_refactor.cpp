#include "tt_llk_wormhole_b0/common/inc/ckernel_xmov.h"
int main() { 
    ckernel::xmov_set_base(0x1000); 
    return 0; 
}
