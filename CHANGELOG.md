# Changelog

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
