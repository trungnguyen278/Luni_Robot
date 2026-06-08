#pragma once

// =============================================================================
// Luni V2 - ESP32-C5 Pin Configuration
// =============================================================================
// ESP32-C5 has GPIO0-GPIO14 available.
// Responsibilities: WiFi, WebSocket, BLE, SPI slave bridge to S3,
//                   and the user push-to-talk button (moved here from S3 to
//                   free pins on the Freenove ESP32-S3-WROOM CAM board).
// Audio and display remain on S3.
// =============================================================================

namespace PinConfig {

// --- SPI Slave (S3=Master, C5=Slave) ---
static constexpr int SPI_MOSI      = 2;   // C5 GPIO2  <- S3 GPIO35  (MOSI)
static constexpr int SPI_MISO      = 3;   // C5 GPIO3  -> S3 GPIO36  (MISO)
static constexpr int SPI_SCLK      = 4;   // C5 GPIO4  <- S3 GPIO37  (SCLK)
static constexpr int SPI_CS        = 5;   // C5 GPIO5  <- S3 GPIO38  (CS)
static constexpr int SPI_HANDSHAKE = 8;   // C5 GPIO8  -> S3 GPIO39  (HIGH = C5 has data)

// --- SPI Configuration ---
static constexpr int SPI_NUM       = 2;   // SPI2

// --- UART Bridge to S3 (control/status messages) ---
static constexpr int UART_TX       = 7;   // C5 GPIO6  -> S3 GPIO17  (RX)
static constexpr int UART_RX       = 6;   // C5 GPIO7  <- S3 GPIO18  (TX)
// Higher baud for control/status + camera image chunks. MUST match the S3.
static constexpr int UART_BAUD     = 460800;

// --- NVS Reset Button (hold LOW at boot to erase NVS) ---
// Set to -1 to disable. Assign a real GPIO when hardware is ready.
static constexpr int NVS_RESET_BTN = 9;

// --- User push-to-talk button ---
// DISABLED (-1): replaced by on-device WAKE WORD on the S3 (mic side). The S3
// now triggers the listening turn and sends START to C5 over UART. Set a real
// GPIO here (e.g. 10) to bring back a physical push-to-talk button.
static constexpr int USER_BUTTON   = -1;

// --- Free GPIOs ---
// GPIO 11, 12, 13, 14 available for future use (mode/power/volume buttons,
// status LED, ...).

} // namespace PinConfig
