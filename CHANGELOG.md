# Changelog

## v2.2.0 (2026-04-20) - release

This release lets users pick which version of a microfirmware to install, including older ones when the publisher keeps them online.

### Changes
- Replace the Download/Update buttons on the Apps page with a single context-aware action button (Install / Update / Downgrade).

### New features
- Add an optional `previous_versions` array to entries in `apps.json`, backwards compatible with existing catalogs.
- Add a per-app version selector defaulting to the latest, with downgrades requiring an explicit confirmation.

### Fixes
- Compare versions using a semantic-version comparator instead of string equality.

---

## v2.1.0 (2026-03-24) - release

This release delivers a smoother overall user experience, with a cleaner Manager interface on both the Atari ST screen and the web UI, better behavior when WiFi is unavailable, fixes for some network issues in weak WiFi environments, and major flash-usage optimizations behind the scenes.

### Changes
- Reduced flash usage across Booster and the embedded Upgrader with targeted code-size optimizations, smaller display primitives, and streamlined helper code.
- Minify the Manager web assets before generating `fsdata_srv.c`, and refactor repeated page content to reduce the embedded web payload size.
- Add automatic flash usage reporting to the Booster build and stop the build if the generated image exceeds the available flash slot.
- Accept `MinSizeRel` explicitly in the build scripts, and always build the Upgrader in `MinSizeRel` for `release` and `minsizerel` flows.

### New features
- Add an offline Manager mode: if WiFi connection retries are exhausted, Booster keeps running without network features and still allows manual boot of already-downloaded microfirmwares from the Atari ST terminal path.
- Allow launching installed apps directly from the Atari ST terminal, including the ESC-to-apps and SHIFT-to-GEMDOS workflow for manual booting.
- Show signal strength and MAC address information in both the Atari ST Manager screen and the shared system information panel in the web UI.
- Re-enable ICMP ping replies for easier network diagnostics and compatibility checks.
- Add responsive Platform and Features filters to the Apps page so users can narrow the available microfirmwares dynamically from the downloaded catalog.
- Update the integrated Atari ST terminal firmware and embedded Upgrader image used by Booster.

### Fixes
- Improve SELECT-button debouncing for long presses so factory reset is more reliable.
- Fix several WiFi stability issues, including reconnect memory pressure, polling behavior, multi-connection handling, and weaker-signal network environments.
- Harden MD5 parsing for downloaded app metadata by validating the input length before converting it from JSON.
- Polish Manager copy, status messaging, and minor header consistency issues found during PR review.

---

## v2.0.9 (2025-12-20) - release

This release now shows the MAC address to help users to configure their home router filters.

### Changes
- Show the WiFi countries as a drop down list instead of a text input, to avoid invalid country codes.
- Show the available power WiFi network options as a drop down list instead of a text input, to avoid invalid power values.
- Show the range of SD baud rate values (KB/s) as a drop down list instead of a text input, to avoid invalid speed values.
- Shows the MAC address always on the Atari ST connection screen, even if the device is not connected to a WiFi network.

### New features
- Shows the MAC address of the Pico W in the Atari ST screen and system information banner at the bottom of the web pages.
- Added AGENTS.md file with instructions for AI agentic development. 

### Fixes
- The DMA memory copy to ROM in RAM gets the number of bytes to copy instead of words. For consistency with the microfirmwares.

---

## v2.0.8 (2025-10-16) - First stable release

This is the first stable release of the 2.0.x series. The code has been tested and is considered stable. 

### Changes
No changes in this release.

### New features
No new features in this release.

### Fixes
No fixes in this release.

---

## v2.0.7beta (2025-10-07) - Beta release

This is the second beta release. It includes all the new features and improvements for the version 2.0.x, and it will not include any more new features but only fixes and small improvements. The code is still in development and may contain bugs, but it is stable and ready to use for all users.

### Changes
- **Link to the Report Issues page**: A link to the Report Issues page has been added to the web app at the bottom of the page. This makes it easier for users to report any issues they encounter while using the app.

### New features
- **New ./notreboot file**: A new `./notreboot` file has been introduced. If this file is present on the microSD card, the device will not reboot automatically if the user performs a reset to the factory settings. This feature is useful during the development and testing phases, because combined with the `./wificonf` file, it allows to test the device without the need to reconfigure the WiFi settings after each reset.

### Fixes
No fixes in this release.

---

