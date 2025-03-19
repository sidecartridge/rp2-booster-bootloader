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
#include "cjson/cJSON.h"
#include "constants.h"
#include "debug.h"
#include "display.h"
#include "display_mngr.h"
#include "display_usb.h"
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
#include "usb_mass.h"

void mngr_enable_network_scan();
void mngr_disable_network_scan();
wifi_scan_data_t* mngr_get_networks();

void mngr_schedule_reset(int seconds);
void mngr_schedule_factory_reset(int seconds);

int mngr_init(void);
void mngr_loop();

#endif  // MNGR_H
