​ The purpose of this document is to provide detailed overview of the Tensix processor architecture and emphasize everything needed to generate functional and efficient code. It is important to note that this document doesn’t describe data movement methodology between Tensix cores, but rather how to process data that is already local.

### Tensix Core Architecture

Tensix core is a processor highly efficient in executing tensor mathematical operations commonly seen in neural networks. It consists of several building blocks that are illustrated in Figure 1: 
- 5 RISCV processors 
- Tensix engine for efficient execution of tensor operations 
- NOC sub-system for efficient data movement between Tensix cores 
- SRAM memory (L1) that all sub-systems in Tensix could use for various purposes
Tensix processor and its sub-components operate on data resident in L1 memory and it is assumed that at the very beginning of Tensix core operation all of data is already local and resident in L1. NOC sub-system and data movement layer of software are responsible for moving the data between Tensix cores. Besides tensor data, L1 also stores everything else needed for proper functioning, like various program binaries or random data structures. Programing of Tensix core is done through RISCV processors, while high-throughput tensor operations like matrix multiplication, convolution, elementwise addition/multiplication are offloaded to Tensix engine.
Tensix engine is a multi-threaded, single issue, in-order processor with custom instruction set (Tensix ISA). It implements temporal multi-threading with 3 threads and multiple execution units (EXUs). The key components containing math units are Compute EXUs and rest of the engine consists of data movement components grouped under hierarchy called TDMA. Data movement in Tensix engine goes in two directions: 
- L1 -> Compute FPU, aka unpacking – bring data from L1 into source registers 
- Compute FPU -> L1, aka packing – take computed data from dest registers and store into L1  
TDMA EXUs work together to provide maximum utilization of Compute FPU EXU. Unpacker EXUs will move data from L1 into src registers, Compute FPU will consume data from src registers, do the operation and store results into dst registers and finally, Packer EXU will move data from dst registers into L1. The goal of Tensix core is to multiply numbers so 3 components described must work in balance to keep the core utilization high. That is the main reason why there are 3 threads in the Tensix engine and 3 TRISC cores that control them: 
- Tensix engine thread 0, controlled by TRISC0 – specialized to unpack data (L1 -> src registers) 
- Tensix engine thread 1, controlled by TRISC1 – specialized to compute data (src registers -> math -> dst registers) 
- Tensix engine thread 2, controlled by TRISC2 – specialized to pack data (dst registers -> L1)
​ 
##### Data formats

Data can be stored using different formats which allow the programmer to choose smallest precision that fits their application. Complete list of formats and their description can be found here: *if needed ask for it*

Some of the key highlights of different formats are: 

- Different ranges through multiple exponent widths: 
- 5bit exponent: range -1.99 * 2^-15 to 1.99 * 2^15 
- 8bit exponent: range -1.99 * 2^-127 to 1.99 * 2^127 
- Different mantissa precision: 
- From 1bit to 23bits 
- Shared exponent per 16 numbers: 
- 16x numbers with 1b/3b/7b mantissa share a single 5 or 8bit exponent
​ 
- Integer numbers support: 
- Int8/Uint8 – signed numbers in format sign + magnitude 
- Int16/Uint16 – signed numbers in format sign + magnitude 
- Int32 – sign + magnitude
​ 
It is important to note that not all sub-blocks of Tensix core can support all formats for data storage. In L1 memory all formats can be used, but once data is unpacked and stored into compute src/dst registers, only a few formats are used to represent it. Tensix EXUs recognizing different formats will have more details about which formats they support and under which conditions.

##### Math fidelity

One more important aspect of Tensix engine is the fidelity of multiplication operations. Tensix does floating point multiplications which is illustrated below: 

- 𝐹𝑝_𝑛𝑢𝑚0= 𝑚𝑎𝑛𝑡0∗2^𝑒𝑥𝑝0; 𝐹𝑝_𝑛𝑢𝑚1= 𝑚𝑎𝑛𝑡1∗2^𝑒𝑥𝑝1;Fp_num0= mant0∗2^exp0; Fp_num1= mant1∗2^exp1;
- 𝐹𝑝_𝑛𝑢𝑚0∗ 𝐹𝑝_𝑛𝑢𝑚1=𝑚𝑎𝑛𝑡0∗𝑚𝑎𝑛𝑡1∗2^((𝑒𝑥𝑝0+𝑒𝑥𝑝1));Fp_num0∗ Fp_num1=mant0∗mant1∗2^((exp0+exp1));

The multiplying part of the calculation mant0*mant1 is done using hardware multipliers. Compute FPU EXU is designed to have 2048 hardware multipliers to efficiently do matrix multiplication, but they aren’t wide enough to handle widest mantissas that Tensix supports. The idea behind this decision is that calculations in AI applications are tolerant to precision loss and that multiplications can be done with lower fidelity, i.e., with multiplication products formed using less input mantissa bits. In Wormhole B0, multipliers are 5x7 wide which means that they could handle 5bx7b multiplication in a single cycle. For some smaller data formats (Bfp4, Bfp2, Lf8) that may be enough to multiply all the useful bits of mantissa bits, but for many formats it wouldn’t. That is why the concept of higher fidelity multiplication is introduced, where we use different mantissa bits over multiple cycles of hardware multiplications and accumulate results. It is up to the programmer to control the fidelity of the calculations using the fidelity phases that are defined by the engine. The phases are explained in Figure 2. Doing 4 fidelity phases provides the result that is of full precision for all formats Tensix supports.

##### Programmability

There are 5 RISCV processors that could be used to program Tensix core, split into 3 categories: 

- 3x TRISC processors – 3x compute processors 
- 1x BRISC processor – data movement processor 0 
- 1x NRISC processor – data movement processor 1 

All RISCVs are similar in design, with the difference being that TRISCs can efficiently drive Tensix engine. TRISC cores have been modified to accept Tensix engine instructions natively and translate them to MMIO stores to Tensix engine instruction buffer associated with that TRISC (Figure 1). Tensix instructions could be embedded into TRISC binary, or could be constructed dynamically by doing a store to a MMIO location of the instruction buffer. Obviously, instructions with parameters known at compile time will be embedded into the binary and be much faster to execute and that is what programmers should strive for. 

