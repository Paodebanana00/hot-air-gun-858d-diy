#include "hal.h"
#include "bootloader.h"
#include "rp_bootrom.h"

__attribute__((weak)) void mcu_reset(void) {
    NVIC_SystemReset();
}

void bootloader_jump(void) {
    rpRomResetUsbBoot(0U, 0U);
}

void enter_bootloader_mode_if_requested(void) {
}
