#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapP4IoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    uint16_t io_status;
    int      safe_active;
} LeapP4IoShadow;

void leap_hw_init(void);
void leap_hw_apply_outputs(LeapP4IoShadow *io, uint16_t outputs);
void leap_hw_enter_safe(LeapP4IoShadow *io);
void leap_hw_refresh_inputs(LeapP4IoShadow *io);
void leap_hw_set_locate_led(uint8_t on);

#ifdef __cplusplus
}
#endif
