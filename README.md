# Raspberry Pi Pico W Booster Loader  

This is an advanced second-stage bootloader for the Raspberry Pi Pico W and the SidecarTridge Multidevice board. The current version is designed for the RP2040-based Pico W, but future releases will support the RP235x series and move beyond the Atari ST architecture.  

## Key Features  

- **OTA (Over-The-Air) Updates**  
  The Booster app fetches and installs the latest apps or microfirmwares from a web server, making it easy to manage and update multiple solutions on a single device.  

- **Web-Based Interface**  
  Access and manage the device via a web browser. The interface simplifies app installation, updates, and system configuration without the need for additional software.  

- **Micro SD Card Support**  
  Store and manage files on a micro SD card, allowing apps to use external storage or turn the device into a file server.  

- **Mass Storage Mode**  
  The device can function as a USB mass storage device for easy file transfers.  

- **Multi-App Support**  
  Install and manage multiple app or microfirmwares simultaneously, ensuring flexible and dynamic usage.  

- **External Display Support**  
  Connect an external screen or run headless. Currently supports **Atari ST high and low resolutions**, with easy expansion to other display types.  

- **Global Settings**  
  Store system-wide configurations in flash memory, accessible via the web interface or terminal.  

## Booster components  

The Booster app is designed to be modular and extendable. The key components include:  

- **Booster Core**  
  The heart of the system, responsible for managing the device, loading/unloading apps, handling the micro SD card, and providing the web interface.  

- **Placeholder App**  
  A temporary app that runs when no other app is installed. It hands control over to the Booster Core and is replaced when an active app is loaded.  

- **Global Settings**  
  Stored in flash memory, these settings apply to the entire device and its apps.  

- **Local Settings**  
  Each app has its own isolated configuration stored in flash memory, ensuring app-specific settings remain independent.

- **Atari ST firmware**  
  The Booster app uses the Atari ST computer as the terminal for screen and keyboard input/output. A small custom firmware is included to handle the Atari ST's unique hardware and protocols.

## Memory Map  

The Pico W and Pico 2W feature a **2MB flash memory**, structured as follows:  

```plaintext
+-----------------------------------------------------+
|                                                     |
|              MICROFIRMWARE APP (1152K)              |  0x10000000
|          Active microfirmware flash space           |
|                                                     |
+-----------------------------------------------------+
|                                                     |
|                  BOOSTER CORE (768K)                |  0x10120000
|  - Manages device, apps, SD card, web interface     |
|  - Loads/unloads apps                               |
|                                                     |
+-----------------------------------------------------+
|                                                     |
|                  CONFIG FLASH (120K)                |  0x101E0000
|             Microfirmwares configurations           |
|                                                     |
+-----------------------------------------------------+
|                                                     |
|              GLOBAL LOOKUP TABLE (4K)               |  0x101FE000
|  Microfirmwares mapping to configuration sectors    |
|                                                     |
+-----------------------------------------------------+
|                                                     |
|              GLOBAL SETTINGS (4K)                   |  0x101FF000
|           Device-wide configurations                |
|                                                     |
+-----------------------------------------------------+
```

The **Booster Core** resides in **flash memory from 0x10120000 (768K)**. This region is only modified when updating the core itself.  

The **Config Flash (120K at 0x101E0000)** stores per-app settings, allowing up to **30 microfirmware apps**, each allocated **4K of flash memory**.  

A **Global Lookup Table (4K at 0x101FE000)** maps microfirmware apps to their configuration sectors using unique UUIDs.  

Finally, the **Global Settings (4K at 0x101FF000)** store system-wide configurations for the device and its microfirmware apps.  

Each **microfirmware app is loaded at 0x10000000** after powering on the device. It can be updated over the air or through the web interface.  

This design ensures efficient memory usage while providing a structured and extensible platform for managing multiple apps and configurations on the Raspberry Pi Pico W. 🚀 developement section.  

## Booster core libraries and components  

### External Libraries  

These libraries are in different repositories and are included as submodules:  

- Pico-SDK: The official Raspberry Pi Pico SDK, providing essential libraries and tools for development.
- Pico-Extras: A collection of additional libraries and utilities for the Pico platform, enhancing functionality and ease of use.
- [no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico): A library for interfacing with micro SD cards using the SPI protocol, enabling file system access and management.

### Internal Libraries  

These libraries are part of the project and are included in the repository:  

- cJSON: A lightweight JSON parsing library for C, used for handling JSON data in the web interface and configuration files.
- dhcpserver: A simple DHCP server implementation for the Pico, allowing the device to assign IP addresses to connected clients.
- dnserver: A DNS server implementation for the Pico, enabling hostname resolution for local network devices.
- httpc: A lightweight HTTP client library for making requests to web servers, used for fetching updates and resources.
- md5: A library for calculating MD5 hashes, used for verifying the integrity of downloaded files and updates.
- qrcodegen: A library for generating QR codes, allowing users to easily share device information and configurations.
- settings: The `rp-settings` library.
- u8g2: A graphics library for rendering text and images on various display types, used for the external display support in the Booster Core.

## Building the Project  

To build the project assuming you have already cloned this repository and the submodules, follow these steps:  

1. Install the required dependencies:
   - CMake
   - GCC ARM toolchain

2. Modify the `version.txt` file to set the version of the project. The file in the root directory will be copied to the different executables needed.  

3. Execute the following command to build the project in release mode:
   ```bash
   ./build.sh pico_w release 
   ```

4. To build the project in debug mode, execute the following command:
   ```bash
   ./build.sh pico_w debug 
   ```

## Installing the project  

[Install the picotool](https://github.com/raspberrypi/picotool) to flash the bootloader on your board:
```bash
picotool load -xv dist/rp-booster-$VERSION.uf2
```

## License

The source code of the project is licensed under the GNU General Public License v3.0. The full license is accessible in the [LICENSE](LICENSE) file. 

The project can also be licensed under a commercial license. Please contact the author for more information.