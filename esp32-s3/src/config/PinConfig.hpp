#pragma once

// =============================================================================
// Luni V2 - ESP32-S3 Pin Configuration  (Freenove ESP32-S3-WROOM CAM)
// =============================================================================
// Board     : Freenove ESP32-S3-WROOM (N16R8, OCTAL PSRAM)
// Camera/SD : pins are HARD-WIRED by the board (see Camera namespace).
//             Octal PSRAM consumes GPIO 33-37 -> NOT usable.
//             Usable GPIOs: 0,1,2,3,14,19,20,21 and 38-48
//             (43/44 = UART0 console, 48 = onboard RGB LED).
//
// Responsibilities: Display (ST7789), I2S Audio (Mic+Speaker), Opus codec,
//   UART bridge to C5, I2C (PCA9685 servo driver + MPU6050 IMU), Camera (OV2640).
//   The user button has MOVED to the C5 (push-to-talk) to free pins.
//
// NOTE: camera/SD pin numbers follow the standard Freenove ESP32-S3-WROOM
//       pinout. Cross-check against the official `camera_pins.h` before flashing.
// =============================================================================

namespace PinConfig {

// --- SPI Display (ST7789 240x320) on SPI2 -----------------------------------
// Relocated off the low GPIOs (now owned by the camera) onto the freed SD-block
// pins (38/39/40) + 41/42/47. SD is therefore deferred (see SD namespace).
static constexpr int SPI_MOSI  = 41;
static constexpr int SPI_SCLK  = 42;
static constexpr int LCD_CS    = 40;
static constexpr int LCD_DC    = 39;
static constexpr int LCD_RST   = 38;
static constexpr int LCD_BL    = 47;  // Backlight PWM (LEDC_CHANNEL_0 / TIMER_0)

// --- Battery ADC (-1 = no battery, skips PowerManager) ----------------------
static constexpr int BATTERY_ADC = -1;  // ADC1_CH0 = GPIO1 (now used by I2S BCLK)

// --- Charge status (optional, -1 if not wired) ------------------------------
static constexpr int CHG_STATUS = -1;
static constexpr int CHG_FULL   = -1;

// --- Button -----------------------------------------------------------------
// MOVED to ESP32-C5 (PinConfig::USER_BUTTON). S3 no longer reads a button;
// it follows the LISTENING interaction state pushed by C5 over UART.
// (kept as -1 so any legacy `BUTTON >= 0` guard stays disabled)
static constexpr int BUTTON    = -1;

// --- I2S Audio (Full-Duplex, shared BCLK/WS for mic+speaker) ----------------
static constexpr int I2S_BCLK  = 1;   // Shared bit clock
static constexpr int I2S_WS    = 2;   // Shared word select
static constexpr int I2S_DIN   = 14;  // ICS43434 mic data in
static constexpr int I2S_DOUT  = 21;  // MAX98357 speaker data out
static constexpr int I2S_NUM   = 0;   // I2S0

// --- Speaker (MAX98357) control ---------------------------------------------
static constexpr int SPK_SD    = 45;  // MAX98357 shutdown (LOW=off, HIGH=on)
                                       // GPIO45 = VDD_SPI strapping: default-LOW at
                                       // boot keeps amp muted, set HIGH after boot.
static constexpr int SPK_GAIN  = -1;  // Hard-wire GAIN (float = 9dB) to save a pin

// --- UART Bridge to ESP32-C5 (control/status messages) ----------------------
// Relocated to GPIO19/20. This SACRIFICES the native USB-OTG port; flashing and
// logging still work via UART0 (GPIO43/44) through the on-board CP2102.
static constexpr int UART_TX       = 19;  // S3 GPIO19 -> C5 GPIO7 (RX)
static constexpr int UART_RX       = 20;  // S3 GPIO20 <- C5 GPIO6 (TX)
// Higher baud for control/status + camera image chunks. MUST match the C5.
static constexpr int UART_BAUD     = 460800;

// --- SPI3 Bridge to ESP32-C5 (S3=Master, C5=Slave) --------------------------
// Disabled (-1): no free pins on the CAM board. Audio downlink uses UART path.
static constexpr int SPI3_MOSI      = -1;
static constexpr int SPI3_MISO      = -1;
static constexpr int SPI3_SCLK      = -1;
static constexpr int SPI3_CS        = -1;
static constexpr int SPI3_HANDSHAKE = -1;

// --- Battery voltage divider resistors (ohms) -------------------------------
static constexpr float BAT_R1 = 10000.0f;
static constexpr float BAT_R2 = 20000.0f;

// =============================================================================
// I2C bus (shared) - PCA9685 servo driver + MPU6050 IMU
// =============================================================================
// Dedicated bus on strapping-tolerant pins (lines idle HIGH via pull-ups, only
// driven after boot). Optional optimisation: share the camera SCCB bus
// (Camera::SIOD/SIOC) at 100kHz to free SDA/SCL — not done here for robustness.
namespace I2C {
    static constexpr int SDA       = 3;
    static constexpr int SCL       = 46;
    static constexpr int PORT      = 0;            // I2C_NUM_0
    static constexpr int FREQ_HZ   = 400000;       // 400kHz (PCA9685 + MPU6050 OK)
    static constexpr int PCA9685_ADDR = 0x40;      // servo PWM driver
    static constexpr int MPU6050_ADDR = 0x68;      // IMU (AD0=GND)
}

// =============================================================================
// Servo control - 8 channels on the PCA9685 (NOT GPIO pins)
// =============================================================================
// S3 has only 8 LEDC channels and the backlight already owns CHANNEL_0, so 8
// servos cannot be driven directly. PCA9685 (16-ch, 50Hz, separate V+ supply)
// is used over I2C. Per-servo calibration lives in MotionConfig.hpp.
namespace Servo {
    static constexpr int COUNT      = 8;           // CH0..CH7 on PCA9685
    static constexpr int PWM_FREQ_HZ = 50;         // standard analog servo
}

// =============================================================================
// Camera (OV2640) - Freenove ESP32-S3-WROOM FIXED pins (DVP 8-bit)
// =============================================================================
namespace Camera {
    static constexpr int PWDN  = -1;
    static constexpr int RESET = -1;
    static constexpr int XCLK  = 15;
    static constexpr int SIOD  = 4;   // SCCB SDA
    static constexpr int SIOC  = 5;   // SCCB SCL
    static constexpr int VSYNC = 6;
    static constexpr int HREF  = 7;
    static constexpr int PCLK  = 13;
    static constexpr int D0    = 11;  // Y2
    static constexpr int D1    = 9;   // Y3
    static constexpr int D2    = 8;   // Y4
    static constexpr int D3    = 10;  // Y5
    static constexpr int D4    = 12;  // Y6
    static constexpr int D5    = 18;  // Y7
    static constexpr int D6    = 17;  // Y8
    static constexpr int D7    = 16;  // Y9
    // 10MHz XCLK (was 20MHz). Halving the clock halves the DVP data rate, giving
    // the no-PSRAM-DMA fallback path (Octal PSRAM, see CameraConfig) time to drain
    // QVGA lines without "EV-VSYNC-OVF". Capture is slower but we only need <=1Hz.
    // NOTE: the earlier 10MHz-vs-20MHz "NO-SOI" test was in JPEG mode (broken HW
    // encoder) — clock-independent there; this is the RGB565 path, where XCLK
    // does set PCLK. If frames still overflow, try 8MHz; if they go dark, the data
    // bus (D0-D7) integrity is suspect (hand-wired DevKitC-1).
    static constexpr int XCLK_FREQ_HZ = 10000000;  // 10MHz
}

// =============================================================================
// SD card - DEFERRED on this board
// =============================================================================
// The board's SDMMC is hard-wired to GPIO 38/39/40, which we now use for the
// ST7789 display. Display was prioritised, so SD cannot be enabled at the same
// time. To re-enable SD you must free 38/39/40 (e.g. drop the display or move
// it). Pins documented here for reference only — DO NOT init SD with this map.
namespace SD {
    static constexpr bool ENABLED = false;
    static constexpr int  CLK = 39;   // (in use by LCD_DC)
    static constexpr int  CMD = 38;   // (in use by LCD_RST)
    static constexpr int  D0  = 40;   // (in use by LCD_CS)
}

} // namespace PinConfig
