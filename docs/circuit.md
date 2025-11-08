# Circuit documentation

## Summary

This circuit interfaces an ESP8266 ESP-01 Wi‑Fi module with a DHT22 temperature & humidity sensor. The ESP8266 connects to Wi‑Fi and publishes sensor readings to an MQTT broker. Power comes from USB and is regulated to 3.3V using an AMS1117. A pull-up resistor is used on the DHT22 data line.

## Components

1. ESP8266 ESP-01
	- Description: Small Wi‑Fi module used to connect to a network and run the MQTT client.
	- Pins: `TXD`, `CH_PD`, `RST`, `VCC`, `GND`, `GPIO_2`, `GPIO_0`, `RXD`

2. DHT22 (temperature & humidity sensor)
	- Description: Digital temperature and humidity sensor.
	- Pins: `VCC`, `DATA`, `NC`, `GND`

3. Pull-up resistor (for DHT22 data line)
	- Description: Pull-up resistor for the DHT22 data line.
	- Value: 4.7 kΩ (4700 Ω)
	- Pins: `pin1`, `pin2`

4. USB 2‑pin power connector
	- Description: Provides 5V power from a USB source.
	- Pins: `V+` (Positive), `GND` (Negative)

5. AMS1117 3.3V regulator
	- Description: Regulates USB 5V down to 3.3V for the ESP8266 and sensor.
	- Pins: `GND`, `OUT` (3.3V), `IN` (5V)

6. Electrolytic capacitor
	- Description: Voltage stabilization on the regulator output.
	- Typical value: 100 µF (0.0001 F)
	- Polarity: `-` (negative), `+` (positive)

## Wiring details

### ESP8266 (ESP-01)
 - `GND`: connect to regulator GND, USB GND, DHT22 GND, and capacitor `-`.
 - `CH_PD` (chip enable): tie to `VCC` (3.3V).
 - `VCC`: connect to AMS1117 `OUT` (3.3V).
 - `GPIO_0`: used for programming mode; if used for the DHT/data wiring follow your firmware wiring (commonly not used by the DHT data line). Ensure correct logic level for boot.

### DHT22
 - `VCC`: connect to AMS1117 `OUT` (3.3V).
 - `DATA`: connect to the microcontroller data pin through a pull-up resistor (4.7 kΩ to 3.3V).
 - `GND`: connect to system GND.

### Pull-up resistor
 - `pin1`: connected to the DHT22 `DATA` line (or microcontroller input if placed there).
 - `pin2`: connected to `VCC` (3.3V) as the pull-up.

### USB power connector
 - `GND` (Negative): connect to system GND.
 - `V+` (Positive): connect to AMS1117 `IN` (5V from USB).

### AMS1117 3.3V regulator
 - `GND`: system ground.
 - `OUT`: 3.3V output — connect to ESP8266 `VCC`, DHT22 `VCC`, capacitor `+`.
 - `IN`: 5V input from USB `V+`.

### Electrolytic capacitor
 - `-` (negative): connect to system GND.
 - `+` (positive): connect to AMS1117 `OUT` (3.3V) for smoothing.

---

Notes:
- Use a common ground for all modules.
- The DHT22 data line requires a pull-up (4.7 kΩ is a standard choice). Place the pull-up between DATA and 3.3V.
- Ensure the ESP8266 receives a stable 3.3V supply and that the regulator can supply sufficient current (ESP8266 can spike during Wi‑Fi transmissions).