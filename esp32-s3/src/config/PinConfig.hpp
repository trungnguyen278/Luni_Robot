#pragma once

// GPIO Pin Assignments for ESP32-S3
// Responsibilities: ST7789 Display (SPI), Battery ADC, BLE, UART to C5

namespace PinConfig {

// --- SPI Display (ST7789 240x320) ---
static constexpr int SPI_MOSI  = 11;
static constexpr int SPI_SCLK  = 12;
static constexpr int LCD_CS    = 10;
static constexpr int LCD_DC    = 13;
static constexpr int LCD_RST   = 14;
static constexpr int LCD_BL    = 21;  // Backlight PWM

// --- Battery ADC ---
static constexpr int BATTERY_ADC = 1;  // ADC1_CH0 = GPIO1

// --- Charge status (optional, GPIO_NUM_NC if not wired) ---
static constexpr int CHG_STATUS = -1;  // Not connected
static constexpr int CHG_FULL   = -1;  // Not connected

// --- UART to ESP32-C5 ---
static constexpr int UART_TX   = 17;
static constexpr int UART_RX   = 18;
static constexpr int UART_NUM  = 1;
static constexpr int UART_BAUD_RATE = 115200;

// --- Button (optional on S3 side) ---
static constexpr int BUTTON    = 0;  // BOOT button

// --- Battery voltage divider resistors (ohms) ---
static constexpr float BAT_R1 = 10000.0f;
static constexpr float BAT_R2 = 20000.0f;

} // namespace PinConfig
