/**
 * File: term.h
 * Author: Diego Parrilla Santamaría
 * Date: January 20205
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Header for the terminal
 */

 #ifndef TERM_H
 #define TERM_H
 
 #include "debug.h"
 #include "constants.h"
 #include "memfunc.h"
 #include "reset.h"

 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "time.h"

 #include "hardware/dma.h"
 
 #include "tprotocol.h"
 #include "display_term.h"

#ifndef ROM3_GPIO
    #define ROM3_GPIO 26
#endif

#ifndef ROM4_GPIO
    #define ROM4_GPIO 22
#endif

// Use the highest 4K of the shared memory for the terminal commands
#define TERM_RANDOM_TOKEN_OFFSET 0xF000 // Random token offset in the shared memory
#define TERM_RANDON_TOKEN_SEED_OFFSET (TERM_RANDOM_TOKEN_OFFSET + 4) // Random token seed offset in the shared memory: 0xF004

// The shared variables are located in the + 0x200 offset
#define TERM_SHARED_VARIABLES_OFFSET (TERM_RANDOM_TOKEN_OFFSET + 0x200) // Shared variables offset in the shared memory: 0xF200

// Shared variables for common use. Must be set in the init function
#define TERM_HARDWARE_TYPE      (0)     // Hardware type. 0xF200
#define TERM_HARDWARE_VERSION   (1)     // Hardware version.  0xF204

// App commands for the terminal
#define APP_TERMINAL 0x00 // The terminal app

// App terminal commands
#define APP_TERMINAL_START     0x00 // Enter terminal command
#define APP_TERMINAL_KEYSTROKE 0x01 // Keystroke command

#ifdef DISPLAY_ATARIST
// Terminal size for Atari ST
#define TERM_SCREEN_SIZE_X 40
#define TERM_SCREEN_SIZE_Y 25
#define TERM_SCREEN_SIZE (TERM_SCREEN_SIZE_X * TERM_SCREEN_SIZE_Y)

#define TERM_DISPLAY_BYTES_PER_CHAR 8
#define TERM_DISPLAY_ROW_BYTES (TERM_DISPLAY_BYTES_PER_CHAR * TERM_SCREEN_SIZE_X)
#endif

// Holds up to one line of user input (between '\n' or '\r')
#define TERM_INPUT_BUFFER_SIZE 256

// Display command to enter the terminal mode and ignore other keys
#define DISPLAY_COMMAND_TERM 0x3    // Enter terminal mode

#define TPRINTF(fmt, ...) \
    do { \
        char buffer[256]; \
        snprintf(buffer, sizeof(buffer), fmt, ##__VA_ARGS__); \
        term_print_string(buffer); \
    } while (0)


// Command lookup structure, now with argument support
typedef struct {
    const char *command;
    void (*handler)(const char *arg);
} Command;
  
void __not_in_flash_func(term_dma_irq_handler_lookup)(void);

void term_init(void);

void __not_in_flash_func(term_loop)();


#endif // TERML_H