## v2.0.6beta (2025-08-14) - Beta release

This is the first beta release. It includes all the new features and improvements for the version 2.0.x, and it will not include any more new features. The code is still in development and may contain bugs, but it is more stable than previous alpha releases and ready to use for all users.

### Changes
- **Show information in the web app when launched app**: The web app will now display relevant information and tips when launched, helping users to get started quickly and easily.
- **Removed USB Mass Storage support**: The USB Mass Storage feature has been removed. Now, to use the Mass storage mode the user has to launch the File Manager or Multi-drive microfirmwares.
- **Disable ICMP ping responses**: ICMP ping responses have been disabled for improved reliability.
- **Better network deinitialization**: The network deinitialization process has been improved to avoid the network to stuck when changing from the "Factory" mode to the "Booster" mode.

### New features
- **Firmware update notifications**: The web app will now notify users when a new firmware update is available.
- **Firmware upgrade process**: The firmware upgrade process has been streamlined for easier updates. Now the users can initiate the upgrade process directly from the web app, without needing to manually download and install firmware files.
- **mDNS support**: The web app now supports mDNS (Multicast DNS), allowing for easier discovery and connection to devices on the local network. Now the site "sidecart.local" is accessible via mDNS. Previously it was only through DHCP plus DNS, which small routers from telecom providers often struggle with.

### Fixes
- Multiple small bug fixes and improvements.

## v2.0.5alpha (2025-07-02) - Alpha release

This is a rolling alpha release. It may include new features, bugs, and issues. Bug and issue tracking is not yet available as this is not a public release. This is a development version.

### Changes
- **Highlight microfirmware updates available**: The system will now highlight available microfirmware updates, making it easier for users to identify and apply important updates.

- **Remove Safe SELECT reboot option**: The Safe SELECT reboot option has been removed from the configuration settings. Not needed anymore.

### New features
No new features in this release.

### Fixes
- **Display tooltips for configuration options**: Tooltips did not display for various configuration options, providing users with helpful information and guidance when making changes.

## v2.0.4alpha (2025-06-13) - Alpha release

This is a rolling alpha release. It may include new features, bugs, and issues. Bug and issue tracking is not yet available as this is not a public release. This is a development version.

### Changes
- **Disabled USB Mass storage in Booster**: The USB Mass Storage feature has been disabled in the Booster firmware to prevent potential issues with device stability and performance.
- **Display HTML description**: Now the description accepts HTML tags, allowing for more flexible and rich text formatting in the device description field.
- **Stay in Booster mode**: The device will now remain in Booster mode after a reboot, ensuring that users can continue to use the Booster features without needing to re-enter the mode.

### New features
No new features in this release.

### Fixes
No fixes in this release.

## v2.0.3alpha (2025-06-13) - Alpha release

This is a rolling alpha release. Except new features, bugs and issues. No tracking yet of bugs and issues since there is no public release yet. This is a development version.

### Changes
- **Better retry logic for Wifi connection**: The retry logic for WiFi connections has been improved to handle connection failures more gracefully. This should reduce the number of failed connections and improve overall reliability.
- **Better error messages**: Error messages have been enhanced to provide more detailed information about issues.

### New features
- **New .wificonf file**: A new `.wificonf` file has been introduced to allow users to configure WiFi settings right from the microSD card. This file can be used to set up WiFi credentials, making it easier to connect to WiFi networks without needing to access the web interface.

### Fixes
- **URL decoding**: Fixed an issue where the decoding of the password sent from the web interface to the device was not working correctly. This could lead to connection issues if special characters were included in the password.

## v2.0.1alpha (2025-06-06) - Alpha release

This is a rolling alpha release. Except new features, bugs and issues. No tracking yet of bugs and issues since there is no public release yet. This is a development version.

### Changes
- **Slow USB Mass Storage**: Trying to fix the issues when mounting USB Mass Storage mode that hangs the system. A slower USB Mass Storage mode is now enabled by default.
- **Automatic build of the full image**: The full image is now automatically built and uploaded to the repository. This ensures that the latest version is always available for users with the drag and drop feature, not just for the `picotool` command line tool users.

### New features
- **New HELP link**: A new HELP link has been added to the Management page. This link provides users with quick access to documentation and support resources, enhancing the user experience.

### Fixes
Everything is a massive and ongoing fix...

## v2.0.0alpha (2025-06-05) - Alpha release
- First version
