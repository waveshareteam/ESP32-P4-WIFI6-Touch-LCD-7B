#pragma once

#include <Arduino.h>
#include <string.h>

namespace serial_log {

inline void begin(unsigned long baud) {
  Serial.begin(baud);
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE && defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
}

inline void println(const char *message) {
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE && defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  if (!Serial) {
    return;
  }

  const size_t required = strlen(message) + 2;  // CRLF
  const int available = Serial.availableForWrite();
  if (available < 0 || static_cast<size_t>(available) < required) {
    return;
  }
#endif

  Serial.println(message);
}

template <typename... Args>
inline void printf(const char *format, Args... args) {
#if defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE && defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  if (!Serial) {
    return;
  }
#endif
  Serial.printf(format, args...);
}

}  // namespace serial_log
