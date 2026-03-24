/**
 * File: display_mngr.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the mnaager mode diplay functions.
 */

#ifndef DISPLAY_MNGR_H
#define DISPLAY_MNGR_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <u8g2.h>

#include "constants.h"
#include "debug.h"
#include "display.h"
#include "hardware/dma.h"
#include "memfunc.h"
#include "network.h"
#include "qrcodegen.h"

#define DISPLAY_MNGR_QR_SCALE 5

#define DISPLAY_MNGR_SELECT_RESET_MESSAGE \
  "Hold SELECT over 10s to restore WiFi factory defaults."

#define DISPLAY_MNGR_WIFI_STATUS_CONNECTING 0
#define DISPLAY_MNGR_WIFI_STATUS_CONNECTED 1
#define DISPLAY_MNGR_WIFI_STATUS_RETRY_ERROR 2
#define DISPLAY_MNGR_WIFI_STATUS_OFFLINE 3

#define DISPLAY_MNGR_EMPTY_MESSAGE                                             \
  "                                                                          " \
  "      "

#define DISPLAY_STRETCHED_TILES_WIDTH \
  68  // Tile width when using squeezed/stretched fonts

// For Atari ST display
#ifdef DISPLAY_ATARIST
#define DISPLAY_MANAGER_BYPASS_MESSAGE \
  "Press any SHIFT key to boot from GEMDOS. ESC for apps."
#endif

void display_mngr_start(const char *ssid, const char *url1, const char *url2);
void display_mngr_wifi_change_status(uint8_t wifi_status, const char *url1,
                                     const char *url2,
                                     const char *status_details,
                                     const char *mac_str);
void display_mngr_change_status(uint8_t status, const char *details);
void display_mngr_refresh_connection_info();
void display_mngr_redraw_current(void);

#endif  // DISPLAY_MNGR_H
