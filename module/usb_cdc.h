#ifndef HOT_AIR_GUN_MODULE_USB_CDC_H
#define HOT_AIR_GUN_MODULE_USB_CDC_H

#include "ch.h"
#include "hal.h"
#include "usbcfg.h"
void usb_cdc_handler_init(void);
static inline void usb_cdc_start(void)
{   
    usbDisconnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(1000);

    sduObjectInit(&SDU1);
    sduStart(&SDU1, &serusbcfg);

    usbStart(serusbcfg.usbp, &usbcfg);
    chThdSleepMilliseconds(100);

    usbConnectBus(serusbcfg.usbp);
    chThdSleepMilliseconds(500);
}

#endif /* HOT_AIR_GUN_MODULE_USB_CDC_H */