/**
 * File: display_term.c
 * Author: Diego Parrilla Santamaría
 * Date: January 2025
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Terminal display functions.
 */

#include "display_term.h"

static uint8_t max_col = 0;
static uint8_t max_row = 0;

// Static assert to ensure buffer size fits within uint32_t
_Static_assert(DISPLAY_BUFFER_SIZE <= UINT32_MAX, "Buffer size exceeds allowed limits");

// Draw graphics into the buffer
void display_term_char(const uint8_t col, const uint8_t row, const char c) {
    u8g2_DrawGlyph(display_get_u8g2_ref(), col * 8, (DISPLAY_TERM_FIRST_ROW_OFFSET + row) * 8, c);
}

// Draw a solid block at the cursor position
void display_term_cursor(const uint8_t col, const uint8_t row) {
    u8g2_DrawBox(display_get_u8g2_ref(), col * 8, row * 8, 8, 8);
}

// The main function should be as follows:
void display_term_start(const uint8_t num_col, const uint8_t num_row)
{
    size_t buffer_size = DISPLAY_BUFFER_SIZE; // Safe usage
    (void)buffer_size; // To avoid unused variable warning if not used elsewhere

    // Initialize the u8g2 library for a custom buffer
    display_setup_u8g2();

    // // Clear the buffer first
    u8g2_ClearBuffer(display_get_u8g2_ref());

    // Set the flag to NOT-RESET the computer   
    SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_NOP);

    display_refresh();

    // Set the max column and row
    max_col = num_col;
    max_row = num_row;

    DPRINTF("Created the term display\n");
}

void display_term_refresh()
{
    // Refresh the display
    display_refresh();
}

void display_term_clear()
{
    // Clear the buffer
    u8g2_ClearBuffer(display_get_u8g2_ref());
    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
}
