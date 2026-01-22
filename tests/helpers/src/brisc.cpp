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

    *state_command = 0;
    *state_brisc   = 0;

    while (true)
    {
        switch (static_cast<enum CommandState>(*state_command))
        {
            case CommandState::START_TRISCS:
                while (*mailbox_unpack != 0)
                {
                    *mailbox_unpack = 0;
                }
                while (*mailbox_math != 0)
                {
                    *mailbox_math = 0;
                }
                while (*mailbox_pack != 0)
                {
                    *mailbox_pack = 0;
                }

                device_setup();
                clear_trisc_soft_reset();

                *state_command = static_cast<uint32_t>(CommandState::IDLE_STATE);
                *state_brisc   = static_cast<uint32_t>(CommandState::START_TRISCS);
                break;
            case CommandState::RESET_TRISCS:
                set_triscs_soft_reset();

                while (*mailbox_unpack != 0)
                {
                    *mailbox_unpack = 0;
                }
                while (*mailbox_math != 0)
                {
                    *mailbox_math = 0;
                }
                while (*mailbox_pack != 0)
                {
                    *mailbox_pack = 0;
                }

                *state_command = static_cast<uint32_t>(CommandState::IDLE_STATE);
                *state_brisc   = static_cast<uint32_t>(CommandState::RESET_TRISCS);
                break;
            default:
                break;
        }
    }

#endif
}
