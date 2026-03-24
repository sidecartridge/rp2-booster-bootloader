/**
 * File: gconfig.h
 * Author: Diego Parrilla Santamaría
 * Date: November 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the global configuration manager
 */

#ifndef GCONFIG_H
#define GCONFIG_H

#define PARAM_APPS_FOLDER "APPS_FOLDER"
#define PARAM_BOOT_FEATURE "BOOT_FEATURE"

#define GCONFIG_SUCCESS 0
#define GCONFIG_INIT_ERROR -1
#define GCONFIG_MISMATCHED_APP -2

int gconfig_init(const char *currentAppName);
const char *gconfig_get_apps_folder(void);

#endif  // GCONFIG_H
