/**
 * File: aconfig.h
 * Author: Diego Parrilla Santamaría
 * Date: February 2025
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Header file for the app configuration manager
 */

#ifndef ACONFIG_H
#define ACONFIG_H

#include "debug.h"
#include "constants.h"

#include "settings.h"

#define ACONFIG_PARAM_APPS_FOLDER "APPS_FOLDER"
#define ACONFIG_PARAM_BOOT_FEATURE "BOOT_FEATURE"
#define ACONFIG_PARAM_HOSTNAME "HOSTNAME"
#define ACONFIG_PARAM_SAFE_CONFIG_REBOOT "SAFE_CONFIG_REBOOT"
#define ACONFIG_PARAM_SD_BAUD_RATE_KB "SD_BAUD_RATE_KB"

#define ACONFIG_SUCCESS 0
#define ACONFIG_INIT_ERROR -1
#define ACONFIG_MISMATCHED_APP -2
#define ACONFIG_APPKEYLOOKUP_ERROR -3

// Each lookup table entry is 38 bytes:
//   - 36 bytes for the UUID
//   - 2 bytes for the sector (page number)
#define ACONFIG_LOOKUP_ENTRY_SIZE 38

int aconfig_init(const char *current_app_id);
SettingsContext *aconfig_get_context(void);

#endif // ACONFIG_H
