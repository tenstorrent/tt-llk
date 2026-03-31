// SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "stream.h"


namespace llk {

// Add new message to enum below, and structure for data if needed.
enum class MessageType: std::uint8_t
{
    EXEC_REQUEST,
    EXEC_DONE,
    MEMCPY_REQUEST, // instruct RISC-V core to execute memcpy.
    MEMCPY_DONE, // response sent back to CHAN, once memcpy is completed.

    // add new messages ^^^^^
    COUNT // must be last member of the enum.
};

struct MemcpyRequestData
{
    void* destination;
    void* source;
    size_t length;
};


// Message queue impl. below, proceed with CAUTION

enum class MessageStreamId : std::size_t {
    // TRISC -> RUNTIME streams [0,1,2]
    TRISC_RUNTIME = 0,

    // RUNTIME -> TRISC streams [3,4,5]
    RUNTIME_TRISC = 3,

    // BRISC -> HOST stream [6]
    BRISC_HOST = 6,

    // HOST -> BRISC stream [7]
    HOST_BRISC = 7,

    // TRISC -> HOST streams [8,9,10]
    TRISC_HOST = 8,

    // HOST -> TRISC streams [11,12,13]
    HOST_TRISC = 11,

    COUNT = 14
};

using MessageStream = Stream<24>;

class MessageQueue {
private:
    static constexpr size_t QUEUE_COUNT = to_underlying(MessageStreamId::COUNT);

    MessageStream streams[QUEUE_COUNT];

    auto& get_stream(const MessageStreamId id) {
        #ifdef COMPILE_FOR_TRISC
            return streams[to_underlying(id) + COMPILE_FOR_TRISC];
        #else
            return streams[to_underlying(id)];
        #endif
    }

public:

    void send(const MessageStreamId id, const MessageType type) {
        auto& stream = get_stream(id);
        stream.push(&type, sizeof(type));
    }

    template <typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
    void send(const MessageStreamId id, const MessageType type, const T& data) {
        auto& stream = get_stream(id);
        stream.push(&type, sizeof(type));
        stream.push(&data, sizeof(data));
    }

    void receive(const MessageStreamId id, const MessageType expected_type) {
        auto& stream = get_stream(id);

        MessageType type_message;

        stream.pop(&type_message, sizeof(type_message));

        LLK_ASSERT(type_message == expected_type, "MessageQueue::receive: unexpected message type");
    }

    template <typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
    void receive(const MessageStreamId id, const MessageType expected_type, T& data) {
        auto& stream = get_stream(id);

        MessageType type_message;
        stream.pop(&type_message, sizeof(type_message));

        LLK_ASSERT(type_message == expected_type, "MessageQueue::receive: unexpected message type (with data payload)");

        stream.pop(&data, sizeof(data));
    }

    std::optional<MessageType> next(const MessageStreamId id) {
        auto& stream = get_stream(id);

        auto byte = stream.peek();
        if (!byte.has_value()) {
            return std::nullopt;
        }

        LLK_ASSERT(*byte < to_underlying(MessageType::COUNT), "MessageQueue::next: peeked byte is not a valid MessageType")

        return static_cast<MessageType>(*byte);
    }

};

}
