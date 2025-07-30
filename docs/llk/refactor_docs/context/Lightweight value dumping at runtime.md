
The usual way to report some values from kernels running on the device is DPRINT when using metal or ad-hoc breadcrumbs in L1.  
This lightweight dumping feature aims to address two biggest pain points of the above solutions, namely performance impact of DPRINT which makes it hard to reliably reproduce bugs with it enabled and lack of structure and ease of use of ad-hoc breadcrumbs.

All features discussed here are currently available on `lpremovic/dbus-spec` (LLK Repo) and `lpremovic/dbus-dump` (Metal Repo) and are tested on BH but should work on WH too

### Principle of operation

Lightweight dumping uses a free piece of L1 to store its values which are then read on the host by a python script built on top of tt-exalens.

Right now the memory usually used for DPRINT is repurposed for this (making usage of this and DPRINT exclusive, but if you are using this you probably already tried DPRINT and it didn't work so it shouldn't be a problem).

// TODO Metal has allocated a fixed 1K for LLK debug, change which buffer is used to this.

Additionally it only needs a one or two global variables that are then declared extern in `ckernel.h` (See `ckernel.h` and `trisc.cc` in example)

Each core gets a 50 word buffer that can store arbitrary data + a single word header that contains two 16bit id’s.

Higher 16 bits are a type id which is used to differentiate different dumps which contain different types of data, for example dump with type id 1 might contain semaphore status in words 0-6 while dump with type id 2 might contain risc pc’s in words 0-2.

Lower 16 bits are primarily used to signal to the host script that a dump is ready (host concludes that a dump is ready when the lower 16 bits are different from what was last read) and are by default incremented each time a dump is performed, additionally they can be used to differentiate where the dump occurred if dumping from multiple places with the same type id (bear in mind that if passing explicit value, two consecutive dumps **MUST** use a different value otherwise they will not be captured properly)

### Usage

Call `inline void dbg_init_runtime_dump()` at least once before starting to dump values.

Call `inline void dbg_write_runtime_dump(const uint32_t offset, const uint32_t value)` to write a particular value at a particular offset.

Call `inline void dbg_close_runtime_dump(const uint32_t type, int32_t code = -1)` with a dump type that signifies what values were dumped between the last call to close dump and this one and optionally a custom code that goes into the lower 16 bits of the header.

An example can be seen in `trisc.cc` in the feature branch.

**Host side script**

An example host side script can be found in `runtime_dump_watcher.py` in root of metal repo, it requires that tt-exalens is available in the active python environment.

The script starts with user configuration section which specifies the base address of the buffer used (0x3C8 for DPRINT buffer at the time of writing), section of dictionaries that describe what data is located at each offset in order to provide a more pleasant output (as well as where dump data is located inside a buffer), a type dictionary which maps data description dictionaries to particular type id’s and lists of cores (specified using logical coordinates) and riscs (0, 1 and 2 correspond to unpack math and pack triscs respectively) inside those cores which to monitor.

The script itself enters a loop where it checks the header of each trisc on each core and if the header differs from the last read it gets the type id, then reads and prints all words specified in the data description dictionary for that type id.