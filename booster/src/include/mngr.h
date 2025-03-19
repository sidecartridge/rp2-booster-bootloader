/**
 * File: mngr.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the generic manager mode.
 */

#ifndef MNGR_H
#define MNGR_H

#include "debug.h"
#include "constants.h"
#include "gconfig.h"
#include "network.h"
#include "reset.h"
#include "sdcard.h"
#include "usb_mass.h"
#include "display.h"
#include "display_mngr.h"
#include "display_usb.h"
#include "mngr_httpd.h"
#include "appmngr.h"
#include "term.h"
#include "select.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "lwip/apps/httpd.h"
#include "pico/async_context.h"
#include "lwip/altcp_tls.h"

#include "httpc/httpc.h"
#include "cjson/cjson.h"

void mngr_enable_network_scan();
void mngr_disable_network_scan();
wifi_scan_data_t* mngr_get_networks();

void mngr_schedule_reset(int seconds);
void mngr_schedule_factory_reset(int seconds);

int mngr_init(void);
void mngr_loop();


#endif // MNGR_H
