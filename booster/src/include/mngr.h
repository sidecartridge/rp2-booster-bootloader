/**
 * File: mngr.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the generic manager mode.
 */

#ifndef MNGR_H
#define MNGR_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "appmngr.h"
#include "blink.h"
#include "cjson/cJSON.h"
#include "constants.h"
#include "debug.h"
#include "display.h"
#include "display_mngr.h"
#include "gconfig.h"
#include "httpc/httpc.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/httpd.h"
#include "mngr_httpd.h"
#include "network.h"
#include "pico/async_context.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "reset.h"
#include "sdcard.h"
#include "select.h"
#include "term.h"
#include "version.h"

typedef enum {
  FIRMWARE_UPGRADE_IDLE,
  FIRMWARE_UPGRADE_DOWNLOADING,
  FIRMWARE_UPGRADE_DOWNLOADED,
  FIRMWARE_UPGRADE_VERIFYING,
  FIRMWARE_UPGRADE_VERIFIED,
  FIRMWARE_UPGRADE_INSTALLING,
  FIRMWARE_UPGRADE_FAILED,
  FIRMWARE_UPGRADE_SUCCESS
} firmware_upgrade_state_t;

void mngr_enable_network_scan();
void mngr_disable_network_scan();
wifi_scan_data_t* mngr_get_networks();

void mngr_schedule_reset(int seconds);
void mngr_schedule_factory_reset(int seconds);

int mngr_init(void);
void mngr_loop();

// Firmware upgrade control
void mngr_firmwareUpgradeStart(void);
void mngr_firmwareUpgradeClean(void);
firmware_upgrade_state_t mngr_get_firmwareUpgradeState(void);

#endif  // MNGR_H
