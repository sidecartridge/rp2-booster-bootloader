# Changelog

## v2.0.6alpha (2025-08-XX) - Alpha release

This is a rolling alpha release. It may include new features, bugs, and issues. Bug and issue tracking is not yet available as this is not a public release. This is a development version.

### Changes
- **Show information in the web app when launched app**: The web app will now display relevant information and tips when launched, helping users to get started quickly and easily.

### New features
- **mDNS support**: The web app now supports mDNS (Multicast DNS), allowing for easier discovery and connection to devices on the local network. Now the site "sidecart.local" is accessible via mDNS. Previously it was only through DHCP plus DNS, which small routers from telecom providers often struggle with.

### Fixes

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
