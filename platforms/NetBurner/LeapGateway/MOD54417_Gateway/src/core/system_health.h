#ifndef LEAP_GATEWAY_SYSTEM_HEALTH_H_
#define LEAP_GATEWAY_SYSTEM_HEALTH_H_

#include <nbrtos.h>
#include <stdint.h>

struct GatewaySystemHealthSnapshot
{
    bool ready;
    uint32_t sample_ms;
    uint32_t cpu_load_tenths;
    uint32_t idle_tenths;
    uint32_t leap_tenths;
    uint32_t eip_tenths;
    uint32_t network_tenths;
    uint32_t other_tenths;
    uint32_t max_task_tenths;
    uint32_t max_task_ticks;
    char max_task_name[32];
};

void GatewaySystemHealthRegisterLeapTask(OS_TCB *task);
void GatewaySystemHealthRegisterOpenerTask(OS_TCB *task);
void GatewaySystemHealthSample(void);
GatewaySystemHealthSnapshot GatewaySystemHealthGetSnapshot(void);

#endif /* LEAP_GATEWAY_SYSTEM_HEALTH_H_ */
