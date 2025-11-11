# Raspberry Pi Pico W Booster Loader  

![Discord](https://img.shields.io/discord/1160868666162823218?style=flat&label=Discord&link=https%3A%2F%2Fdiscord.com%2Finvite%2Fu73QP9MEYC)


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

## Downloading the project

> **Note**: The project is in alpha version so only the Alpha nightly builds are available. Use at your own risk. 

The project is available in the [Nightly releases section](https://github.com/sidecartridge/rp2-booster-bootloader/releases/tag/nightly) section.

## Installing the project

Go for the option that best suits you. Both options will install the bootloader on your Raspberry Pi Pico W.

###  Option 1: Using the drag-and-drop method
1. Connect the Raspberry Pi Pico W micro USB connector to your computer while holding the **BOOTSEL** button.
2. Your computer should recognize the device as the mass storage device `RPI-RP2`. Now you can release the **BOOTSEL** button.
3. Drag and drop the [Full Release](https://github.com/sidecartridge/rp2-booster-bootloader/releases/download/nightly/rp-booster-v1.0.0alpha-full.uf2) file to the `RPI-RP2` drive.
4. Wait for the file to be copied. When the copy is complete you can disconnect the Raspberry Pi Pico W from your computer.

### Option 2: Using picotool

With this method you must [first install the picotool](https://github.com/raspberrypi/picotool) helper tool, which is a command-line utility for managing Raspberry Pi devices based on the RP2040 microcontroller.

1. Connect the Raspberry Pi Pico W micro USB connector to your computer while holding the **BOOTSEL** button.
2. Your computer should recognize the device as the mass storage device `RPI-RP2`. Now you can release the **BOOTSEL** button.
3. Flash the bootloader on your board:
```bash
picotool load -xv dist/rp-booster-$VERSION.uf2
```


## Booster Quickstart Guide

### Initial Factory Configuration for SidecarTridge Multidevice on Atari ST Series Computers

1. Format a microSD card with either **exFAT** or **FAT32** file system and insert it into the SidecarTridge Multidevice board.

2. Plug the SidecarTridge Multidevice into the **cartridge port** of your Atari ST series computer.

3. Power on the Atari ST computer.

4. The Booster app will automatically start in **Factory (Fabric) mode**, showing the following message on screen:

   ![Booster Factory Mode step 1](/docs/BOOSTER-FABRIC-1.png)

   This screen confirms that the Booster app is running in Factory mode and is ready to be configured.  
   Scan the **QR code** with your smartphone to connect to the WiFi network created by the Booster app, or manually connect via your WiFi settings.  
   The default network details are:
   - **SSID**: `SIDECART`  
   - **Password**: `sidecart`

5. Once connected to the WiFi, the Booster app will detect the connection and show the following screen:

   ![Booster Factory Mode step 2](/docs/BOOSTER-FABRIC-2.png)

   This screen invites you to open a web browser and navigate to either:
   - `http://sidecart.local`  
   - or `http://192.168.4.1` (if `.local` domains aren’t supported)

6. In your web browser, enter the URL shown above to access the web interface.  
   You’ll be prompted to configure your home or office WiFi network.  
   Select your WiFi SSID from the list (you can click **Refresh** if it doesn’t appear), then click it to continue.  
   In this example, the network is `CHISMEROUTER`.

   ![Booster Factory Mode step 3](/docs/BOOSTER-FABRIC-3.png)

7. Enter your WiFi password and click the **Connect** button.

   ![Booster Factory Mode step 4](/docs/BOOSTER-FABRIC-4.png)

8. The Booster app will save the WiFi credentials to flash memory and reboot.  
   It will then connect to your WiFi network and show this message:

   ![Booster Factory Mode step 5](/docs/BOOSTER-FABRIC-5.png)

   Meanwhile, the device will also display a reboot message on the Atari screen:

   ![Booster Factory Mode step 6](/docs/BOOSTER-FABRIC-6.png)

9. After rebooting, the Booster app will attempt to connect to your configured WiFi network:

   ![Booster Manager Mode step 1](/docs/BOOSTER-MANAGER-1.png)

10. Once connected successfully, the screen will show:

    ![Booster Maneger Mode step 2](/docs/BOOSTER-MANAGER-2.png)

    Your WiFi setup is now complete. From now on, the Booster app will boot directly into **Manager mode**.  
    See the next section for details on using Manager mode.

## Booster Manager Mode

In **Manager mode**, the Booster app will automatically connect to your configured WiFi network and will automatically connect to the public repository of microfirmware apps. 

If your DHCP network supports a DNS with `.local` domains, you can access the web interface using `http://sidecart.local`. Otherwise, use the IP address assigned by your DHCP server and visible on your computer screen.

> **Note**: The device needs a SDHC or SDXC microSD card formatted with **exFAT** or **FAT32** file system to work properly. The app will warn the user if the microSD card is not present or not formatted correctly.

### Apps view

The **Apps view** shows:

- The list of instalable apps from the public repository.
- The list of installed apps on the device.
- The list of apps that are not installed but are available on the microSD card.

This view is the default view when you access the web interface.

![Booster Manager Apps View 1](/docs/BOOSTER-MANAGER-APPS-1.png)

Each app shows the following information:
- **Name**: The name of the app.
- **Version**: The version of the app.
- **Description**: A short description of the app.
- **Features**: A list of features supported by the app as a taxonomy.
- **Computer**: The computers supported by the app as a taxonomy.

At the right hand side of each app, you can see the following buttons:
- **Download**: Download the app from the public repository to the micro SD card. But it does not launch it yet.
- **Delete**: Delete the app from the device microSD card. You can download the app again if you want to.

![Booster Manager Apps View 2](/docs/BOOSTER-MANAGER-APPS-2.png)

- **Launch**: Launch the app. This will install the app in the flash memory of the device and after reboot it will be launched automatically. If you want to launch an app that is already installed, you can use the **Launch** button at any time.

![Booster Manager Apps View 3](/docs/BOOSTER-MANAGER-APPS-3.png)

When launching an app, the computer screen will show a message indicating that the app is being launched. The Booster app will make its best effort to also reboot the computer, but this may not always be possible. So assume that the computer is not rebooted and you need to do it manually.

The Booster microfirmware app is now installed in the flash memory of the device, and every time you power on the device, it will automatically launch the app. 

To return to the Booster app, the user needs to use the command menu on the microfirmware app to do it, or press the SELECT button for more than 10 seconds and then power off and power on the device and the computer.

> **Note**: In this Alpha version, the **DEV APP** is a placeholder development app for anybody who wants to develop a microfirmware app. It is not a real app and it does not do anything.

### WiFi view

The **WiFi view** shows the list of available WiFi networks and permits some basic network configuration options:

![Booster Manager WiFi View](/docs/BOOSTER-MANAGER-WIFI-1.png)

- **Country**: The country code of the WiFi network. This is used to configure the WiFi module to use the correct channels and frequencies.
- **Hostname**: The hostname of the device. This is used to identify the device on the network. By default is `sidecart`. 
- **Wifi Power**: The power of the WiFi module. From 0 to 4. 
- **Show RSSI**: Show the RSSI of the WiFi module. This is used to show the signal strength of the WiFi network.
- **DHCP Enabled**: Enable or disable the DHCP server. This is used to assign IP addresses to the devices connected to the Booster app. In Alpha, only DHCP is tested.

Don't forget to click the **Save** button to save the changes!

It's also possible to change the WiFi network. To do this, click on the new WiFi network from the list. A new window will open with the WiFi network details. Enter the password and click the **Connect** button.

![Booster Manager WiFi View 2](/docs/BOOSTER-MANAGER-WIFI-2.png)

The Booster app will save the WiFi credentials to flash memory and reboot. It will then connect to your WiFi network and show a message on the computer screen.

> **Note**: If the Booster app is not able to connect to the WiFi network, press SELECT button for ten seconds and power off and power on the device and the computer to start the Factory mode again.

### Device view

The **Device view** shows the device information and some basic configuration options:

![Booster Manager Device View](/docs/BOOSTER-MANAGER-CONFIG-1.png)

- **Apps folder**: The folder where the apps are stored. By default is `/apps`. This folder is created on the microSD card when the Booster app is launched for the first time if it does not exist.
- **Apps catalog URL**: The URL of the apps catalog. By default is `http://atarist.sidecartridge.com/apps.json`. You can configure your own apps catalog URL if you want to use your own apps. In the alpha version, the `apps.json` file must be hosted in a HTTP server. HTTPS is not supported yet.
- **Boot feature**: The UUID of the microfirmware app to load at boot time, or the Booster mode: `FABRIC` o `MANAGER`. If the app cannot find the UUID of the microfirmware in the flash memory, it will load the Booster app in **Manager mode**.
- **Safe SELECT reboot**: Not used in alpha version.
- **SD card baud rate (KB)**: The baud rate of the SD card. By default is `12500`. It can be safely increased to `25000`. Above this value, the SD card may not work properly or the value will be ignored.

Don't forget to click the **Save** button to save the changes!

It also provides two buttons to reboot the device:

- **Reboot the device**: Reboot the device and load the microfirmware app in flash memory. This is the normal way to reboot the device.
- **Reboot the device in default fabric settings**: Reboot the device and load the Booster app in **Factory mode**. This is useful to reset the device to factory settings or to change the WiFi network.


## License

The source code of the project is licensed under the GNU General Public License v3.0. The full license is accessible in the [LICENSE](LICENSE) file. 

The project can also be licensed under a commercial license. Please contact the author for more information.