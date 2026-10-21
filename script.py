import time
from machine import Pin

# Port 0 device name mapped from the Zephyr devicetree
GPIO0 = "gpio@842500"
LED0_PIN = 28

# Initialize LED0 as an output
led = Pin((GPIO0, LED0_PIN), Pin.OUT)

# Blink 5 times
for i in range(5):
    led.value(0)  # Active-low ON
    time.sleep(2)
    led.value(1)  # Active-low OFF
    time.sleep(0.5)