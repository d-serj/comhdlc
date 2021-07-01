#ifndef MINIHDLC_H
#define MINIHDLC_H

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <stdint.h>
#include <stdbool.h>

typedef void (*sendchar_type)(uint8_t data);
typedef void (*frame_handler_type)(const uint8_t *frame_buffer,
              uint16_t frame_length);

#define MINIHDLC_MAX_FRAME_LENGTH 512

void minihdlc_init(sendchar_type sendchar_function,
                   frame_handler_type frame_handler_function);
void minihdlc_char_receiver(uint8_t data);
void minihdlc_send_frame(const uint8_t *frame_buffer, uint16_t frame_length);

void minihdlc_send_frame_to_buffer(const uint8_t *frame_buffer, uint16_t frame_length);
const uint8_t *minihdlc_get_buffer(void);
uint32_t minihdlc_get_buffer_size(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINIHDLC_H
