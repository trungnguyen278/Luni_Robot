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
//   UART bridge to C5, SPI3 audio bridge to C5, Camera (OV2640).
//   The user button has MOVED to the C5 (push-to-talk). The I2C motion bus
//   (PCA9685 servos + MPU6050 IMU) has ALSO MOVED to the C5 — it is low-rate and
//   the CAM board has no spare pins; freeing GPIO3/46 (old SDA/SCL) gave the
//   audio SPI3 lane its pins. See esp32-c5/src/config/PinConfig.hpp.
//
// CROSS-MCU WIRING (must match esp32-c5/src/config/PinConfig.hpp):
//   SPI3 audio lane (S3 master <-> C5 slave):
//     S3 SCLK 46 <-> C5 SCLK 4     S3 MOSI 3  -> C5 MOSI 2
//     S3 MISO 38 <-  C5 MISO 3     S3 CS   48 -> C5 CS   5
//     S3 HS   40 <-  C5 HS   8  (C5 raises HS HIGH when downlink data ready)
//   UART control/status:  S3 TX 19 -> C5 RX 6     S3 RX 20 <- C5 TX 7
//
// NOTE: camera/SD pin numbers follow the standard Freenove ESP32-S3-WROOM
//       pinout. Cross-check against the official `camera_pins.h` before flashing.
// =============================================================================

