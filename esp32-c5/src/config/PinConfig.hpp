#pragma once

// =============================================================================
// PTalk V2 - ESP32-C5 Pin Configuration
// =============================================================================
// ESP32-C5 has GPIO0-GPIO14 available
// Adjust these pins to match your PCB layout
// =============================================================================

namespace PinConfig {

// --- I2S Audio (Full-duplex, shared clock) ---
// Mic (ICS43434) and Speaker (MAX98357) share BCLK/WS
static constexpr int I2S_BCLK  = 2;   // Shared bit clock
static constexpr int I2S_WS    = 3;   // Shared word select (LRCLK)
static constexpr int I2S_DIN   = 4;   // Mic data in  (ICS43434 SD)
static constexpr int I2S_DOUT  = 5;   // Speaker data out (MAX98357 DIN)

// --- UART (Communication with ESP32-S3) ---
static constexpr int UART_TX   = 6;   // C5 TX -> S3 RX
static constexpr int UART_RX   = 7;   // C5 RX <- S3 TX

// --- Button ---
static constexpr int BUTTON    = 8;   // Push button (active low, pull-up)

// --- UART Configuration ---
static constexpr int UART_NUM       = 1;       // UART1
static constexpr int UART_BAUD_RATE = 115200;

// --- I2S Configuration ---
static constexpr int I2S_NUM        = 0;       // I2S0 (only 1 on C5)

} // namespace PinConfig
