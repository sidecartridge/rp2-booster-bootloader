/**
 * File: main.c
 * Author: Diego Parrilla Santamaría
 * Date: Novemeber 2023
 * Copyright: 2023 - GOODDATA LABS SL
 * Description: C file that launches the ROM emulator
 */

#include "include/debug.h"
#include "include/constants.h"

#include "include/gconfig.h"
#include "include/aconfig.h"

enum {
    SETTINGS_ADDRESS_OFFSET = 0x1FF000,
    BUFFER_SIZE = 4096,
    MAGIC_NUMBER = 0x1234,
    VERSION_NUMBER = 0x0001
};

static inline void jump_to_booster_app() {
    // This code jumps to the Booster application at the top of the flash memory
    // The reason to perform this jump is for performance reasons
    // This code should be placed at the beginning of the main function, and should be 
    // executed if the SELECT signal or the BOOSTER app is selected
    // Set VTOR register, set stack pointer, and jump to reset
    __asm__ __volatile__(
    "mov r0, %[start]\n"
    "ldr r1, =%[vtable]\n"
    "str r0, [r1]\n"
    "ldmia r0, {r0, r1}\n"
    "msr msp, r0\n"
    "bx r1\n"
    :
    : [start] "r"((unsigned int)&_booster_app_flash_start + 256), [vtable] "X"(PPB_BASE + M0PLUS_VTOR_OFFSET)
    :);
    DPRINTF("You should never reach this point\n");
}

int main()
{
#if defined(_DEBUG) && (_DEBUG != 0)
    // Initialize chosen serial port
    stdio_init_all();
    setvbuf(stdout, NULL, _IONBF,
            1);  // specify that the stream should be unbuffered

    // Only startup information to display
    DPRINTF("\n\nPlaceholder app to jumpstart to Booster app. %s (%s). %s mode.\n\n", RELEASE_VERSION, RELEASE_DATE, _DEBUG ? "DEBUG" : "RELEASE");

    // Show information about the frequency and voltage
    int current_clock_frequency_khz = RP2040_CLOCK_FREQ_KHZ;
    const char *current_voltage = VOLTAGE_VALUES[RP2040_VOLTAGE];
    DPRINTF("Clock frequency: %i KHz\n", current_clock_frequency_khz);
    DPRINTF("Voltage: %s\n", current_voltage);
    DPRINTF("PICO_FLASH_SIZE_BYTES: %i\n", PICO_FLASH_SIZE_BYTES);

    unsigned int flash_length = (unsigned int)&_booster_app_flash_start - (unsigned int)&__flash_binary_start;
    unsigned int booster_flash_length = (unsigned int)&_config_flash_start - (unsigned int)&_booster_app_flash_start;
    unsigned int config_flash_length = (unsigned int)&_global_lookup_flash_start - (unsigned int)&_config_flash_start;
    unsigned int global_lookup_flash_length = FLASH_SECTOR_SIZE;
    unsigned int global_config_flash_length = FLASH_SECTOR_SIZE;
    unsigned int rom_in_ram_length = 128 * 1024;

    DPRINTF("Flash start: 0x%X, length: %u bytes\n", (unsigned int)&__flash_binary_start, flash_length);
    DPRINTF("Booster Flash start: 0x%X, length: %u bytes\n", (unsigned int)&_booster_app_flash_start, booster_flash_length);
    DPRINTF("Config Flash start: 0x%X, length: %u bytes\n", (unsigned int)&_config_flash_start, config_flash_length);
    DPRINTF("Global Lookup Flash start: 0x%X, length: %u bytes\n", (unsigned int)&_global_lookup_flash_start, global_lookup_flash_length);
    DPRINTF("Global Config Flash start: 0x%X, length: %u bytes\n", (unsigned int)&_global_config_flash_start, global_config_flash_length);
    DPRINTF("ROM in RAM start: 0x%X, length: %u bytes\n", (unsigned int)&__rom_in_ram_start__, rom_in_ram_length);

#endif
    
    // We don't want to initialized any settings in the global config
    // We only want to read the global config to get the magic number and version number
    // So we don't pass any entries to the settings_init function
    int err = gconfig_init(CURRENT_APP_UUID_KEY);

    if (err < 0) {
        DPRINTF("Settings not initialized. Jump to Booster application\n");
        jump_to_booster_app();
    }

    // If we are here, it means the app uuid key is correct. So we can read or initialize the app settings
    err = aconfig_init(CURRENT_APP_UUID_KEY);
    if (err < 0) {
        // No settings found. Initialize them
        DPRINTF("App settings not initialized. Initialize them first\n");
        err = settings_save(aconfig_get_context(), true);
        if (err < 0) {
            // Something went wrong saving the settings. Go to booster.
            DPRINTF("Error saving settings. Go to BOOSTER.\n");
            jump_to_booster_app();
        }
        // Show the settings
        settings_print(aconfig_get_context(), NULL);
    }

    DPRINTF("Start the app loop here\n");
    while (1) {
        // Do something
    }
}