Utilization of Tensix engine (and therefore hardware math) is directly dependent on how efficient instructions can be issued to it. Any bubbles introduced by instruction fetching, loop handling, conditional statements, branch misprediction, … could directly impact rate of issuing instruction to Tensix engine which in the end could hurt utilization. That is why TRISC sub-systems have hardware mechanisms to issue certain sequences of instructions, usually referred to as macro-operations (MOP). Those hardware MOP blocks are pre-programmed before data starts to stream through and all TRISC has to do is a single MMIO store. That way TRISC can offload issuing of individual Tensix instructions to the hardware MOP block while it progresses further with its code. That is general idea how TRISC can handle all the inefficiencies that affect its IPC while keeping high IPC of Tensix engine.  

More on MOPs and other methods of optimized issuing of instructions to Tensix engine in the dedicated section about RISCVs.
​ 
##### RiscV sub-system

There are 5 RISCV processors in Tensix core: BRISC, NRISC and three TRISCs. They are very similar in design, with differences mostly in configuration and some interfaces. Program code for every RISCV resides in L1. Except for BRISC, all other RISCVs can start executing at any L1 address, which is controlled through reset vector registers (Table 1). Using listed registers, the programmer could load program code to any L1 address for NRISC and TRISCs; in case of BRISC the program always must be at address 0x0.

TRISC_RESET_PC_SEC0_PC - CFG - Global - Start Address of TRISC0 Code
TRISC_RESET_PC_SEC1_PC - CFG - Global - Start Address of TRISC1 Code
TRISC_RESET_PC_SEC2_PC - CFG - Global - Start Address of TRISC2 Code
NRISC_RESET_PC_RC - CFG - Global - Start Address of NRISC Code
TRISC_RESET_PC_OVERRIDE_Reset_PC_Override_en - CFG - Global - Start address override enable for TRISCs
NCRISC_RESET_PC_OVERRIDE_Reset_PC_Override_en - CFG - Global - Start address override enable for NRISC

Block diagram of RISCV sub-system is showed in Figure 3 and Figure 4. 

All 5 RISCVs have the same structure, with the exceptions listed below: 

- TRISCs have all the blocks and connectivity as illustrated in Figure 3 and Figure 4 
- BRISC differences: 
- Doesn’t have MOP decoder from Figure 4, but can still push to Tensix instruction buffers 
- Doesn’t recognize Tensix instructions as native 
- NRISC differences: 
- Doesn’t have interfaces to: Tensix GPRs, Tensix CFG, RISC mailboxes, Tensix Instructions 
- Doesn’t have MOP decoder from Figure 4, can’t push to Tensix instruction buffers 
- Doesn’t recognize Tensix instructions as native

All RISCVs contain local data memory that sits physically close to the processor and has the lowest access latency. It is limited in size and should be used for most frequently used variables. In BUDA SW stack, it is used to store stack, compile time parameters and globals. Ideally, low level program should never go out of local memory and into L1 to fetch data, as latency to L1 is on the order of 5-7 clock cycles.  

Instruction cache is another component that every RISCV has. It is a 2-way set-associative instruction cache with LRU eviction policy. Cache line size is 16B, enough for 4 instructions. On a cache miss, instructions will be fetched from L1 and stored in cache. iCache also contains a pre-fetcher logic which can pre-fetch in two modes: next line, or in case of a jump, predicted jump. Both modes could be enabled/disabled. Note: In Wormhole, pre-fetcher doesn’t seem to work well, Black Hole should have it fixed.

##### RISCV configurations

Configurations for all RISCVs are listed in Table 2. It is worth mentioning that NRISC has another memory in the sub-system called IRAM (16KB) used for holding program code (hence, smaller cache). However, IRAM is deprecated in future chips so only Wormhole/Grayskull will be using it.

As mentioned earlier, TRISC has been designed to natively recognize Tensix instructions and translate them as MMIO stores directly to Tensix instruction buffer. Tensix instructions are added to RISCV code by using macros: 

