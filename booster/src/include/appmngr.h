/**
 * File: appmngr.h
 * Author: Diego Parrilla Santamaría
 * Date: January 2025
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Header file for the application manager mode.
 */

#ifndef APPMNGR_H
#define APPMNGR_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cjson/cJSON.h"
#include "constants.h"
#include "debug.h"
#include "gconfig.h"
#include "httpc/httpc.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/httpd.h"
#include "md5/md5.h"
#include "memfunc.h"
#include "network.h"
#include "pico/async_context.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "reset.h"
#include "sdcard.h"

// Sanity caps on Content-Length, checked before we start writing a download to
// the SD card.
//
// A UF2 carries 256 payload bytes in each 512-byte block, so a .uf2 is about
// twice the size of the binary it holds, plus a little framing (the observed
// upgrade.bin is 512 bytes over exactly 2x).
//
// NOTE: these caps were dead code until v2.3.0 -- httpc.c unconditionally
// overwrote the headers callback that enforces them, so no download was ever
// size-checked and both values were wrong without anyone noticing.
#define UF2_OVERHEAD_FACTOR 2
#define UF2_FRAMING_SLACK (64 * 1024)

// A downloaded microfirmware .uf2 can at most fill the 1152K microfirmware
// slot (STORAGE_FLASH in memmap_booster.ld).
#define MAXIMUM_APP_UF2_SIZE \
  ((1152 * 1024 * UF2_OVERHEAD_FACTOR) + UF2_FRAMING_SLACK)

// A downloaded Booster upgrade.bin is the FULL-image .uf2 (placeholder plus
// Booster core, 3,932,672 bytes as of v2.2.0) -- far larger than any single
// microfirmware, so it needs its own cap. Bound it by the whole flash
// expressed as UF2 so the value survives changes to the image layout.
#define MAXIMUM_FIRMWARE_UF2_SIZE \
  ((PICO_FLASH_SIZE_BYTES * UF2_OVERHEAD_FACTOR) + UF2_FRAMING_SLACK)
// Redirects are followed on both download paths, microfirmware and firmware
// OTA. Five matches md-browser and is well past what real hosting chains use:
// the GitHub release path this was written for takes two.
#define APPMNGR_MAX_REDIRECT_HOPS 5

// How long to let lwIP finish tearing down an aborted redirect response
// before opening the next connection. Without this the two overlap and the
// new handshake stalls until the poll timeout.
// Minimum spacing between closing one hop's connection and opening the next.
// Empirical and load-bearing: reconnecting within a few milliseconds yields a
// connection that receives nothing and times out. Debug builds get this
// spacing for free from UART logging.
#define APPMNGR_REDIRECT_SETTLE_MS 500

// Holds a fully-resolved redirect target. Sized for the same worst case as
// url_components.uri plus scheme and host.
#define APPMNGR_MAX_URL_SIZE 1700

#define MAX_TAGS 6
#define MAX_DEVICES 6

#define MAXIMUM_APP_INFO_SIZE 4096

#define UF2_BLOCK_SIZE 512

// Chunk size for the UF2 -> flash copy at app launch (storeUF2FileToFlash).
// Only a batching buffer in front of flash_range_program, which programs
// 256-byte pages: any multiple of FLASH_PAGE_SIZE writes byte-identical flash
// contents. One flash sector is a natural granularity (it matches the erase
// unit) and keeps the buffer small enough to live in bss.
//
// The buffer is STATIC, not malloc'd. History of why:
//   - It used to be the SDK's FLASH_BLOCK_SIZE (64K) from the heap. Once
//   mbedTLS
//     grew bss by ~9KB the allocation ran 536 bytes past 0x20030000, into the
//     ROM_IN_RAM cartridge window, silently corrupting the ROM served to the
//     Atari. memmap_booster.ld now caps the heap at the stack bottom so the
//     heap can never reach that window at all.
//   - With the cap the heap is ~47KB, so 64K could not be allocated anyway, and
//     even 32K failed at launch: by then the session has run the web server,
//     parsed catalog JSON, and done a TLS handshake, so a single CONTIGUOUS 32K
//     block is not obtainable even though the total free heap is larger. malloc
//     panics rather than returning NULL (PICO_MALLOC_PANIC), so that surfaced
//     as
//     "*** PANIC *** Out of memory".
// A static buffer removes the whole failure class: no allocation, no
// fragmentation sensitivity, and it leaves the heap free for TLS.
#define APP_FLASH_COPY_CHUNK_SIZE FLASH_SECTOR_SIZE

