/**
 * File: version.h
 * Author: Diego Parrilla Santamaría
 * Date: August 2025
 * Copyright: 2025 - GOODDATA LABS SL
 * Description: Header file for the version downloader
 */

#ifndef VERSION_H
#define VERSION_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"
#include "debug.h"
#include "httpc/httpc.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/httpd.h"
#include "network.h"
#include "pico/async_context.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

// Status for the version download flow
typedef enum {
  DOWNLOAD_VERSION_IDLE,
  DOWNLOAD_VERSION_DEFAULT,
  DOWNLOAD_VERSION_START,
  DOWNLOAD_VERSION_IN_PROGRESS,
  DOWNLOAD_VERSION_COMPLETED,
  DOWNLOAD_VERSION_FAILED,
  DOWNLOAD_VERSION_CANNOTPARSEURL
} download_version_t;

// Polling interval and timeout for version download
#ifndef VERSION_POLL_INTERVAL_MS
#define VERSION_POLL_INTERVAL_MS 10
#endif
#ifndef VERSION_DOWNLOAD_TIMEOUT_MS
#define VERSION_DOWNLOAD_TIMEOUT_MS 10000
#endif

// Polling result for version download progress
typedef enum {
  DOWNLOAD_VERSION_POLL_CONTINUE,
  DOWNLOAD_VERSION_POLL_COMPLETED
} download_version_poll_t;

/**
 * @brief Drive the asynchronous download of the remote VERSION file.
 *
 * Starts the HTTP(S) request to fetch the version text from VERSION_URL and
 * polls the async context until the transfer completes, fails, or times out
 * (see VERSION_POLL_INTERVAL_MS and VERSION_DOWNLOAD_TIMEOUT_MS).
 *
 * On success, the internal version string buffer is updated with the downloaded
 * value. On error, the internal status reflects the failure reason.
 */
void version_loop();

// Returns the current download status
/**
 * @brief Get the current status of the version download workflow.
 *
 * @return download_version_t The current state (idle, in-progress,
 *         completed, failed, etc.).
 */
download_version_t version_get_status(void);

/**
 * @brief Get the current version string.
 *
 * Returns the downloaded version if present; otherwise falls back to
 * RELEASE_VERSION compiled into the firmware.
 *
 * @return const char* Pointer to a NUL-terminated version string.
 */
const char *version_get_string(void);

// Compare downloaded version (version_string) with built-in RELEASE_VERSION.
// Returns true if downloaded is newer.
/**
 * @brief Check if the downloaded version is newer than the built-in release.
 *
 * Compares the first three numeric components of the downloaded version string
 * against RELEASE_VERSION. Any non-numeric suffix (e.g., "alpha", "beta")
 * after the third numeric component is ignored for the comparison.
 *
 * Example: "2.0.5alpha" is considered older than "2.0.6alpha".
 *
 * @retval true  If the downloaded version is strictly newer.
 * @retval false If equal or not newer, or if no downloaded version is present.
 */
bool version_isNewer(void);

#endif  // VERSION_H
