#include "include/aconfig.h"

// We don't have any variables because this is the placeholder app
static SettingsConfigEntry defaultEntries[] = {};

enum {
    ACONFIG_BUFFER_SIZE = 4096,
    ACONFIG_MAGIC_NUMBER = 0x1234,
    ACONFIG_VERSION_NUMBER = 0x0001
};

// Create a global context for our settings
static SettingsContext g_settingsCtx;

// Helper function to check if a 36-character string is a valid UUID4.
// A valid UUID4 is in the canonical format "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
// where x is a hexadecimal digit and y is one of [8, 9, A, B] (case-insensitive).
static int is_valid_uuid4(const char *uuid) {
    // We assume "uuid" is exactly 36 characters (no null terminator at index 36)
    // Positions of hyphens and specific checks:
    //   Hyphens at indices 8, 13, 18, 23.
    //   The character at index 14 must be '4'.
    //   The character at index 19 must be one of '8', '9', 'A', 'B' (case-insensitive).

    // Quick initial checks for mandatory hyphens.
    if (uuid[8]  != '-' || uuid[13] != '-' ||
        uuid[18] != '-' || uuid[23] != '-') {
        return 0;
    }

    // Check the version char is '4'.
    if (uuid[14] != '4') {
        return 0;
    }

    // Check the variant char is one of '8', '9', 'A', or 'B' (case-insensitive).
    char variant = uuid[19];
    switch (variant) {
        case '8': case '9': case 'A': case 'B':
        case 'a': case 'b':
            break;
        default:
            return 0;
    }

    // Check the rest of the characters for hex digits
    // (excluding the four known hyphen positions)
    static const int hyphen_positions[4] = {8, 13, 18, 23};
    int j = 0; // index into hyphen_positions

    for (int i = 0; i < 36; i++) {
        if (j < 4 && i == hyphen_positions[j]) {
            // we already checked these are '-', so skip
            j++;
            continue;
        }
        // Check for hex digit
        if (!isxdigit((unsigned char)uuid[i])) {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Initializes the application configuration settings.
 *
 * This function initializes the application configuration settings using the provided default entries.
 * If the settings are not initialized, it initializes them with the default values.
 * Searchs the flash address of the configuration using the current_app_id as key in the config app lookup table
 *
 * @param current_app_id The UUID4 of the current application. Not NULL.
 * @return int Returns ACONFIG_SUCCESS on success, ACONFIG_INIT_ERROR if there is an error initializing the settings,
 *             or ACONFIG_MISMATCHED_APP if the current application does not match the one in the settings.
 */
int aconfig_init(const char *current_app_id) {

    DPRINTF("Finding the configuration flash address for the current app\n");

    // Calculate the lookup table boundaries
    uint8_t *lookup_start = (uint8_t *)&_global_lookup_flash_start;
    uint8_t *lookup_end   = (uint8_t *)&_global_config_flash_start; // one past the last valid lookup byte
    size_t   lookup_len   = (size_t)(lookup_end - lookup_start);

    // Loop through each lookup entry
    //    ACONFIG_LOOKUP_ENTRY_SIZE bytes per entry: 
    //      first 36 bytes => UUID 
    //      next 2 bytes  => sector index
    //    Stop on zero-filled UUID or invalid data or out-of-bounds.
    uint8_t *p = lookup_start; 
    uint32_t flash_address = 0; // Will remain 0 if we don't find a match

    while ((size_t)(p - lookup_start) + ACONFIG_LOOKUP_ENTRY_SIZE <= lookup_len) {
        // If the first byte is 0, we consider there are no more valid entries
        if (p[0] == 0) {
            break;
        }

        // Check if the 36 bytes form a valid UUID4
        // If invalid, assume no further valid data
        if (!is_valid_uuid4((const char *)p)) {
            break;
        }

        // Compare with the given current_app_id
        if (memcmp(p, current_app_id, 36) == 0) {
            // We found our app ID
            // Next two bytes (little-endian) represent the sector number
            uint16_t sector = (uint16_t)(p[36]) | ((uint16_t)(p[37]) << 8);

            // Convert sector number to actual flash address
            flash_address = (uint32_t)&_config_flash_start + (sector * FLASH_SECTOR_SIZE);
            DPRINTF("Configuration flash address found sector:%u addr: 0x%X\n", sector, flash_address);
            break;
        }

        // Move pointer to the next entry
        p += ACONFIG_LOOKUP_ENTRY_SIZE;
    }

    if (flash_address == 0) {
        DPRINTF("Configuration flash address not found for the current app\n");
        return ACONFIG_APPKEYLOOKUP_ERROR;
    }

    DPRINTF("Initializing app settings\n");
    int err = settings_init(&g_settingsCtx,
                            defaultEntries, 
                            sizeof(defaultEntries) / sizeof(defaultEntries[0]), 
                            flash_address - XIP_BASE,
                            ACONFIG_BUFFER_SIZE, 
                            ACONFIG_MAGIC_NUMBER, 
                            ACONFIG_VERSION_NUMBER);

    // If the settings are not initialized, then we must initialize them with the default values
    // in the Booster application
    if (err<0) {
        DPRINTF("Error initializing app settings.\n");
        return ACONFIG_INIT_ERROR;
    }

    DPRINTF("Settings app loaded.\n");

    settings_print(&g_settingsCtx, NULL);

    return ACONFIG_SUCCESS;
}

/**
 * @brief Returns a pointer to the global settings context of the application.
 *
 * This function allows other parts of the application to retrieve a pointer
 * to the global settings context at any time.
 *
 * @return SettingsContext* Pointer to the global settings context of the application
 */
SettingsContext *aconfig_get_context(void) {
    return &g_settingsCtx;
}