// Each lookup table entry is 38 bytes:
//   - 36 bytes for the UUID
//   - 2 bytes for the sector (page number)
#define LOOKUP_ENTRY_SIZE 38
#define APPMNGR_MAX_INSTALLED_APPS (FLASH_SECTOR_SIZE / LOOKUP_ENTRY_SIZE)
#define APPMNGR_INSTALLED_APP_NAME_LENGTH 64
#define APPMNGR_INSTALLED_APP_VERSION_LENGTH 16

typedef struct {
  char protocol[16];
  char host[128];
  // A redirect target can be far longer than the URL originally requested.
  // GitHub release assets end at a signed storage URL whose query string alone
  // runs past 900 characters (SAS token plus JWT), so 256 truncated it and the
  // follow-up request went to a mangled path.
  char uri[1536];
  // Explicit port from a "host:port" URL, or 0 to let the scheme decide
  // (80 for http, 443 for https).
  uint16_t port;
} url_components_t;

typedef struct {
  bool ready;
  uint32_t total_size;
  uint32_t free_space;
  bool apps_folder_found;
} sdcard_info_t;

typedef struct {
  char uuid[64];
  char name[128];
  char description[512];
  char image[256];

  int tags_count;
  char tags[MAX_TAGS][32];

  int devices_count;
  char devices[MAX_DEVICES][32];

  char binary[256];
  uint8_t md5[16];
  char version[16];
  char json[2048];
  uint8_t file_md5_digest[16];  // Result of algorithm
} app_info_t;

typedef struct {
  char uuid[37];
  char name[APPMNGR_INSTALLED_APP_NAME_LENGTH];
  char version[APPMNGR_INSTALLED_APP_VERSION_LENGTH];
} appmngr_installed_app_t;

typedef enum {
  DOWNLOAD_STATUS_IDLE,
  DOWNLOAD_STATUS_REQUESTED,
  DOWNLOAD_STATUS_NOT_STARTED,
  DOWNLOAD_STATUS_STARTED,
  DOWNLOAD_STATUS_IN_PROGRESS,
  DOWNLOAD_STATUS_COMPLETED,
  DOWNLOAD_STATUS_FAILED
} download_status_t;

typedef enum {
  DOWNLOAD_POLL_CONTINUE,
  DOWNLOAD_POLL_ERROR,
  DOWNLOAD_POLL_COMPLETED
} download_poll_t;

typedef enum {
  DOWNLOAD_OK,
  DOWNLOAD_BASE64_ERROR,
  DOWNLOAD_PARSEJSON_ERROR,
  DOWNLOAD_PARSEMD5_ERROR,
  DOWNLOAD_CANNOTOPENFILE_ERROR,
  DOWNLOAD_CANNOTWRITEFILE_ERROR,
  DOWNLOAD_CANNOTCLOSEFILE_ERROR,
  DOWNLOAD_FORCEDABORT_ERROR,
  DOWNLOAD_CANNOTSTARTDOWNLOAD_ERROR,
  DOWNLOAD_CANNOTREADFILE_ERROR,
  DOWNLOAD_CANNOTPARSEURL_ERROR,
  DOWNLOAD_MD5MISMATCH_ERROR,
  DOWNLOAD_CANNOTRENAMEFILE_ERROR,
  DOWNLOAD_CANNOTCREATE_CONFIG,
  DOWNLOAD_CANNOTDELETECONFIGSECTOR_ERROR,
  DOWNLOAD_HTTP_ERROR,
} download_err_t;

