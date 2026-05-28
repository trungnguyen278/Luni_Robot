# Luni BLE Mobile App Developer Guide

## Overview

Document cho **mobile app developers** de implement BLE provisioning cho Luni.
App su dung BLE GATT de cau hinh WiFi, WebSocket URL, va device token.

Luni su dung **3 cap do truy cap** (Access Level) de bao ve cac thao tac nhay cam.

---

## 1. BLE Service

### Service UUID

```
Service: 0xFF01 (Luni Config Service)
```

### Scanning

- **Device Name:** `Luni` hoac custom name
- **Advertising:** Connectable, Scannable
- **MTU:** Khuyen nghi 512

---

## 2. Characteristics

### Full Characteristics Table

| UUID   | Name         | Properties | Min Level | Description |
|--------|--------------|------------|-----------|-------------|
| 0x0001 | SSID         | R/W        | 1         | WiFi SSID (max 32 chars) |
| 0x0002 | PASSWORD     | W          | 1         | WiFi Password (max 64 chars, write-only) |
| 0x0003 | WS_URL       | R/W        | 1         | WebSocket server URL |
| 0x0004 | DEV_TOKEN    | W          | 1         | Device authentication token (write-only) |
| 0x0005 | USER_ID      | R/W        | 0         | User namespace ID |
| 0x0006 | DEVICE_INFO  | R          | 0         | JSON: name, model, version, MAC |
| 0x0007 | DIAG_INFO    | R          | 0         | JSON: heap, uptime, connection state |
| 0x0010 | AUTH_UNLOCK  | W          | 0         | Write 6-digit PIN to unlock Level 1 |
| 0x0011 | COMMAND      | W          | 1/2       | BLE command byte (see §4) |
| 0x0012 | ADMIN_AUTH   | W          | 1         | HMAC-SHA256 admin auth (unlock Level 2) |
| 0x0013 | LOG_LEVEL    | R/W        | 2         | Log verbosity (0=OFF, 5=VERBOSE) |
| 0x0014 | ADMIN_SECRET | W          | 2         | Set admin secret key (write-only) |

---

## 3. Access Levels

### Level 0 (Unauthenticated)
- Read DEVICE_INFO, DIAG_INFO, USER_ID
- Write AUTH_UNLOCK (to enter Level 1)

### Level 1 (PIN Authenticated)
- All Level 0 permissions
- Read/Write: SSID, WS_URL
- Write: PASSWORD, DEV_TOKEN, USER_ID
- Write: COMMAND (RESTART only)
- Write: ADMIN_AUTH (to enter Level 2)

### Level 2 (Admin)
- All Level 1 permissions
- Write: ADMIN_SECRET, LOG_LEVEL
- Write: COMMAND (all commands including FACTORY_RESET, FULL_WIPE, etc.)

### PIN Authentication (Level 0 -> Level 1)

PIN 6 chu so duoc hien thi tren man hinh robot. App gui PIN vao `AUTH_UNLOCK` (0x0010):

```
Write "123456" -> 0x0010
// Ket qua tra ve qua notification tren COMMAND (0x0011):
//   0x00 = OK (Level 1 granted)
//   0x01 = FAIL (wrong PIN)
```

### Admin Authentication (Level 1 -> Level 2)

Su dung HMAC-SHA256 voi timestamp de chong replay attack:

```
Payload: [timestamp:4 LE][hmac:32]
  - timestamp: Unix timestamp (UTC), +-5 phut tolerance
  - hmac: HMAC-SHA256(admin_secret, timestamp_bytes)

Write payload -> 0x0012
// Ket qua: 0x00=OK, 0x01=FAIL, 0x02=UNAUTHORIZED
```

---

## 4. BLE Commands

Write 1 byte vao COMMAND (0x0011):

| Byte | Command        | Min Level | Description |
|------|----------------|-----------|-------------|
| 0x01 | RESTART        | 1         | Restart device |
| 0x10 | FACTORY_RESET  | 2         | Xoa WiFi, device_token, admin_secret |
| 0x11 | FULL_WIPE      | 2         | Xoa toan bo NVS |
| 0x12 | ROLLBACK_FW    | 2         | Rollback firmware (OTA) |
| 0x13 | ENABLE_DEBUG   | 2         | Bat debug logging |
| 0x14 | DISABLE_DEBUG  | 2         | Tat debug logging |
| 0x15 | CLEAR_USERS    | 2         | Xoa danh sach users |
| 0x16 | ENTER_DFU      | 2         | Vao DFU mode |

Command response tra ve qua notification:
- `0x00` CMD_OK
- `0x01` CMD_FAIL
- `0x02` CMD_UNAUTHORIZED