- TTI_<instruction_name>(<instruction_arguments>) 
- TT_<instruction_name>(<instruction_arguments>) 
The file containing definitions of those macros is ckernel_ops.h, it is pre-generated using the Tensix assembly file (`<arch>/assembly.yaml') and it shouldn’t be hand-modified. Tensix assembly can be found at the location below and it contains detailed instruction information and respective fields:
`$BUDA_BACKEND_ROOT/src/meta/<arch_name>/instructions/yaml/assembly.yaml`
The difference between the two approaches of inserting instructions is such that `TTI_<inst_name>` will resolve as compile-time constant therefore embed the Tensix instruction (32bits) into the instruction stream. This method of executing Tensix instructions is faster and preferred from the programming perspective, but to use it all instruction arguments must be compile-time constants. In cases when that is not the case, other macro `TT_<inst>` will translate to set of shifts, ANDs, ORs to create the 32bit instruction and finally issue a store to instruction buffer address.

##### MOPs

Rate of issuing instructions directly impacts performance of Tensix engine. That is illustrated below:
*Example of a loop unrolling*

TRISC pseudo-code and pseudo-assembly show TRISC execution cycles. Assuming that inst0 to instN fall under the outer most loop, rate of instructions that Tensix engine sees will be less than 100% and utilization will depend on the ratio between Tensix and non-Tensix instructions.  

That is why few macro-op concepts have been designed in TRISC hardware. Macro-op (MOP) hardware has been added (MOP Decoder from Figure 4) which can be programmed ahead of time so that it issues instructions with the specific pattern on a single “go” instruction from TRISC. Main idea behind this is to decouple TRISC instruction issue rate from Tensix instruction issue rate such that TRISC can still spend cycles handling loop overheads, conditional code, etc.. while Tensix engine receives instructions at highest rate.  

The patterns of interest are illustrated in Figure 5.

2 MOPs in RISC instruction set:
* General-Purpose MOP
* Unpack specific MOP
```asm
# GENERAL-PURPOSE MOP
LOOP_OUTER: <OUTER_LOOP_COUNT>
	START OP
	LOOP_INNER: <INNER_LOOP_COUNT>
		LOOP OP1
		LOOP OP2
	END LOOP INNER
	END OP1
	END OP2
END LOOP OUTER

# UNPACK-SPECIFIC MOP
LOOP:
	IF(ZMASK[ITERATION])
		UNPACR A0
		UNPACR A1
		UNPACR A2
		UNPACR A3
		UNPACR B
	ELSE
		SKIP A0
		SKIP A1
```
​ 
General-purpose MOP is available on all TRISC cores, while Unpack-specific MOP is available on TRISC0 only (not used in current production software).  Unpack specific MOP is also available from all TRISCs, and is in-fact not unpack specific. However, the pattern of execution is different than general purpose MOP and usually more suited for unpacking. 

General-purpose MOP instructions should be self-explanatory with all parameters programable, some of them optional:
- OUTER_LOOP_COUNT – 1 to 127 
- INNER_LOOP_COUNT – 1 to 127 
- START_OP0 – optional, any Tensix instruction 
- LOOP_OPx – optional, any Tensix instruction  
- END_OPx – optional, any Tensix instruction  
In addition to the listed parameters, it is also possible to modify instructions executed at the last iteration of inner or outer loop. For example, in a case where one instruction LOOP_OP0 = MVMUL with side-effects to increment dst address by 4, one could program inner loop end instruction modification such that at the last iteration of inner loop, we do dst address increment of 16. Register address side-effects are the most common use case for different instruction at the end of inner or outer loop, but it is worth noting that there are no restrictions with regards to what could be executed on the last inner/outer loop (it could be a completely different instruction).

​TRISC code becomes much simpler (less loops, less memory stores) while MOP decoder issues Tensix instructions at every clock cycle using pre-programmed MOP settings. For TRISC, TTI_MOP() is just a mem store so once its issued it will continue to crunch the code and move into the next iteration of a t loop. This allows TRISC to amortize the overhead of non-Tensix instructions over the time Tensix engine needs to execute all instructions. MOP decoder has a buffer through which it receives instruction from TRISC, so TRISC in this case can run as many iterations of t loop as needed to fill the MOP decoder buffer with instructions. That is a desirable programing model since it’s utilizing Tensix engine to the maximum and ideally it is TRISC that gets stalled because MOP decoder buffer is full.

​Very important note is that this  decouples execution of TRISC and Tensix engine and explicit sync barriers and checks are needed for TRISC to know when Tensix engine has completed everything that MOP issued. Although this statement can sound a bit scary, in reality TRISC doesn’t need to know when Tensix engine completes the instructions except in few isolated cases. Tensix engine is a multi-threaded processor, and it is desirable to use Tensix primitives for thread-synchronization and events that need to follow certain ordering. Those primitives include: 
- Thread barriers – e.g. wait while EXU is busy 
- Semaphores 
- Mutexes  
Thread barriers are very useful for executing ordered events, such as: pack data, wait for packer to complete, update output fifo pointers.

##### REPLAY instructions

Besides MOPs, one more way to speed up rate of issuing instructions to Tensix engine is by using a special instruction called REPLAY. REPLAY is a Tensix instruction on its own and TRISC doesn’t differentiate it any specific way. Special hardware block in the Tensix engine has been added to detect this instruction and, depending on the instruction parameter, either program the replay buffer or execute instructions from the buffer. The definition of the REPLAY instruction and meaning of instruction parameters can be found in the `<arch>/assembly.yaml`, however worth noting here are the rules how TRISC should use it. Programing model is such that once TTI_REPLAY instructions is issued with parameter load_mode = 1, Tensix instructions that follow REPLAY will be placed in a buffer. Another parameter size dictates how many instructions will be placed. For Wormhole B0, max number of instructions that can fit REPLAY buffer is 32. The first issuance of REPLAY instruction will program the buffer with instructions that follow (it is also possible to execute those instructions while loading if instruction param is

​set), while every subsequent invocation of REPLAY instruction will execute the instructions from the buffer.

MOPs and REPLAY instructions are not mutually exclusive. It is perfectly valid to program MOP such that some of the programed instructions is REPLAY instruction. Obviously, REPLAY will have to be programed before the MOP is executed.

##### L1 memory

L1 is an SRAM based memory shared between multiple blocks within Tensix core. It is a multi-bank, multi-port structure with round-robin arbitration in front of every bank.
L1 is organized in 16B words, with supported byte-enables on the write side. When doing reads, 16B are always returned as read data. There are no optimizations when doing 32bit reads from addresses that all fall to the same 16B word (that is fixed in Black Hole and future products). 

Number of access ports to L1 in Wormhole B0 is 16, however, almost every port is shared between multiple Tensix core clients. Example is illustrated in Figure 10.
The important take away is that L1 access latency from clients can be different and it will depend on the access pattern and activity in Tensix core.  

L1 banks are interleaved by default using the low order bits [3:0], that is 16 consecutive addresses always end up in a different bank, but interleaving is configurable, and any hash could be applied. 

L1 supports atomic instructions: ATINCGET, ATINCGETPTR, ATSWAP, ATCAS. Those are Tensix instructions, issued by the THCON EXU, and could be used for any purpose (currently not used). BRISC and TRISC can issue those as well by pushing them to Tensix instruction buffer.

##### Tensix engine

Tensix engine is multi-threaded, single-issue, in-order processor with custom instruction set (Tensix ISA). It has 3 threads that are usually fed from 3 different instruction streams (instructions pushed by corresponding TRISC). Instruction thread logic decodes the instruction, resolve stalls and finally at every clock cycle can arbitrate for any of the execution units (EXUs). Thread logic and context are described in Figure 11. Instruction buffer holds the instructions that TRISC pushes, and instruction decoder decodes them and decides which EXU to use. Once instruction is decoded, it will be placed in a buffer and instruction at the top of the buffer is the one that arbitrates for EXU. The instruction will not get popped from the buffer until EXU is ready and all subsequent instructions will be behind it. Once EXU is ready, the instruction will get popped from the buffer and next instruction comes to arbitrate. Every EXU is different and some of them have internal pipeline and further arbitration.  

Important note: EXU accepting the instruction doesn’t mean that instruction has completed, but it does mean that next instruction has come at the top of the buffer, to arbitrate. That next instruction may be targeting some other EXU and may get accepted and even finish before previous instruction did. Ordering of instructions is guaranteed only before they get committed to EXUs. Every EXU has different instruction pipeline, sources of stall, pipelined latency, and execution time so there’s no guarantee of ordering of completion of instructions from different EXUs.

In cases where specific ordering must be guaranteed, the programmer can use barrier instructions which will stall the thread instruction pipeline until a specific resource is busy. For that purpose, STALLWAIT instruction is used which blocks next instruction from arbitrating for EXU while a specific resource is busy.  
As an example, consider two actions that unpacker does:
1. Issue UNPACR that will unpack a tile 
2. Issue TDMA GPR write, once UNPACR has completed 
The naïve sequence would be:  
TTI_UNPACR(…); TTI_SETDMAREG(…);  
those will execute cycle after cycle and SETDMAREG will complete while UNPACR is still going on. That is because TTI_UNPACR(…) goes through a different Unpacker EXU pipeline and can take many cycles to execute (1 instruction can unpacker programable amount of bytes). Instructions will be popped from the buffer in order, but once they reach respective EXU’s pipelines, no more guarantees which one will finish first. The solution to this is to use a barrier instruction in between: 
TTI_UNPACR(…); TTI_STALLWAIT(THCON, UNPACK0); TTI_SETDMAREG(…); 
STALLWAIT instruction issues a stall for next THCON instruction, while UNPACKER0 EXU is busy. 
Tensix threads have context that consists of:
1. Config (CFG) registers. Private set of bits that could be used by that thread only
2. General-purpose registers. A bank of registers that could be viewed as 16 words of 16B, or 64 words of 4B

##### Tensix thread config

Thread CFG registers are listed in the `src/firmware/riscv/wormhole/wormhole_b0_defines/cfg_defines.h`, under the section “Registers for THREAD”. The bits have various meanings, and they can be used by specific thread only.

##### Tensix thread GPRs

Tensix GPRs serve general-purpose storage private to a thread. Register structure can be accessed through multiple clients (TODO: Try to find a good picture): 

1. BRISC and TRISCs. TRISC access GPRs attached to the thread they push instructions to.  
2. THCON EXU 
3. CFG EXU

GPRs can be used for following: 
- passing various settings from software to hardware (like programing configuration registers). The value would be written to GPR, then issue WRCFG Tensix instruction to move value from GPR to CFG register 
- Arguments for Tensix loads and stores from/to L1 (THCON instructions LOADIND, STOREIND). Other than L1, these instructions could target other parts of Tensix as well. 
- Arguments for Tensix arithmetic instructions – Bitwise OR, AND, XOR; MUL, ADD, SUB, SHIFT, CMP, etc.. where source and destination arguments are GPRs 
- Arguments for accessing RISCV MMIO registers through THCON – LOADREG, STOREREG 
- TODO: Is there more stuff this is used for? 

Being used by multiple Tensix EXUs and TRISC, GPR hardware ensures that proper arbitration happens at every access so due to port conflicts, it is possible that some instructions take variable number of cycles to complete. The programmer should be aware of that and always insert explicit barriers if there are dependencies between successive instructions. 

Use of GPRs is completely under programmer’s management. Usually, our software stacks will have a header file which assigns some GPRs to some specific purpose so other programmers don’t use them. It is important to view them as a shared resource so all programmers must be aware of that.

##### Tensix configuration

Tensix engine has a lot of configuration bits that comprise state, commonly referred to as config state (CFG state), whose purpose is to provide maximum flexibility and expose many features. These registers and bits are different from already mentioned Tensix thread config registers
because they represent a collection of registers shared among all EXUs. They reside in the CFG EXU and all threads can access them. They are duplicated, and programmer may choose which CFG set to use (i.e. the CFG State Id), 0 or 1.   

There’s lots of bit that configure Unpacker, Packer and Compute EXUs and programing them dynamically usually isn’t going to yield a high-performance code. General mode of programing should be: 

1. Configure bits – quick  
2. Stream data – long 
with the goal to spend as little time as possible configuring. Ideal place to configure would be once at the very beginning of the operation, however, in some cases config bits need to be reprogramed (i.e. Tensix engine reconfigured) to do some other feature during the execution of the operation. It is very important to pay attention to which bits are being reconfigured and proper timing. The programmer should always reprogram the minimum set of registers so least number of cycles are spent reprograming (perf impact), and make sure that updated bits don’t affect the instruction that may be executing at the same time in another EXU (functional impact). The best way to know whether changing CFG bits could affect an instruction in another EXU is to know how EXUs process instructions, how EXUs use CFG bits, whether they “latch” CFG bits once they commit to instruction and whether it's safe to change their CFG bits while execution is in progress. All of that should be in the programming guide.


##### CFG

Tensix CFG EXU is a collection of CFG state registers and thread CFG registers. The registers are defined in $BBE_ROOT/src/firmware/riscv/wormhole/wormhole_b0_defines/cfg_defines.h, with following sections:
- // Registers for THREAD
- // Registers for ALU 
- // Registers for PACK0 
- // Registers for UNPACK0 
- // Registers for UNPACK1 
- // Registers for THCON 
- // Registers for GLOBAL 
Section “Registers for THREAD” represent the thread CFG registers and Tensix instructions to access them is SETC16. Tensix is only allowed to write to them, while they can be read through TRISC/BRISC MMIO interface. Registers are 16bits wide, SETC16 instruction carries an immediate value to be programed into a register. Address space starts from 0, with address increments of +1 for next 16bit register. 

Main use case of these registers is to be accessed by Tensix instructions, dynamically, during the operation. Latency for SETC16 is just 1 clock cycles and as soon as instruction is popped from the instruction buffer so register bits become ready for the instruction in the pipeline. 

Other sections from the cfg_defines.h file represent state CFG registers. Registers are duplicated to expose 2 sets of state CFG registers which every Tensix thread can use. Each thread
​ 

​ independently selects which state to choose, and hardware will respect that by muxing the bits from selected state. State is controlled using thread CFG register at address 0 - CFG_STATE_ID_StateID_ADDR32. Both states are mapped to uniformed address space: 

- State 0: address range 0 – CFG_STATE_SIZE 
- State 1: address range CFG_STATE_SIZE – 2*CFG_STATE_SIZE 
State CFG registers can be written and read from both TRISC/BRISC MMIO and Tensix instructions WRCFG, RDCFG, RMWCIB*. Registers are 32bit wide, start from address 0 with address increments of +1 for subsequent 32bit register. There are different modes of writing into these registers: 
- From BRISC/TRISC – write or read using MMIO address, 32bit wide interface, 1 register at the time 
- From Tensix instructions: 
- Write operation – WRCFG instruction, which writes data from a Tensix GPR (supplied as argument) into a CFG register  
- Two modes of operation: 32bit (1 register) or 128bit (4 registers) 
- Needs 2 cycles to complete, once committed in EXU. Programmer must follow this instruction with NOP if it expects following instruction to use those bits (TODO: confirm with Syed that lack of port arbitration doesn’t affect this, i.e. instruction is committed once the ports are available) 
- Read operation – RDCFG instruction, reads data from CFG register and stores into Tensix GRP (supplied as argument) 
- Mode of operation: 32bit read 
- Needs 1 cycle to complete, the result is available in the GPR at the following cycle 
- Important note: There is a hardware bug when RDCFG and RISC MMIO read requests happen at the same time. RDCFG instruction shouldn’t be used at all in Wormhole B0, only RISC MMIO reads. 
- Read-modify-write operation – RMWCI* instructions, does read-modify-write on a CFG register 
- Mode of operation: 4 flavors of instruction, one for each byte of 32bit register RMWCI0, RMWCI1, RMWCI2, RMWCI3 
- Needs 1 cycle to complete, bits are available to use by the following instruction

​ Modifying both thread and state CFG bits affects hardware operation, and it is highly recommended to consider next few things when deciding how and when to update the bits: 

- Which EXU can be affected by those bits?  
- Is it safe to modify CFG bits when that EXU currently executes that instruction? If not, insert a STALLWAIT barrier to ensure bits don’t get modified unless EXU is idle 
- Which method of access to use – RISC MMIO or with Tensix instructions? 
- Use RISC MMIO – this is a sideband interface to CFG registers which comes from RISC’s store instruction so there’s no guaranteed ordering between that MMIO access and any Tensix operation. Tensix may have uncompleted instructions in the pipeline when MMIO access to CFG registers happen, effectively changing the CFG bits before some previously pushed instructions get to execute.  
The programmer must ensure that changing CFG bits through MMIO is safe, regardless of current state of Tensix engine (instruction pipeline, EXUs having instructions or no). This is usually possible in early stages of configuration when there’s no data streaming, just config. 
- Use Tensix instructions – Tensix instructions ensure ordering with other instructions in the Tensix instruction pipeline and when coupled with STALLWAIT barriers they can guarantee ordering between events that result from different EXUs.  
It is recommended that all dynamic reconfigurations that happen when data is streaming to be done using Tensix instructions


##### SYNC

SYNC EXU has the implementation of primitives used for synchronization: 
- barriers for synchronization within the same thread 
- semaphores and mutexes for cross-thread synchronization 
Barriers are invoked through instruction STALLWAIT which accepts 2 arguments: wait resource and stall resource. The instruction will ensure that next instruction that targets stall resource gets halted at the top of the buffer while the wait resource is busy. Important thing to note is that next instruction targeting stall resource isn’t necessarily next instruction in the queue. 
```cpp
TTI_UNPACR(...) // unpacker exu instruction
TTI_STALLWAIT(THCON, UNPACKOIUNPACK1) // STALL NEXT THCON INSTRUCTION, WHILE UNPACK0 OR UNPACK1 ARE BUSY!
TTI_SETC16(<SOME_REG>) // CFG EXU INSTRUCTION
TTI_SETDMAREG(...) // THCON INSTRUCTION
```

STALLWAIT instruction is indicating that next instruction targeting THCON EXU should be stalled at the top of the buffer while UNPACK0 or UNPACK1 are busy. The instruction immediately following STALLWAIT is SETC16 (targeting CFG EXU), therefore that instruction will not be stalled, and it will be executed. After SETC16 comes SETDMAREG which targets THCON EXU and if at that moment either Unpacker0 or Unpacker1 are busy, the SETDMAREG instruction will get stalled at the top of the buffer.
Complete list of stall resources and wait resources are in the Tensix ISA document (assembly.yaml). It is possible to assign more than one stall resource and more than one wait resource. Nesting STALLWAIT instructions is not supported in hardware, unpredicted errors could happen.
Tensix semaphores primitives used for synchronizations between Tensix threads and between TRISC and Tensix engine. There’s two ways to manage them: 
- Using Tensix instructions: SEMINIT, SEMPOST, SEMGET, SEMWAIT 
- Using TRISC MMIO 
Important to note is that Tensix semaphores aren’t “true” atomic primitives since they behave differently from semaphores usually described in computer architecture books. Main difference is that there are no “acquire semaphore operation” and semaphore increment/decrement operations aren’t blocking but rather unconditional. No thread “owns” the semaphores and they are mostly used as counters indicating the availability of specific resource, like available space in registers. Explicit WAIT instructions are testing the semaphore and are blocking if condition isn’t met, while SEMPOST (increment) and SEMGET (decrement) are always unconditional.  

Semaphores have initial value and max value defined at initialization and WAIT instruction is basically allowing programmer to block thread execution if semaphore is either 0 or at max value. Consider the example in Figure 13. The example illustrates synchronization between thread 1 (Math thread) and thread 2 (Packer thread) on DEST registers. Math thread needs to compute results and store them into DEST registers while Packer thread will take the results and pack them into L1. Math thread signals completion of compute operation by incrementing the semaphore SEM1, while Packer thread signals completion of packing by decrementing the semaphore SEM1.

```CPP
// SEMAPHORE SYNC EXAMPLE

// MATH THREAD SYNC ON DEST REGISTERS
for(int i = 0; i<tiles; i++)
{
	TTI_SEMWAIT(SEM1, WAIT AT MAX);
	TTI_ELWADD(...);
	TTI_ELWADD(...);
	...
	TTI_SEMPOST(SEM1);
}

// PACKER THREAD SYNC ON DEST REGISTERS
for(int i = 0; i<tiles; i++)
{
	TTI_SEMWAIT(SEM1, WAIT AT ZERO);
	TTI_PACR(...);
	TTI_PACR(...);
	...
	TTI_SEMGET(SEM1);
}
```

SEMWAIT instructions in both cases are blocking – Math thread will block if semaphore is at max value, meaning that whatever has so far been computed isn’t packed yet and there’s no more DEST registers space to store next set of compute results; Packer thread will block if semaphore is at 0, meaning that Math thread hasn’t computed anything yet and there’s nothing to pack.  

Both increment and decrement, busy-wait and simple reading of semaphores is possible through TRISC MMIO. It is important to note that mixing semaphore operations through MMIO and Tensix instructions can be dangerous due to no guaranteed ordering between TRISC MMIO and Tensix engine execution, but if done carefully it could be used as well. For example, TRISC could increment the semaphore while Tensix instructions are used to decrement it. For example, this case is useful when managing config contexts. Config context represents a set of registers which for example Unpacker0 EXU uses, and let’s say there are 2 config contexts TRISC can program. TRISC needs to make sure that context is used by the Unpacker before it overwrites it so the following sequence could exist: 
- while (semaphore_read(SEMx) == 2);        // Read semaphore through MMIO 
- program_context_registers(); 
- semaphore_post(SEMx);                               // MMIO increment semaphore SEMx  
- TTI_UNPACR(..);                                              // Unpack0 EXU instruction 
- TTI_STALLWAIT(STALL_SYNC, UNPACK0); // Stall next SYNC instruction (SEMGET) 
- TTI_SEMGET(SEMx);                                      // Decrement SEMx
TTI_* instructions for TRISC are just MMIO stores so TRISC could program both contexts of registers entirely disregarding Tensix engine state. The code above will block TRISC execution if semaphore is indicating that both contexts are programed and Unpacker0 is still busy so TRISC has effectively done as it could to feed Tensix engine with. This is preferred way of programing so TRISC is never a bottleneck!

Mutexes are basically test-and-set atomic primitives which all Tensix threads can acquire to ensure a critical code section. There are 8 mutexes, any thread can access any mutex – no restrictions. Instruction to acquire mutex is ATGETM, to release it ATRELM.
​ 
##### COMPUTE FPU

Compute FPU hosts Tensix engine’s math logic: register files, high number of multipliers, adders, and accumulators. All math primitives are built to work natively with 2D tensors, providing max throughput of operations for matrix multiplication. Instruction and data path of the Compute FPU are illustrated in Figure 14. It consists of: 

- 2 source operand register files: SRCA regfile and SRCB regfile 
- 1 destination register file: DEST regfile 
- Compute FPU unit: collection of FPU tiles – multi-functional FPU units organized in a 8x16 grid 
- Instruction path with stall logic
​ 
##### srcA and srcB

Source operand register files are organized as 2D structure, 64 x 16 datums. Single datum is 19bits wide but depending on the format not all bits are going to be used. When smaller formats are used, padded zero-bits are added to each datum. The storage container for every datum is given in the Figure 15. Number of non-padded bits in every datum will depend on the unpacker destination format parameter.

Each datum is 19bits because Compute FPU widest source operands can be with 10b mantissa, 5 or 8bit exponent and 1b sign, effectively: float16, tf32 and int8, int16. All smaller formats will have zero-pad bits added, while wider formats like fp32 and int32 are not supported as source operands to the FPU. 

SRC registers store data produced by the Unpacker EXUs. Unpacker0 unpacks/writes data into SRCA, while Unpacker1 unpacks/write data into SRCB. This write interface is high throughput and contains embedded hardware synchronization between SRC registers and Unpackers so programmer doesn’t have to waste cycles on software synchronization. This applies when programing model is such that Tensix thread 0 is used to drive Unpacker EXUs and Tensix thread 1 is used to drive Compute FPU EXU – this is the programming model Tenstorrent recommends and implements. Embedded hardware synchronization consists of: 

- Double-buffering on SRC register files – 2 banks of each SRC register 
- Write_ready/Data_valid hardware signals 
Hardware is built such that every instruction executing in Compute FPU EXU will have designated meta data describing whether it should depend on one, both or none SRC operands. The meta data is described in the assembly.yaml file, field name “src_mask” – bit0 indicates dependency on SRCA, bit1 indicates dependency on SRCB; 0x3 is a valid setting as well. Programmer doesn’t have to worry about this level of synchronization as hardware is taking care of it automatically.  

For example, Tensix thread 1 can have just compute instructions while Tensix thread 0 could have just unpack instructions. Code snipped in Figure 16 illustrates highly efficient execution of Math thread that drives Compute FPU instructions because if unpacker can provide data at appropriate rate, math will always be occupied and 100% util will be achieved.

```cpp
// Figure 16 Thread 0 and 1 code relying on hardware synchronization

// TENSIX THREAD 0 - UNPACK
for(int i = 0; i<tiles; i++){
	TTI_UNPACR(...);
}

// TENSIX THREAD 1 - MATH
for(int i = 0; i<tiles; i++){
	TTI_MVMUL(...);
	TTI_MVMUL(...);
}
```
​ 
The bandwidth between each Unpacker and respective SRC register file is 64B/CLK, which determines the time needed to unpack the chunk of data. UNPACR instruction that initiates unpack operation is designed to trigger programmable amount of transfer into SRC registers and
depending on the unpacking format it could take more or less time to finish. For example, unpacking a 16x16 data chunk in fp16 format (2B per datum) would take 512B / 64BpCLK =  8 CLK cycles. It is important to note that even though each unpacker has a 64B/CLK  link to its SRC register, some ports between two unpackers are shared on L1 so when both SRCA and SRCB are being unpacked at the same time, effective BW that both unpackers can achieve to both SRC registers is 80B/CLK.  

Due to various non-idealities in the engine, maximum BW is not achievable, so it is recommended to move as bigger chunks of data into SRC registers as possible using single UNPACR instruction. That way, all non-idealities induced from every UNPACR instruction get amortized over longer transfer time.   

Each SRC register file has 64 entries, with entry being 16 datums wide. Compute FPU addresses SRC register file in rows, therefore selecting one or many rows to feed the instruction primitive. It is not possible to address parts of rows. Compute primitives would never select entire SRC register file for processing, it would always be a subsection, i.e. a window of register indices, which is selected using register-window counters (RWC). RWC counters represent the pointers to which register indices will be selected for next compute instruction, i.e. define the start of the next register window. The size of the window (how many register indices/rows will be selected) depends on the instruction itself as every instruction takes “all rows it needs”. 

​ 
There are separate REGW counters for every SRC register, and 1 for DEST registers as well. Programing them can be done explicitly through instructions SETRWC, INCRWC or implicitly as side-effects of some instructions (more on this later). REGW counters are not simple counters that can go up/down, they also have one more counter associated used for carriage return (CR) function. The function of CR and regw counter increment can be described through the pseudo-code from Figure 18.  

CR function can be useful when reusing parts of SRC register multiple times, like with matrix multiplication operation, where CR counter can be used to easily return to start of the reused block while incrementing CR counter would indicate a move to next block to be reused.
​ 

​ 
```cpp
// FIGURE 18, Regw counter operation - increment and increment with CR
if(cr_operation){
	regw_cnt = regw_cnt + increment;
	regw_cr_cnt = regw_cr_cnt + increment;
}
else{
	regw_cnt = regw_cnt + increment;
	regw_cr_cnt = regw_cr_cnt;
}
```


##### DST

DEST operand register file is organized similarly as SRC register files, with following differences: 

- More indices/rows 
- Two modes for datum width and effective size a programmer could use: 
- 16bit datums => size is 1024 rows X 16 datums
- 32bit / tf32 datums => size is 512 rows X 16 datums 

Compute FPU EXU has configuration registers that could be used to program one of two modes and depending on that programing the math instructions would produce result that will be wide enough to fit that DEST configuration. Main purpose of this is wide accumulation (fp32) while keeping SRC operands with less bits. Tensix engine programming model is such that configuration bit will determine width of the operation result and therefore mode of DEST registers, while the instruction itself would be unmodified. 

It is also possible to “force” wider datum format and keep math instructions producing 16bit outputs. This is particularly useful in cases when operands with different format width are mixed in DEST, for example fp16 and tf32/fp32/int32. Math instructions would in this “forced fp32 mode” still produce fp16 but store them into fp32 container for further usage (possibly by Compute SFPU EXU). 

DEST registers are addressed the same as SRC addresses, using register windows and REGW counters. All bells and whistles of SRC addressing exists with DEST addressing as well, the counters are just wider. Same instructions SETRWC and INCRWC should be used for manual manipulation, while moving the counters through side-effects is also possible. 

DEST registers are usually shared with packer, and they should be big enough to virtually split them in two halves and emulate double buffering. The idea is that Tensix thread 1 would compute and store results in one half, while packer packs out data from the other half into L1. If packing takes more time than computing results, the goal of keeping math 100% occupied should be achievable. To support the mode of double buffering CFG registers are added for both Compute and Packer EXUs so they can independently select which half they are targeting. Programing registers in the Table 6 will set the base value that will be added to the DEST RWC counter value to form the final address:
​ 
​ 
`dest_adr_fpu = dest_target_reg_cfg_math_offset + dest_rwc_cnt`
`dest_addr_pack = dest_target_Reg_cfg_pack_sec_offset + pack_req_address`

* DEST_TARGET_REG_CFG_MATH_Offset - cfg - thread - Base address of DEST registers used by FPU instructions
* DEST_TARGET_REG_CFG_PACK_SEC0_Offset - cfg - global - Base address of DEST registers used by packer0
* DEST_TARGET_REG_CFG_PACK_SEC1_Offset - cfg - global - Base address of DEST registers used by packer1
* DEST_TARGET_REG_CFG_PACK_SEC2_Offset - cfg - global - Base address of DEST registers used by packer2
* DEST_TARGET_REG_CFG_PACK_SEC3_Offset - cfg - global - Base address of DEST registers used by packer3

##### Compute SFPU

The Compute SFPU is a functional unit that is instantiated within the Compute FPU and is intended to complement the main math engine. The Compute SFPU can execute more complex functions that are not well suited for the main math engine (e.g. reciprocal, sigmoid, and exponential calculations), including support for conditional execution. Tenstorrent recommends and implements the programming model where Tensix thread 1 is used to drive the Compute SFPU, like for the Compute FPU. Unlike the Compute FPU, the Compute SFPU operates only on the DEST register, and requires the data to be loaded from DEST into one of its LREGs for use in computation and stored back from an LREG to the DEST manually.In Wormhole_B0, the Compute SFPU is instantiated 8 times within the FPU. The instances are arranged in a way that is aligned with the organization of the DEST Register, such that each instance of the Compute SFPU is connected to one instance of the DEST Register. Within each Compute SFPU instance, there are 4 lanes worth of independent computation, allowing the Compute SFPU to operate on 4 rows of the DEST at once. This brings the effective Compute SFPU computation instance count to 32. 

The array of Compute SFPU instances can be thought of as a SIMD engine, with each instance executing the same instruction sequence on its own local data and writing the results back to its own local section of the DEST Register. Within each instance there is a single instance of instruction decode logic, which is shared among the 4 rows of computation.  

The Compute SFPU implements a set of instructions which allow for the execution of complex mathematical and logical functions. The instruction issue block broadcasts an instruction which is received by and decoded inside of each instance of the Compute SFPU. Once the instruction is issued, the execution will happen on a fixed pipeline, without stalling. 

If the instruction is a load or store, the DEST Register will be accessed accordingly. In the case of a load, the data will be read from the DEST Register instance which is associated with each Compute SFPU and then returned to the Compute SFPU to be written into the specified LREG. In the case of a store, the Compute SFPU will access the specified LREG, format it accordingly, and the formatted data will be sent to the associated instance of the DEST Register where it will be written into the specified address. In Wormhole_B0, since there are 4 lanes per Compute SFPU instance, 4 rows will be loaded into the specified LREG simultaneously. Each of the 8 Compute SFPU instances will load 4 rows from its corresponding column within DEST. When loading/storing from addresses that are multiples of 4, the even columns of DEST will be loaded/stored for the 4 rows starting from that address. To load odd columns, a +2 offset can be added to the address, allowing all 16 datums per DEST row to be accessed. 

DEST registers are addressed the same as for the Compute FPU, using register windows and REGW counters. The same instructions SETRWC and INCRWC should be used for manual manipulation, while moving the counters through side-effects of instructions is also possible. 

The Compute SFPU includes a MAD block to perform floating point multiplication and addition. This block uses the FP32 format, which consists of 1 sign bit, 8 exponent bits, and 23 mantissa bits. The source operands are assumed to be in this format before utilizing the MAD block.
The Compute SFPU also implements condition code (CC) functionality which enables conditional execution of instructions. This conditional execution is useful for cases where the control flow is dependent upon the values within each Compute SFPU. This type of mechanism is necessary to enable conditional execution because each Compute SFPU instance sees the same instruction sequence issued by the RISC cores, but we sometimes want to perform different actions when the values are in certain regions. For example, if the kernel needs to perform certain operations only when the values being calculated are negative, the CC bits can be used to prevent those operations from being performed within the Compute SFPU instances that contain positive values, while allowing those operations to be performed within the Compute SFPU instances which contain negative values.  

The mechanism includes two state bits, which are combined to determine whether a given instruction will write its result into the specified destination LREG or into the DEST Register in case of a store instruction. The two bits are referred to as the condition code enable register and the condition code result register. When the condition code enable register is set to 0, conditional execution is disabled, and all instructions will be performed. When the condition code enable register is set to 1, subsequent instructions will only be performed if the condition code result register is also set to 1.
The following features exist within the Compute SFPU:  

- Local instruction decode logic to implement the Compute SFPU instruction set  
- Read and Write interface to the local DEST Register  
- 8 general purpose registers (per lane/row) which are 32-bits wide (LREGs) 
- 4 programmable FP32 constant values (shared across rows) which can be referenced by math operations  
- 4 hard-coded FP32 constant values which can be referenced by math operations
- A MAD module which implements a fused Multiply-ADD in FP32 precision (per row)  
- An execution unit which handles an array of “simple” operations (per row)  
- A stochastic rounding module which can perform rounding in hardware (per row)  
- Condition Code bits which can be used to enable or disable operations (per row)  
- Condition Code stack which can be used for nested conditional execution (per row)  
- Support for FP16 (fp16_a), bfloat (fp16_b), TF32, FP32, and integer formats  
- Fully pipelined execution path  
- Enhanced parallel execution support enabled by LOADMACRO mechanism  
- Enhanced support for sorting algorithms  
- Simultaneous operation of Compute SFPU and main math engine via independent instruction issue and execution pipelines   
- Detection and flagging of floating-point special numbers (NaN, infinity)

##### Address generation

Address counters shown on the diagram below are used for generating unpacker source and destination address. There is separate counter instance (context) for each thread and separate instance (context) per unpacker engine.  As part of each instance there are two set of counters, channel 0 and channel 1. Channel 0 counters are used for generating unpacker source address (L1 address) and channel 1 is used for generating unpacker destination address (SrcA and SrcB address). The actual connectivity and logic used for generating address is described in more details below. There are multiple ways to manipulate counter values:  

1. Using SETADC instructions to set counters to specific value  
2. Using INCADC instructions to increment counters by the amount specified in the instruction field
3. Using address mod (ADDR_MOD) as a side effect of the unpacker instruction to increment counters by the amount set in the bits 22:15 of the unpack instruction. This mode is limited to Y and Z counters.  
4. Using REG2FLOP thread controller instruction to set counters to the value specified in the GPR. This is backdoor access that provides maximum flexibility and allows any thread to set counters values for all threads while ADC instructions are limited to the thread on which the instruction is executed. In general there is no valid use case where this mode would be required and it hasn’t been verified on silicon.

Base address and stride values are set via per thread registers that can programmed using RISC MMIO writes to config space or using Tensix WRCFG or RMWCIB instructions. MMIO writes are not in sync with tensix instruction pipeline and are safe to be used only when unpacker engine is idle (e.g. after reset as part of initial configuration).  Tensix instructions are intended for dynamic manipulation of values during runtime before unpack instruction is executed. It’s safe to update base address or strides just before unpack instruction (adding 2 NOPs after WRCFG is still required to make sure register value is updated and latched by the next unpack instruction).
RMWCIB(REG_1_Y_STRIDE) -> program channel 1 y stride 
SETADCXY(CH1_Y_CNT = 0)  -> set channel 1 y counter value to 0 (y_cnt = 0) 
UNPACR(ADDR_MOD[3:2] = 2’b02) -> At this point unpacker will latch CH0_ADDR which is generated using y counter value 0 and programmed stride. As address is latched, y counter will be incremented  by 2 in the same cycle (y_cnt = 2) 
UNPACR(ADDR_MOD[3:2] = 2’b00) -> At this point unpacker will latch CH0_ADDR which is generated using y counter value 2 and programmed stride. Counter value remains unchanged. 
SETADCXY(CH1_Y_CNT = 1)   -> set channel 1 y counter value to 1 (y_cnt = 1)


##### Carriage Return (CR) Counter


CR is additional counter used for easy return to previous value (value before incrementing). When SETADC instruction is used,  both CR and position counters will be set to the same value. Increment instruction or ADDR_MOD will update position counter value only while CR will remain unchanged. Using ADDRCRXY or ADDRCRZW instructions position counter can be set to previous value:

```
cnt <= cr + cr_inc
cr <= cr + cr_inc
```

CR counter is not used in the current software stack.

