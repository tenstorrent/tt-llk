static constexpr uint32_t timestamp_address  = 0x00019000;
static constexpr uint32_t buffer_address     = 0x00020000;
static constexpr uint32_t bucket_address     = 0x00030000;
static constexpr uint32_t wall_clock_address = 0xFFB121F0;
static constexpr uint32_t start_address      = 0x00019FF0;
static constexpr uint32_t regfile_address    = 0xFFE00000;
static constexpr uint32_t pc_buf_address     = 0xFFE80000;

constexpr uint32_t ITERATIONS = 200;

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

 void run_kernel(){

    volatile uint32_t a  = 123;
    volatile uint32_t b  = 4574579;

    start_measuring();

    for(int j = 0; j < ITERATIONS; j++) {
        res1 = a*b;
        res2 = c*d;
    }

    stop_measuring();

 }