---

## 5. Provisioning Flow

```
+---------------------------------------------------+
|  1. SCAN & CONNECT                                |
|  - Scan for devices with service 0xFF01           |
|  - Connect to selected device                     |
|  - Discover services & characteristics            |
|  - Request MTU 512                                |
+-------------------------+-------------------------+
                          |
                          v
+---------------------------------------------------+
|  2. READ DEVICE INFO (Optional)                   |
|  - Read 0x0006 -> device name, model, version     |
|  - Read 0x0007 -> diagnostics                     |
+-------------------------+-------------------------+
                          |
                          v
+---------------------------------------------------+
|  3. AUTHENTICATE (PIN)                            |
|  - Read PIN from robot screen (6 digits)          |
|  - Write PIN -> 0x0010                            |
|  - Wait for notification: 0x00 = success          |
+-------------------------+-------------------------+
                          |
                          v
+---------------------------------------------------+
|  4. WRITE CONFIG                                  |
|  - Write 0x0001 <- WiFi SSID                     |
|  - Write 0x0002 <- WiFi Password                 |
|  - Write 0x0003 <- WebSocket URL                 |
|  - Write 0x0004 <- Device Token                  |
|  - Write 0x0005 <- User ID (optional)            |
+-------------------------+-------------------------+
                          |
                          v
+---------------------------------------------------+
|  5. RESTART                                       |
|  - Write 0x01 -> 0x0011 (RESTART command)         |
|  - Device saves config to NVS & restarts          |
|  - BLE connection will be lost (expected)         |
+---------------------------------------------------+
```

---

## 6. Code Examples

### Android (Kotlin)

```kotlin
val SERVICE_UUID = UUID.fromString("0000FF01-0000-1000-8000-00805f9b34fb")
val CHAR_SSID      = UUID.fromString("00000001-0000-1000-8000-00805f9b34fb")
val CHAR_PASSWORD   = UUID.fromString("00000002-0000-1000-8000-00805f9b34fb")
val CHAR_WS_URL     = UUID.fromString("00000003-0000-1000-8000-00805f9b34fb")
val CHAR_DEV_TOKEN  = UUID.fromString("00000004-0000-1000-8000-00805f9b34fb")
val CHAR_USER_ID    = UUID.fromString("00000005-0000-1000-8000-00805f9b34fb")
val CHAR_AUTH       = UUID.fromString("00000010-0000-1000-8000-00805f9b34fb")
val CHAR_COMMAND    = UUID.fromString("00000011-0000-1000-8000-00805f9b34fb")

suspend fun provision(
    gatt: BluetoothGatt,
    pin: String,
    ssid: String, password: String,
    wsUrl: String, deviceToken: String
) {
    val service = gatt.getService(SERVICE_UUID)

    // 1. Authenticate with PIN
    writeCharacteristic(service, CHAR_AUTH, pin.toByteArray())
    delay(200) // Wait for Level 1

    // 2. Write config
    writeCharacteristic(service, CHAR_SSID, ssid.toByteArray())
    writeCharacteristic(service, CHAR_PASSWORD, password.toByteArray())
    writeCharacteristic(service, CHAR_WS_URL, wsUrl.toByteArray())
    writeCharacteristic(service, CHAR_DEV_TOKEN, deviceToken.toByteArray())

    // 3. Restart
    writeCharacteristic(service, CHAR_COMMAND, byteArrayOf(0x01))
}
```

### iOS (Swift)

```swift
import CoreBluetooth

class LuniProvisioner: NSObject, CBPeripheralDelegate {
    let serviceUUID = CBUUID(string: "FF01")
    let authChar    = CBUUID(string: "0010")
    let ssidChar    = CBUUID(string: "0001")
    let passChar    = CBUUID(string: "0002")
    let wsUrlChar   = CBUUID(string: "0003")
    let tokenChar   = CBUUID(string: "0004")
    let cmdChar     = CBUUID(string: "0011")

    func provision(
        peripheral: CBPeripheral, pin: String,
        ssid: String, password: String,
        wsUrl: String, deviceToken: String
    ) {
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }),
              let chars = service.characteristics else { return }

        func chr(_ uuid: CBUUID) -> CBCharacteristic? {
            chars.first { $0.uuid == uuid }
        }

        // 1. PIN auth
        if let c = chr(authChar) {
            peripheral.writeValue(pin.data(using: .utf8)!, for: c, type: .withResponse)
        }

        DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) {
            // 2. Write config
            if let c = chr(self.ssidChar) {
                peripheral.writeValue(ssid.data(using: .utf8)!, for: c, type: .withResponse)
            }
            if let c = chr(self.passChar) {
                peripheral.writeValue(password.data(using: .utf8)!, for: c, type: .withResponse)
            }
            if let c = chr(self.wsUrlChar) {
                peripheral.writeValue(wsUrl.data(using: .utf8)!, for: c, type: .withResponse)
            }
            if let c = chr(self.tokenChar) {
                peripheral.writeValue(deviceToken.data(using: .utf8)!, for: c, type: .withResponse)
            }

            // 3. Restart
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                if let c = chr(self.cmdChar) {
                    peripheral.writeValue(Data([0x01]), for: c, type: .withResponse)
                }
            }
        }
    }
}
```

