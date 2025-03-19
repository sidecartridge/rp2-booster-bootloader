/**
 * File: display_term.h
 * Author: Diego Parrilla Santamaría
 * Date: January 2025
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Header file for the term mode diplay functions.
 */

#ifndef DISPLAY_TERM_H
#define DISPLAY_TERM_H

#include "debug.h"
#include "constants.h"
#include "network.h"
#include "memfunc.h"
#include "display.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "hardware/dma.h"

#include <u8g2.h>
#include "qrcodegen.h"

#ifdef DISPLAY_ATARIST
// Terminal size for Atari ST
// If the display of the chars is from bottom to top, then you need to add a ROW_OFFSET
// If it's top down, then set it to 0
#define DISPLAY_TERM_FIRST_ROW_OFFSET 1
#endif

void display_term_char(const uint8_t col, const uint8_t row, const char c);
void display_term_cursor(const uint8_t col, const uint8_t row);
void display_term_start(const uint8_t num_col, const uint8_t num_row);
void display_term_refresh();
void display_term_clear();
#endif // DISPLAY_TERM_H
