// SPDX-FileCopyrightText: © 2024 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>

#ifdef LLK_BOOT_MODE_BRISC
#include "boot.h"
#endif

enum class CommandState : uint32_t
{
    IDLE_STATE   = 0,
    START_TRISCS = 1,
    RESET_TRISCS = 2,
};

int main()
{
#ifdef LLK_BOOT_MODE_BRISC

    volatile uint32_t* const state_command = reinterpret_cast<volatile uint32_t*>(0x1FFEC);
    volatile uint32_t* const state_brisc   = reinterpret_cast<volatile uint32_t*>(0x1FFF0);

    volatile std::uint32_t* const mailbox_unpack = reinterpret_cast<volatile std::uint32_t*>(0x1FFFC);
    volatile std::uint32_t* const mailbox_math   = reinterpret_cast<volatile std::uint32_t*>(0x1FFF8);
    volatile std::uint32_t* const mailbox_pack   = reinterpret_cast<volatile std::uint32_t*>(0x1FFF4);

    asm volatile("fence");
    *state_command = 0;
    asm volatile("fence");
    *state_brisc = 0;
    asm volatile("fence");

    while (true)
    {
        switch (static_cast<enum CommandState>(*state_command))
        {
            case CommandState::START_TRISCS:
                while (*mailbox_unpack != 0)
                {
                    asm volatile("fence");
                    *mailbox_unpack = 0;
                    asm volatile("fence");
                }
                while (*mailbox_math != 0)
                {
                    asm volatile("fence");
                    *mailbox_math = 0;
                    asm volatile("fence");
                }
                while (*mailbox_pack != 0)
                {
                    asm volatile("fence");
                    *mailbox_pack = 0;
                    asm volatile("fence");
                }

                device_setup();
                clear_trisc_soft_reset();

                *state_command = static_cast<uint32_t>(CommandState::IDLE_STATE);
                asm volatile("fence");
                *state_brisc = static_cast<uint32_t>(CommandState::START_TRISCS);
                asm volatile("fence");
                break;
            case CommandState::RESET_TRISCS:
                set_triscs_soft_reset();

                *state_command = static_cast<uint32_t>(CommandState::IDLE_STATE);
                asm volatile("fence");
                *state_brisc = static_cast<uint32_t>(CommandState::RESET_TRISCS);
                asm volatile("fence");
                break;
            default:
                break;
        }
    }

#endif
}
