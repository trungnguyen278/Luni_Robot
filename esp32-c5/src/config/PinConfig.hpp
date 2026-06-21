#pragma once

// =============================================================================
// Luni V2 - ESP32-C5 Pin Configuration   *** SINGLE SOURCE OF TRUTH ***
// =============================================================================
// ESP32-C5 has GPIO0-GPIO14 available (single RISC-V core).
// Responsibilities: WiFi, WebSocket, BLE, OTA, SPI-slave bridge to S3, the user
//   push-to-talk button, AND (moved here from the S3) the I2C motion bus:
//   PCA9685 servo driver + MPU6050 IMU. The I2C peripherals are low-rate and the
//   C5 core is mostly idle (network runs on DMA), so hosting them here frees the
//   pin-starved S3-CAM board for its audio SPI lane. See MotionManager.
// Audio, display and camera stay on the S3.
//
// CROSS-MCU WIRING (must match esp32-s3/src/config/PinConfig.hpp):
//   SPI3 audio lane (S3 master  <-> C5 slave):
//     S3 SCLK 46  <-> C5 SCLK 4      S3 MOSI 3  -> C5 MOSI 2
//     S3 MISO 38  <-  C5 MISO 3      S3 CS   48 -> C5 CS   5
//     S3 HS   40  <-  C5 HS   8   (C5 raises HS HIGH when downlink data ready)
//   UART control/status (S3 <-> C5):
//     S3 TX 19 -> C5 RX 6           S3 RX 20 <- C5 TX 7
// =============================================================================

namespace PinConfig {

// --- SPI Slave (S3=Master, C5=Slave) ----------------------------------------
// UNCHANGED. Only the S3-side GPIO numbers moved (see header table above).
static constexpr int SPI_MOSI      = 2;   // C5 GPIO2  <- S3 GPIO3   (MOSI)
static constexpr int SPI_MISO      = 3;   // C5 GPIO3  -> S3 GPIO38  (MISO)
static constexpr int SPI_SCLK      = 4;   // C5 GPIO4  <- S3 GPIO46  (SCLK)
static constexpr int SPI_CS        = 5;   // C5 GPIO5  <- S3 GPIO48  (CS)
static constexpr int SPI_HANDSHAKE = 8;   // C5 GPIO8  -> S3 GPIO40  (HIGH = C5 has data)

// --- SPI Configuration ---
static constexpr int SPI_NUM       = 2;   // SPI2

// --- UART Bridge to S3 (control/status messages) ----------------------------
static constexpr int UART_TX       = 7;   // C5 GPIO7  -> S3 GPIO20  (S3 RX)
static constexpr int UART_RX       = 6;   // C5 GPIO6  <- S3 GPIO19  (S3 TX)
// Higher baud for control/status + camera image chunks. MUST match the S3.
static constexpr int UART_BAUD     = 460800;

// =============================================================================
// I2C bus (shared) - PCA9685 servo driver + MPU6050 IMU   [MOVED from S3]
// =============================================================================
// Hosted on the C5 because the peripherals are low-rate (servo 50 Hz, IMU ~1 Hz)
// and the S3-CAM board has no free pins. Per-servo calibration + IMU thresholds
// live in MotionConfig.hpp; the bus is driven by MotionManager.
namespace I2C {
    static constexpr int SDA          = 12;       // C5 GPIO12 -> PCA9685 + MPU6050 SDA
    static constexpr int SCL          = 13;       // C5 GPIO13 -> PCA9685 + MPU6050 SCL
    static constexpr int PORT         = 0;        // I2C_NUM_0
    static constexpr int FREQ_HZ      = 400000;   // 400kHz (PCA9685 + MPU6050 OK)
    static constexpr int PCA9685_ADDR = 0x40;     // servo PWM driver
    static constexpr int MPU6050_ADDR = 0x68;     // IMU (AD0=GND)
    static constexpr int PWM_FREQ_HZ  = 50;       // standard analog servo
}

// =============================================================================
// Battery ADC (PowerManager)   [MOVED from S3 — S3-CAM had no free ADC pin]
// =============================================================================
// Read on the C5's ADC1. Only ADC1_CH0 (GPIO1) is free here (CH1-5 = GPIO2-6 are
// SPI/UART). Wire the battery divider tap to GPIO1. If nothing is wired, the
// driver reads floating (<40mV) → reports 255 → S3 hides the icon (no fake %).
// CHG/FULL = optional TP4056 status pins (LOW=active); -1 = not wired.
namespace Battery {
    static constexpr int ADC_CHANNEL = 0;    // ADC_CHANNEL_0 == GPIO1 (ADC1). -1 = disable.
    static constexpr int ADC_GPIO    = 1;    // for logging/reference
    static constexpr int CHG_STATUS  = -1;
    static constexpr int CHG_FULL    = -1;
    static constexpr float R1        = 10000.0f;   // divider high-side
    static constexpr float R2        = 20000.0f;   // divider low-side (tap = VBAT*R2/(R1+R2))
}

// --- NVS Reset Button (hold LOW at boot to erase NVS) -----------------------
// Guarded: erase only on a SUSTAINED hold (see DeviceProfile::shouldEraseNvs),
// not a single read, so a floating/glitching pin can't wipe WiFi creds + token.
// Set to -1 to disable entirely until a real button + pull-up exist on the HW.
static constexpr int NVS_RESET_BTN = 9;

// --- User push-to-talk button -----------------------------------------------
// DISABLED (-1): replaced by on-device WAKE WORD on the S3 (mic side). The S3
// triggers the listening turn and sends START to C5 over UART. Set a real GPIO
// here (e.g. 11) to bring back a physical push-to-talk button.
static constexpr int USER_BUTTON   = -1;

// --- Mic privacy-mute button (QĐ-10) ----------------------------------------
// Physical mute for the always-on mic. The mic is on the S3, but the S3-CAM
// board has NO free GPIO — the C5 does — so the toggle button is read here and a
// SET_MIC_MUTE command is pushed to the S3 over UART (it cuts the I2S input).
// active-low w/ internal pull-up (see TouchInput): an UNWIRED pin idles released,
// so a missing/floating button can never falsely mute. Set to -1 to disable.
// Because a free GPIO exists, QĐ-10's power-button-press-pattern fallback (for a
// pin-starved board) is NOT needed here.
static constexpr int MUTE_BUTTON   = 10;

// --- Free GPIOs ---
// GPIO 11, 14 available for future use (push-to-talk / mode / volume / LED ...).
// GPIO 10 = mic-mute button (above); GPIO 12/13 = I2C motion bus;
// GPIO 1 = battery ADC (above).

} // namespace PinConfig