namespace PinConfig {

// --- SPI Display (ST7789 240x320) on SPI2 -----------------------------------
// Relocated off the low GPIOs (now owned by the camera). GPIO38 (RST) and GPIO40
// (CS) were FREED for the SPI3 audio lane:
//   * LCD_RST=-1: the panel's RST is tied to 3V3 on the board; DisplayDriver
//     already issues a software reset (ST7789 SWRESET 0x01) at init, so no GPIO
//     reset is needed.
//   * LCD_CS=-1: the ST7789 is the ONLY device on SPI2, so its CS is tied to GND
//     (permanently selected). ESP-IDF runs the bus with spics_io_num=-1.
static constexpr int SPI_MOSI  = 41;
static constexpr int SPI_SCLK  = 42;
static constexpr int LCD_CS    = 40;  // CHỐT: CS BẮT BUỘC chân SẠCH (non-strapping). GPIO40 là chân sạch trống duy nhất → CS module -> GPIO40.
                                       // GPIO0 & GPIO45 ĐỀU FAIL cho CS (chân strapping có tụ → méo cạnh tốc-độ-cao). Chỉ hợp tín hiệu chậm.
                                       // GPIO40 vốn là SPI3 handshake → phải BỎ handshake (S3 poll + C5 slave gapless) để nhường cho CS.
static constexpr int LCD_DC    = 39;
static constexpr int LCD_RST   = -1;  // CHỐT: RST tie 3V3 chạy tốt (RST chưa bao giờ là vấn đề). RST module -> 3V3.
static constexpr int LCD_BL    = 47;  // Backlight PWM (LEDC_CHANNEL_0 / TIMER_0)

// --- Battery ADC (-1 = no battery, skips PowerManager) ----------------------
// -1 = NO battery sensor wired. DeviceProfile must NOT report BOOT_POWER=OK in
// this case (it would be a fake reading); the POWER boot-check + battery UI are
// hidden until a real ADC divider exists. ADC1_CH0 = GPIO1 is used by I2S BCLK.
static constexpr int BATTERY_ADC = -1;

// --- Charge status (optional, -1 if not wired) ------------------------------
static constexpr int CHG_STATUS = -1;
static constexpr int CHG_FULL   = -1;

// --- Button -----------------------------------------------------------------
// MOVED to ESP32-C5 (PinConfig::USER_BUTTON). S3 no longer reads a button;
// it follows the LISTENING interaction state pushed by C5 over UART.
// (kept as -1 so any legacy `BUTTON >= 0` guard stays disabled)
static constexpr int BUTTON    = -1;

// --- I2S Audio (Full-Duplex, shared BCLK/WS for mic+speaker) ----------------
static constexpr int I2S_BCLK  = 1;   // Shared bit clock (BCLK on MAX98357, SCK on Mic)
static constexpr int I2S_WS    = 2;   // Shared word select (LRC on MAX98357, WS on Mic)
static constexpr int I2S_DIN   = 14;  // Mic data in (SD on Mic)
static constexpr int I2S_DOUT  = 21;  // Speaker data out (DIN on MAX98357)
static constexpr int I2S_NUM   = 0;   // I2S0

// --- Speaker (MAX98357) control ---------------------------------------------
static constexpr int SPK_SD    = 45;  // MAX98357 shutdown (LOW=off, HIGH=on). GPIO45 = VDD_SPI strapping nhưng SD đổi chậm +
                                       // S3 tự lái + MAX98357 SD là input high-Z → an toàn boot, tụ vô hại. (CS cần chân sạch nên không dùng 45.)
static constexpr int SPK_GAIN  = -1;  // Hard-wire GAIN (float = 9dB) to save a pin

// --- UART Bridge to ESP32-C5 (control/status messages) ----------------------
// Relocated to GPIO19/20. This SACRIFICES the native USB-OTG port; flashing and
// logging still work via UART0 (GPIO43/44) through the on-board CP2102.
static constexpr int UART_TX       = 19;  // S3 GPIO19 -> C5 GPIO6 (C5 RX)
static constexpr int UART_RX       = 20;  // S3 GPIO20 <- C5 GPIO7 (C5 TX)
// Higher baud for control/status + camera image chunks. MUST match the C5.
static constexpr int UART_BAUD     = 460800;

// --- SPI3 Bridge to ESP32-C5 (S3=Master, C5=Slave) --------------------------
// RE-ENABLED. The continuous Opus audio stream (mic uplink + TTS downlink) runs
// on its own SPI3 lane so it gets DMA + does not contend with the byte-by-byte
// UART control path (or block on a camera image burst). Pins were freed by
// moving the I2C motion bus to the C5 (GPIO3/46) and dropping the now-unused
// LCD_RST/LCD_CS (GPIO38/40) + the RGB status LED (GPIO48). SPI3 uses the GPIO
// matrix so any free pin works. MUST match the C5 SPI slave pins.
static constexpr int SPI3_MOSI      = 3;   // -> C5 GPIO2. SPI3 audio (S3 poll + C5 gapless). SpiBridge init giờ NON-FATAL nên
                                            // display luôn boot dù SPI3 lỗi → đọc log để biết SPI3 init OK/fail. Để -1 nếu muốn bench yên tĩnh.
static constexpr int SPI3_MISO      = 38;  // <- C5 GPIO3  (was LCD_RST)
static constexpr int SPI3_SCLK      = 46;  // -> C5 GPIO4  (was I2C SCL)
static constexpr int SPI3_CS        = 48;  // -> C5 GPIO5  (was RGB LED)
static constexpr int SPI3_HANDSHAKE = -1;  // BỎ: GPIO40 nay là LCD_CS (line 42). KHÔNG để =40 — sẽ đè display CS → màn đen khi SPI3 bật.
                                            // S3 poll thay HS (SpiBridge guard HS<0) + C5 slave gapless. Tháo dây C5-GPIO8↔S3-GPIO40.

// --- Battery voltage divider resistors (ohms) -------------------------------
static constexpr float BAT_R1 = 10000.0f;
static constexpr float BAT_R2 = 20000.0f;

// NOTE: the I2C motion bus (PCA9685 servos + MPU6050 IMU) and its servo config
// MOVED to the ESP32-C5 — see esp32-c5/src/config/PinConfig.hpp (namespace I2C)
// and esp32-c5 MotionConfig.hpp. GPIO3/46 are now SPI3 (above).

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
// The board's SDMMC is hard-wired to GPIO 38/39/40. GPIO39 is LCD_DC; GPIO38 and
// GPIO40 are now the SPI3 audio lane (MISO / handshake). SD stays deferred.
// Pins documented here for reference only — DO NOT init SD with this map.
namespace SD {
    static constexpr bool ENABLED = false;
    static constexpr int  CLK = 39;   // (in use by LCD_DC)
    static constexpr int  CMD = 38;   // (now SPI3_MISO)
    static constexpr int  D0  = 40;   // (now SPI3_HANDSHAKE)
}

} // namespace PinConfig
