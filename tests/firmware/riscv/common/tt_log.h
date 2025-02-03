// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#pragma once

// Undef previous potential dummy definition in hlk_api.h
#ifdef TT_LOG_DEFINED
    #undef TT_LOG
    #undef TT_LOG_NB
    #undef TT_PAUSE
    #undef TT_RISC_ASSERT
    #undef TT_LLK_DUMP
    #undef TT_DUMP_LOG
    #undef TT_DUMP_ASSERT
    #undef TT_LOG_DEFINED
#endif

// We need a dummy TT_LOG and it's derivates(assert, etc.) definition in hlk_api.h, in order for backend build to successfully compile.
// This define here tells us that TT_LOG is defined in this file, so we don't have to redefine it in in further includes. 
#define TT_LOG_DEFINED

#if defined(ENABLE_TT_LOG) || defined(ENABLE_TT_LLK_DUMP)
#include "fw_debug.h"
#endif

#include "l1_address_map.h"

// Maybe extern these to allow thread-safe logging
// MAILBOX_ADDR is defined for TRISC0, TRISC1, TRISC2
#ifdef MAILBOX_ADDR
  constexpr uint32_t tt_mailbox_base = MAILBOX_ADDR + l1_mem::address_map::TRISC_TT_LOG_MAILBOX_OFFSET;
  constexpr uint32_t tt_mailbox_size = l1_mem::address_map::TRISC_TT_LOG_MAILBOX_SIZE;
  static_assert(tt_mailbox_base == l1_mem::address_map::TRISC0_TT_LOG_MAILBOX_BASE ||
                tt_mailbox_base == l1_mem::address_map::TRISC1_TT_LOG_MAILBOX_BASE ||
                tt_mailbox_base == l1_mem::address_map::TRISC2_TT_LOG_MAILBOX_BASE,
                "TRISC_TT_LOG_MAILBOX_BASE is not defined for this TRISC");
#else
  constexpr uint32_t tt_mailbox_base = l1_mem::address_map::FW_MAILBOX_BASE;
  constexpr uint32_t tt_mailbox_size = l1_mem::address_map::FW_MAILBOX_BUF_SIZE;
#endif

template <uint32_t Locator, bool Blocking>
void ttlog() {
  auto log_mailbox = reinterpret_cast<volatile uint32_t tt_l1_ptr *>(tt_mailbox_base);

  // Wait for another thread to yield the mailbox
  if constexpr (Blocking) {
    while (log_mailbox[0] == 0xC0FFEE) {};
  }

  // Record the message and take a coffee break
  log_mailbox[1] = Locator;
  log_mailbox[0] = 0xC0FFEE;

  // Wait for host to clear mailbox
  if constexpr (Blocking) {
    while (log_mailbox[0] != 0) {};
  }
}

template <uint32_t Locator, bool Blocking, typename... Args>
void ttlog(Args... args) {
  auto log_mailbox = reinterpret_cast<volatile uint32_t tt_l1_ptr *>(tt_mailbox_base);

  // Wait for another thread to yield the mailbox
  if constexpr (Blocking) {
    while (log_mailbox[0] == 0xC0FFEE) {};
  }

  constexpr uint32_t max_args = tt_mailbox_size / sizeof(uint32_t) - 2;
  constexpr uint32_t num_input = sizeof...(Args);
  constexpr uint32_t num_args = num_input < max_args ? num_input : max_args;

  uint32_t arr[] = { static_cast<uint32_t>(args)... };
  for (uint32_t i = 0; i < num_args; i++) {
      log_mailbox[i + 2] = arr[i];
  }

  // Record the message and take a coffee break
  log_mailbox[1] = Locator;
  log_mailbox[0] = 0xC0FFEE;

  // Wait for host to clear mailbox
  if constexpr (Blocking) {
    while (log_mailbox[0] != 0) {};
  }
}

#ifdef ENABLE_TT_LOG

// Blocking always waits for host clear before and after writing to mailbox
#define TT_LOG(fmt, ...) ttlog<FWLOG_HASH(fmt), true>(__VA_ARGS__)

// Non-blocking skips wait for clear, and hence overwrite or be overwritten
#define TT_LOG_NB(fmt, ...) ttlog<FWLOG_HASH(fmt), false>(__VA_ARGS__)

// Same as TT_LOG blocking, but host will insert pause before clearing mailbox
#define TT_PAUSE(fmt, ...) TT_LOG(fmt, __VA_ARGS__)

// Same as TT_LOG blocking, but host will assert if condition is not met and remain silent otherwise
#define TT_RISC_ASSERT(cond, fmt, ...) do { if (!(cond)) ttlog<FWLOG_HASH(fmt), true>(__VA_ARGS__); } while (0)

#else

#define TT_LOG(...) (void)sizeof(__VA_ARGS__)
#define TT_LOG_NB(...) (void)sizeof(__VA_ARGS__)
#define TT_PAUSE(...) (void)sizeof(__VA_ARGS__)
#define TT_RISC_ASSERT(...) (void)sizeof(__VA_ARGS__)

#endif

namespace ckernel {
void debug_dump(const uint8_t *data, uint32_t byte_size);

#ifdef ENABLE_TT_LOG
// ttlog_dump is a special case of ttlog that does not use the mailbox.
// It is used to dump data to the special debug dump region, which is currently
// available only in TRISC0-2 as last 2kb of TRISC memory.
template <uint32_t Locator, typename... Args>
void tt_dump(Args... args) {
  constexpr uint32_t num_input = sizeof...(Args);
  uint32_t arr[] = { static_cast<uint32_t>(args)... };
  constexpr uint32_t coffee = 0xC0FFEE;
  constexpr uint32_t locator = Locator;
  debug_dump(reinterpret_cast<const uint8_t *>(&coffee), sizeof(uint32_t));
  debug_dump(reinterpret_cast<const uint8_t *>(&locator), sizeof(uint32_t));
  debug_dump(reinterpret_cast<const uint8_t *>(arr), num_input * sizeof(uint32_t));
}

// Similar as TT_RISC_ASSERT, but writes into debug buffer instead of mailbox and does not block
#define TT_DUMP_LOG(fmt, ...) tt_dump<FWLOG_HASH(fmt)>(__VA_ARGS__)
#define TT_DUMP_ASSERT(cond, fmt, ...) do { if (!(cond)) tt_dump<FWLOG_HASH(fmt)>(__VA_ARGS__); } while (0)

#else

#define TT_DUMP_LOG(fmt, ...) (void)sizeof(__VA_ARGS__)
#define TT_DUMP_ASSERT(cond, fmt, ...) do { (void)((bool)(cond)); (void)sizeof(__VA_ARGS__); } while (0)

#endif

#ifdef ENABLE_TT_LLK_DUMP
#define TT_LLK_DUMP(fmt, ...) ttlog<FWLOG_HASH(fmt), true>(__VA_ARGS__)
#else
#define TT_LLK_DUMP(...) (void)sizeof(__VA_ARGS__)
#endif

}