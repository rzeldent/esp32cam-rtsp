# ESP32CAM-RTSP

![status badge](https://github.com/rzeldent/esp32cam-rtsp/actions/workflows/main.yml/badge.svg?event=push)

Simple [RTSP](https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol) server.
Easy configuration through the web interface. Stable.

Flashing this software on a esp32cam module will make it a **RTSP streaming camera** server.
This allows CCTV systems and applications like  **VLC** to connect directly to the ESP32CAM camera stream.
The RTSP protocol also allows to stream directly to a server using **ffmpeg**.
This makes the module a camera server allowing **recording** and  so the stream can be **stored on a disk** and replayed later.

This software supports the following ESP32-CAM modules:
- ESP32CAM
- AI THINKER
- TTGO T-CAM

![ESP32CAM module](assets/ESP32-CAM.jpg)

This software provides a **configuration web server**, that can be used to:
- Set the WiFi parameters,
- Set the timeout for connecting to the Access point,
- Set an access password,
- Select the board type,
- Select the image size,
- Select the frame rate,
- Select the JPEG quality

The software provides contains also a mDNS server to be easily discoverable on the local network.

## Required

- ESP32-CAM module
- USB to Serial (TTL level) converter or the piggyback board ESP32-CAM-MB
- [**PlatformIO**](https://platformio.org/) software (free download)

## Installing and running PlatformIO
More information can be found at: [https://docs.platformio.org/en/latest/installation.html](https://docs.platformio.org/en/latest/installation.html) below the basics.

### Debian based systems command-line
Install platformIO
```
 sudo apt-get install python-pip
 sudo pip install platformio
 pio upgrade
```

### Windows / Linux and Mac
Install [**Visual Studio code**](https://code.visualstudio.com) and install the PlatformIO plugin.
For command line usage Python and PlatformIO-Core is sufficient.

## Putting the ESP32-CAM in download mode

### ESP32-CAM-MB
When using the ESP32-CAM-MB board, press and hold the GP0 button on the ESP32-CAM-MB board.
Then press short the reset button (on the inside) on the ESP32-CAM board and release the GP0 button.
This will put the ESP32-CAM board in download mode.

### FTDI adapter
When using an FTDI adapter, make sure the adapter is set to 3.3 volt before connecting. Use the wiring schema below.

![ESP FTDI wiring](assets/ESP32CAM-to-FTDI.png)

After programming remove the wire to tge GPIO00 pin to exit the download mode.

## Compiling and deploying the software

Open a command line or terminal window and clone this repository from GitHub.

```
git clone https://github.com/rzeldent/esp32cam-rtsp.git
```
go into the folder
```
cd esp32cam-rtsp
```

Next, the firmware has to be build and deployed to the ESP32.
There are to flavours to do this; using the command line or the graphical interface of Visual Studio Code. I recommend to use VIsual Studio Code as it is free to use and offers more insight.

### Using the command line

First the source code and SPIFF partition (data) has to be compiled. Type:
```
pio run
pio run buildfs 
```

When finished, the SPIFF partition and software have to be uploaded.
Make sure the ESP32-CAM is in download mode (see previous section) and type:
```
 pio run -t upload
 pio run -t uploadfs
```

When done remove the jumper when using a FTDI adapter or press the reset button on the ESP32-CAM.
To monitor the output, start a terminal using:
```
 pio device monitor
```

### Using Visual studio

Open the project in a new window. Run the following tasks using the ```Terminal -> Run Task``` or CTRL+ALT+T command in the menu. Make sure the ESP32-CAM is in download mode during the uploads.

- PlatformIO: Build Filesystem Image (esp32cam)
- PlatformIO: Build (esp32cam)
- PlatformIO: Upload Filesystem Image (esp32cam)
- PlatformIO: Upload (esp32cam)

To monitor the behavior run the task
- PlatformIO: Monitor (esp32cam)


## Setting up the ESP32CAM-RTSP
After the programming of the ESP32, there is no configuration present. This needs to be added.
To connect initially to the device open the WiFi connections and select the WiFi network / accesspoint called **ESP32CAM-RTSP**.
Initially there is no password present.

After connecting, the browser should automatically open the status page.
In case this does not happens automatically, connect to [http://192.168.4.1](http://192.168.4.1).
This page will display the current settings and status. On the bottom, there is a link to the config. Click on this link.

This link brings up the configuration screen.

![Configuration screen](assets/Configuration.png)

Configure at least:
- The WiFi network settings. No dropdown is present to show available networks! Enter the Access point name manually.
- A password for accessing the Access point (AP) when starting. (required)
- the type of the ESP32-CAM board

When finished press Apply to save the configuration. The screen will redirect to the status screen.
Here it is possible to reboot the device so the settings take effect.
It is also possible to restart manually by pressing the reset button. 

## Connecting to the RTSP stream
RTSP stream is available at: [rtsp://esp32cam-rtsp.local:554/mjpeg/1](rtsp://esp32cam-rtsp.local:554/mjpeg/1).
This link can be opened with for example [VLC](https://www.videolan.org/vlc/).

 :warning: **Please be aware that there is no password present on the stream!**

## Connecting to the configuration
When a connection is made to [http://esp32cam-rtsp](http://esp32cam-rtsp) the status screen is shown.

![Status screen](assets/index.png)

Clicking on the ```change configuration``` button will open the configuration. It is possible that a password dialog is shown before entering.
If this happens, for the user enter 'admin' and for the password the value that has been configured as the Access Point password.

## Credits
esp32cam-ready depends on PlatformIO, Bootstap5 and Micro-RTSP by Kevin Hester.

