/**
 * File: fabric_httpd.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2023
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the fabric mode httpd server.
 */

#ifndef FABRIC_HTTPD_H
#define FABRIC_HTTPD_H

#include "debug.h"
#include "constants.h"
#include "network.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "lwip/apps/httpd.h"

typedef void (*fabric_httpd_callback_t)(const char *ssid, const char *pass, uint16_t auth, bool is_set);
typedef void (*fabric_httpd_served_callback_t)();

void fabric_httpd_start(fabric_httpd_callback_t callback, fabric_httpd_served_callback_t served_callback);

#endif // HTTPD_H
