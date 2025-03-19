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

// Macro for maximum allowed size
#define MAXIMUM_APP_UF2_SIZE 1048576  // Example: 1 MB
#define MAX_TAGS 6
#define MAX_DEVICES 6

#define MAXIMUM_APP_INFO_SIZE 4096

#define UF2_BLOCK_SIZE 512

// Each lookup table entry is 38 bytes:
//   - 36 bytes for the UUID
//   - 2 bytes for the sector (page number)
#define LOOKUP_ENTRY_SIZE 38

typedef struct {
  char protocol[16];
  char host[128];
  char uri[256];
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

download_delete_err_t appmngr_delete_app(const char *uuid);
download_launch_err_t appmngr_launch_app();
download_err_t appmngr_start_download_app();
download_poll_t appmngr_poll_download_app();
download_err_t appmngr_finish_download_app();
download_status_t appmngr_get_download_status();
download_err_t appmngr_get_download_error();
const char *appmngr_get_download_error_str();
download_launch_err_t appmngr_get_launch_status();
download_err_t appmngr_confirm_download_app();
download_err_t appmngr_confirm_failed_download_app();
void appmngr_download_status(download_status_t status);
void appmngr_download_error(download_err_t err);
void appmngr_set_launch_status(download_launch_err_t status);
download_catalog_err_t appmngr_create_app_catalog();
void appmngr_schedule_launch_app(const char *uuid);

void appmngr_print_apps_lookup_table(uint8_t *table, uint16_t length);
void appmngr_load_apps_lookup_table(uint8_t *table, uint16_t *length);
int8_t appmngr_erase_app_lookup_table();

bool appmngr_ffirst(char *json);
bool appmngr_fnext(char *json);

void appmngr_init();
void appmngr_deinit();

#endif  // APPMNGR_H
