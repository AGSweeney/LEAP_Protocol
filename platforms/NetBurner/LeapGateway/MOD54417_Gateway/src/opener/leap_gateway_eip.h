#ifndef LEAP_GATEWAY_EIP_H_
#define LEAP_GATEWAY_EIP_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void leap_gateway_eip_apply_output_assembly(const uint8_t* data, size_t length);
void leap_gateway_eip_pack_input_assembly(
  uint8_t* data,
  size_t   capacity,
  size_t*  length);
void leap_gateway_eip_force_assembly_sizes(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_GATEWAY_EIP_H_ */
