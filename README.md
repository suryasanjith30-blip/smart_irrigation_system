# Smart Irrigation System — ESP32 + PlatformIO + Wokwi

This project is prepared for the Wokwi VS Code extension and PlatformIO.

## Important simulation note

The physical design/code specifies a DHT11. Wokwi's currently documented temperature/humidity part is DHT22, so `src/main.cpp` is set to `DHT22` for the Wokwi simulation. For the physical DHT11 build, change:

```cpp
#define DHT_TYPE DHT22
```

back to:

```cpp
#define DHT_TYPE DHT11
```

The rest of the control logic is unchanged.

## Sensor controls in Wokwi

- Soil A: potentiometer connected to GPIO34
- Soil B: potentiometer connected to GPIO35
- Tank level: potentiometer connected to GPIO33
- LDR: photoresistor AO connected to GPIO32
- Rain: press the red Rain button; it drives GPIO14 LOW
- DHT: DHT22 simulation connected to GPIO4
- Manual A: GPIO27
- Manual B: GPIO16
- Buzzer silence/reset: GPIO17
- Servo valve: GPIO18
- Relay A / Pump A: GPIO25
- Relay B / Pump B: GPIO26
- Buzzer: GPIO13
- LCD I2C: SDA GPIO21, SCL GPIO22, address 0x27

## Run in VS Code

1. Open this folder in VS Code.
2. Make sure PlatformIO IDE and Wokwi for VS Code are installed.
3. PlatformIO will install the libraries from `platformio.ini`.
4. Click **PlatformIO: Build**.
5. After a successful build, open `diagram.json`.
6. Start the Wokwi simulation.

The Wokwi configuration points to:
- `.pio/build/esp32dev/firmware.bin`
- `.pio/build/esp32dev/firmware.elf`

Do not manually create the compiled `.pio/build/...` files; PlatformIO generates them during Build.
