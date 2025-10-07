/**
 * File: reset.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2025
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for RESET functions of the booster app
 */

#ifndef RESET_H
#define RESET_H

#include "constants.h"
#include "debug.h"
#include "gconfig.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "settings.h"

#define RESET_WATCHDOG_TIMEOUT 20  // 20 ms

/**
 * @brief Reset the current app and jump to the Booster app in flash
 *
 *
 * Do not use to reboot the device, because it does not jump to the start of the
 * Flash
 *
 * @note This function should not return. If it does, an error message is
 * printed.
 */
static inline void reset_jump_to_booster(void) {
  // This code jumps to the Booster application at the top of the flash memory.
  // The reason to perform this jump is for performance reasons.
  // It should be placed at the beginning of main() if the SELECT signal or
  // BOOSTER app is selected. Set VTOR register, set stack pointer, and jump to
  // reset.
  __asm__ __volatile__(
      "mov r0, %[start]\n"
      "ldr r1, =%[vtable]\n"
      "str r0, [r1]\n"
      "ldmia r0, {r0, r1}\n"
      "msr msp, r0\n"
      "bx r1\n"
      :
      : [start] "r"((unsigned int)&__flash_binary_start + 256),
        [vtable] "X"(PPB_BASE + M0PLUS_VTOR_OFFSET)
      :);
  DPRINTF("You should never reach this point\n");
}

/**
 * @brief Reset the app and reentry in the main device app in flash
 *
 *
 * Use to reboot the device, it jumps to the start of the Flash
 *
 * @note This function should not return. If it does, an error message is
 * printed.
 */
void reset_device();

/**
 * @brief Erases the flash memory.
 *
 * This function is responsible for erasing the flash memory. It should be used
 * with caution as the operation is irreversible and can lead to data loss.
 *
 * @note Ensure that any critical data is backed up before calling this
 * function.
 *
 * @warning Flash erasure may leave the device in an unstable state if the
 * process is interrupted. It is recommended to disable interrupts and take
 * necessary precautions prior to invoking this function.
 */
void reset_eraseFlash();

/**
 * @brief Reset the app and reentry in the main device app in flash.
 *       Erase the flash memory before rebooting
 *
 *
 * Use to reboot the device to a fabric configuration, it jumps to the start of
 * the Flash
 *
 * @note This function should not return. If it does, an error message is
 * printed.
 */
void reset_deviceAndEraseFlash();

#endif  // RESET_H
