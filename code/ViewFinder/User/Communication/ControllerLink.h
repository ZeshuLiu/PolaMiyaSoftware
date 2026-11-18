#ifndef VIEWFINDER_CONTROLLER_LINK_H
#define VIEWFINDER_CONTROLLER_LINK_H

#include <stdint.h>

void ControllerLink_Init(void);

/* Call from the SDM50 RX handlers at the same priority as USART4/USART5. */
void ControllerLink_ForwardLegacyRange(const uint8_t frame[4]);

#endif /* VIEWFINDER_CONTROLLER_LINK_H */
