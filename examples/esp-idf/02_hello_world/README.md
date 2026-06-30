# Hello World

Classic ESP-IDF hello world example for ESP32-P4.

The app prints chip information, flash size, and heap information, then restarts
after a short countdown.

## Hardware Required

- ESP32-P4-WIFI6-Touch-LCD-7B board.
- USB cable for flashing and serial monitor.

## Build and Flash

```bash
cd examples/esp-idf/02_hello_world
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port.

## Expected Output

The serial monitor should print `Hello world!`, ESP32-P4 chip information, flash
size, heap size, and a restart countdown.
