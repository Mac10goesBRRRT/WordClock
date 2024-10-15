# Wordclock - ESP32

A Wordclock is a clock that represents the current time as a sentence (e.g., _"It is 5 past 10"_). This version updates the sentence every 5 minutes, with LEDs in the corners that display +1 to +4 intervals. The clock follows a 12-hour format.

The project leverages features of the ESP32 development boards such as WiFi and a filesystem (FileFS). The controller connects to the internet and fetches the time from NTP servers. The LEDs are addressable WS2812b LEDs, which simplifies the design by eliminating the need for multiplexing as in similar projects. You can also customize the LED colors.

## Setup

To use the code:

1. Download [Visual Studio Code](https://code.visualstudio.com/) and install the [PlatformIO](https://platformio.org/) extension.
2. Download or clone this repository and open it in VS Code.
3. Connect your ESP32 NodeMCU to the PC (currently, only the ESP32 NodeMCU is supported).
4. Open the _PlatformIO_ sidebar and click the _Upload_ option to upload the code to your device.

You can monitor the upload process by selecting _Monitor/Upload and Monitor_, but this is optional.

### Connecting to WiFi

After uploading, connect to the clock's WiFi network `ClockWifi` (password: `Placeholder`). Open a browser and go to `Wordclock.local`. On the _Setup Page_, select your WiFi network, enter your password, and click _Connect_. If the connection is successful, you'll be directed to a _Settings Page_, where you can configure the language, timezone, brightness, and more (currently a work in progress).

### Factory Reset

To reset the settings, unplug the clock, press and hold button _B1_, then plug the power back in. After about 3 seconds, the reset will be complete, and you can repeat the "Connecting to WiFi" steps mentioned above.

## Assembly

Assembly instructions and BOM are coming soon!