typedef enum {
  DOWNLOAD_CREATECATALOG_OK,
  DOWNLOAD_CREATECATALOG_PROCESSING,
  DOWNLOAD_CREATECATALOG_ERROR,
  DOWNLOAD_CREATECATALOG_NODIR_ERROR,
  DOWNLOAD_CREATECATALOG_JSON_ERROR,
  DOWNLOAD_CREATECATALOG_CANNOTOPENFILE_ERROR,
  DOWNLOAD_CREATECATALOG_CANNOTWRITEFILE_ERROR
} download_catalog_err_t;

typedef enum {
  DOWNLOAD_DELETEAPP_OK,
  DOWNLOAD_DELETEAPP_SDCARDNOTREADY_ERROR,
  DOWNLOAD_DELETEAPP_NOTUUID_ERROR
} download_delete_err_t;

typedef enum {
  DOWNLOAD_LAUNCHAPP_IDLE,
  DOWNLOAD_LAUNCHAPP_SCHEDULED,
  DOWNLOAD_LAUNCHAPP_INPROGRESS,
  DOWNLOAD_LAUNCHAPP_OK,
  DOWNLOAD_LAUNCHAPP_SDCARDNOTREADY_ERROR,
  DOWNLOAD_LAUNCHAPP_NOTUUID_ERROR
} download_launch_err_t;

sdcard_info_t *appmngr_get_sdcard_info();
app_info_t *appmngr_get_app_info();

download_err_t appmngr_save_app_info(const char *json_str);

int appmngr_delete_app_info();

void appmngr_set_download_update(bool update);

download_delete_err_t appmngr_delete_app(const char *uuid);
download_launch_err_t appmngr_launch_app();
download_err_t appmngr_start_download(const char *url);
download_poll_t appmngr_poll_download_app();
download_err_t appmngr_finish_download_app();
download_status_t appmngr_get_download_status();
download_err_t appmngr_get_download_error();

download_status_t appmngr_get_download_firmware_status();
void appmngr_download_firmware_status(download_status_t status);
download_err_t appmngr_get_download_firmware_error();
void appmngr_download_firmware_error(download_err_t err);

const char *appmngr_get_download_error_str();
download_launch_err_t appmngr_get_launch_status();
download_err_t appmngr_confirm_download_app();
download_err_t appmngr_confirm_failed_download_app();
void appmngr_download_status(download_status_t status);
void appmngr_download_error(download_err_t err);

download_err_t __not_in_flash_func(appmngr_confirm_download_firmware)();
download_err_t appmngr_finish_download_firmware();
download_err_t appmngr_confirm_failed_download_firmware();
void appmngr_firmwareUpgradeStart(void);
void mngr_firmwareUpgradeClean(void);
void mngr_firmwareUpgradeInstall(void);

void appmngr_set_launch_status(download_launch_err_t status);
download_catalog_err_t appmngr_create_app_catalog();
void appmngr_schedule_launch_app(const char *uuid);

void appmngr_print_apps_lookup_table(uint8_t *table, uint16_t length);
void appmngr_load_apps_lookup_table(uint8_t *table, uint16_t *length);
int8_t appmngr_erase_app_lookup_table();
uint16_t appmngr_get_installed_apps(appmngr_installed_app_t *apps,
                                    uint16_t max_apps);

bool appmngr_ffirst(char *json);
bool appmngr_fnext(char *json);

void appmngr_init();
void appmngr_deinit();

// Synchronize the apps lookup table with JSON files in the apps folder.
// Scans all .json files (excluding apps.json), parses them, and ensures each
// app UUID exists in the lookup table; missing entries are added.
void appmngr_sync_lookup_table();

#endif  // APPMNGR_H