### Flutter (Dart)

```dart
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class LuniProvisioner {
  static const serviceUuid = Guid("0000FF01-0000-1000-8000-00805f9b34fb");
  static const authUuid    = Guid("00000010-0000-1000-8000-00805f9b34fb");
  static const ssidUuid    = Guid("00000001-0000-1000-8000-00805f9b34fb");
  static const passUuid    = Guid("00000002-0000-1000-8000-00805f9b34fb");
  static const wsUrlUuid   = Guid("00000003-0000-1000-8000-00805f9b34fb");
  static const tokenUuid   = Guid("00000004-0000-1000-8000-00805f9b34fb");
  static const cmdUuid     = Guid("00000011-0000-1000-8000-00805f9b34fb");

  Future<void> provision(
    BluetoothDevice device, {
    required String pin,
    required String ssid,
    required String password,
    required String wsUrl,
    required String deviceToken,
  }) async {
    final services = await device.discoverServices();
    final service = services.firstWhere((s) => s.uuid == serviceUuid);

    BluetoothCharacteristic chr(Guid uuid) =>
        service.characteristics.firstWhere((c) => c.uuid == uuid);

    // 1. PIN auth
    await chr(authUuid).write(pin.codeUnits);
    await Future.delayed(Duration(milliseconds: 200));

    // 2. Write config
    await chr(ssidUuid).write(ssid.codeUnits);
    await chr(passUuid).write(password.codeUnits);
    await chr(wsUrlUuid).write(wsUrl.codeUnits);
    await chr(tokenUuid).write(deviceToken.codeUnits);

    // 3. Restart
    await chr(cmdUuid).write([0x01]);
  }
}
```

---

## 7. URL Formats

### WebSocket URL

```
ws://hostname:port/ws
wss://secure.server.com/ws
```

---

## 8. Error Handling

| Error | Cause | Solution |
|-------|-------|----------|
| Connection timeout | Device out of range | Move closer |
| Service not found | Wrong device | Verify device name |
| Write returns 0x01 (FAIL) | Wrong PIN or bad data | Re-enter PIN |
| Write returns 0x02 (UNAUTHORIZED) | Access level too low | Authenticate first |
| Disconnected after restart | Expected behavior | Wait & reconnect if needed |

### Validation

- **PIN:** Exactly 6 digits
- **SSID:** Max 32 chars, non-empty
- **Password:** Max 64 chars (empty = open network)
- **WS URL:** Must be `ws://` or `wss://` URL
- **Device Token:** Non-empty string (sensitive)

---

## 9. NVS Keys

Cac NVS keys luu tru boi firmware:

| NVS Key        | Source Characteristic | Description |
|----------------|---------------------|-------------|
| `device_name`  | DEVICE_INFO         | Device display name |
| `ssid`         | SSID (0x0001)       | WiFi SSID |
| `pass`         | PASSWORD (0x0002)   | WiFi password |
| `ws_url`       | WS_URL (0x0003)     | WebSocket server URL |
| `device_token` | DEV_TOKEN (0x0004)  | Authentication token |
| `user_id`      | USER_ID (0x0005)    | User namespace |
| `admin_secret` | ADMIN_SECRET (0x0014)| Admin HMAC key |

**FACTORY_RESET** xoa: `ssid`, `pass`, `device_token`, `admin_secret`
**FULL_WIPE** xoa: toan bo NVS partition

---

## 10. Testing Checklist

- [ ] Scan tim duoc Luni devices
- [ ] Doc duoc DEVICE_INFO va DIAG_INFO (Level 0)
- [ ] PIN auth hoat dong (Level 0 -> Level 1)
- [ ] Write SSID, PASSWORD, WS_URL, DEV_TOKEN (Level 1)
- [ ] RESTART command hoat dong (Level 1)
- [ ] Device khoi dong lai va ket noi WiFi + WebSocket
- [ ] Admin auth hoat dong voi HMAC-SHA256 (Level 1 -> Level 2)
- [ ] FACTORY_RESET xoa dung cac NVS keys (Level 2)

---

BLE Protocol Version: 2.0
