# ESP32-S3 ESP-NOW RC Car Project

This project implements a wireless RC car control system using the **ESP-NOW** protocol on two ESP32-S3 microcontrollers. It uses **FreeRTOS** queues to handle high-speed data transmission without crashing the Wi-Fi stack.

## Architecture

### 1. Sender (Remote Control)
* **Hardware:** ESP32-S3 (connected to Joystick/Buttons).
* **Function:** Reads inputs and broadcasts them via ESP-NOW.
* **Features:** Uses `esp_now_send` with a confirmation callback to ensure packet delivery.

### 2. Receiver (The Car)
* **Hardware:** ESP32-S3 (connected to Motors/Servos).
* **Function:** Listens for packets and drives the hardware.
* **Features:**
    * **Producer-Consumer Pattern:** The ESP-NOW callback (ISR context) strictly copies data to a FreeRTOS Queue.
    * **Processing Task:** A separate task consumes the queue to control motors safely.

## Project Structure

* `sender/` - Code for the remote controller.
* `receiver/` - Code for the vehicle.

## How to Build

1.  **Setup:** Install ESP-IDF extension in VS Code.
2.  **Target:** Set target to `esp32s3` for both projects.
3.  **Flash Receiver:**
    * Open `receiver` folder.
    * Build & Flash.
    * **Note:** Copy the MAC address printed in the serial monitor!
4.  **Configure Sender:**
    * Open `sender/main/main.c`.
    * Paste the Receiver's MAC address into the `receiver_mac` array.
5.  **Flash Sender:**
    * Open `sender` folder.
    * Build & Flash.