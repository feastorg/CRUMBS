#include <stddef.h>

#include "crumbs.h"

int main(void)
{
    crumbs_context_t ctx;
    crumbs_init(&ctx, CRUMBS_ROLE_CONTROLLER, 0u);

    if (CRUMBS_VERSION == 0 || crumbs_context_size() != sizeof(ctx))
    {
        return 1;
    }

    crumbs_message_t msg = {0};
    msg.type_id = 0x01u;
    msg.opcode = 0x02u;
    msg.data_len = 1u;
    msg.data[0] = 0x03u;

    uint8_t frame[CRUMBS_MESSAGE_MAX_SIZE];
    size_t frame_len = crumbs_encode_message(&msg, frame, sizeof(frame));
    if (frame_len == 0u)
    {
        return 1;
    }

    crumbs_message_t decoded = {0};
    if (crumbs_decode_message(frame, frame_len, &decoded, &ctx) != 0)
    {
        return 1;
    }

    return decoded.type_id == msg.type_id &&
                   decoded.opcode == msg.opcode &&
                   decoded.data_len == msg.data_len &&
                   decoded.data[0] == msg.data[0]
               ? 0
               : 1;
}
