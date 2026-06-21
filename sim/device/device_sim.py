"""
Luni V2 - DLunice Simulator
=============================
Simulates the ESP32-C5 audio dLunice using PC microphone and speakers.
Connects to the test server via WebSocket.

Controls:
    SPACE  = Push-to-talk (hold to record, release to stop)
    Q      = Quit

Usage:
    python dLunice_sim.py [--server ws://localhost:8000/ws] [--codec opus|adpcm]
"""

import asyncio
import argparse
import struct
import sys
import os
import threading
import time
import json

import numpy as np
import sounddLunice as sd
import websockets

# Try opus
try:
    import opuslib
    HAS_OPUS = True
except (ImportError, Exception):
    HAS_OPUS = False

SAMPLE_RATE = 16000
CHANNELS = 1
BLOCK_SIZE = 960  # 60ms @ 16kHz for Opus (matches firmware AudioConfig), 256 for ADPCM
OPUS_BITRATE = 24000  # production device contract: 16k VOIP 60ms 24kbps
# Uplink batch tag the C5 prepends to every audio WS frame (WsProtocol 0xAA);
# the cloud strips it in handle_device_message before buffering.
AUDIO_UPLINK_TAG = 0xAA

# ============================================================================
# ADPCM Codec (same as server)
# ============================================================================
ADPCM_INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
]
ADPCM_STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]


class AdpcmEncoder:
    def __init__(self):
        self.predictor = 0
        self.index = 0

    def reset(self):
        self.predictor = 0
        self.index = 0

    def encode(self, pcm: np.ndarray) -> bytes:
        encoded = bytearray()
        nibble_buf = 0
        nibble_count = 0

        for sample in pcm:
            sample = int(sample)
            step = ADPCM_STEP_TABLE[self.index]
            diff = sample - self.predictor
            nibble = 0
            if diff < 0:
                nibble = 8
                diff = -diff
            if diff >= step: nibble |= 4; diff -= step
            if diff >= (step >> 1): nibble |= 2; diff -= (step >> 1)
            if diff >= (step >> 2): nibble |= 1

            recon_diff = step >> 3
            if nibble & 4: recon_diff += step
            if nibble & 2: recon_diff += step >> 1
            if nibble & 1: recon_diff += step >> 2
            if nibble & 8: self.predictor -= recon_diff
            else: self.predictor += recon_diff
            self.predictor = max(-32768, min(32767, self.predictor))
            self.index += ADPCM_INDEX_TABLE[nibble]
            self.index = max(0, min(88, self.index))

            if nibble_count == 0:
                nibble_buf = nibble
                nibble_count = 1
            else:
                nibble_buf |= (nibble << 4)
                encoded.append(nibble_buf)
                nibble_count = 0

        return bytes(encoded)


class AdpcmDecoder:
    def __init__(self):
        self.predictor = 0
        self.index = 0

    def reset(self):
        self.predictor = 0
        self.index = 0

    def decode(self, data: bytes) -> np.ndarray:
        samples = []
        for byte in data:
            for nibble_idx in range(2):
                nibble = (byte & 0x0F) if nibble_idx == 0 else ((byte >> 4) & 0x0F)
                step = ADPCM_STEP_TABLE[self.index]
                diff = step >> 3
                if nibble & 4: diff += step
                if nibble & 2: diff += step >> 1
                if nibble & 1: diff += step >> 2
                if nibble & 8: self.predictor -= diff
                else: self.predictor += diff
                self.predictor = max(-32768, min(32767, self.predictor))
                self.index += ADPCM_INDEX_TABLE[nibble]
                self.index = max(0, min(88, self.index))
                samples.append(self.predictor)
        return np.array(samples, dtype=np.int16)


# ============================================================================
# DLunice Simulator
# ============================================================================

class DLuniceSimulator:
    def __init__(self, server_url: str, codec_type: str,
                 device_token: str = "", mac: str = "SIMDEV000001"):
        self.server_url = server_url
        self.codec_type = codec_type
        # Production WS auth identity (see WsProtocol.cpp / ws/device.py).
        # device_token is sim-only; supply via --device-token or env.
        self.device_token = device_token
        self.mac = mac
        self.ws = None
        self.recording = False
        self.playing = False
        self.running = True
        self.audio_queue = asyncio.Queue()
        self.play_buffer = []

        # Codec setup
        if codec_type == 'opus' and HAS_OPUS:
            self.encoder = opuslib.Encoder(SAMPLE_RATE, CHANNELS, opuslib.APPLICATION_VOIP)
            try:
                self.encoder.bitrate = OPUS_BITRATE
            except Exception:
                pass
            self.decoder = opuslib.Decoder(SAMPLE_RATE, CHANNELS)
            # 60ms @ 16kHz = 960 samples — the production contract (was 320/20ms,
            # which never matched the firmware or the cloud — S15-05).
            self.frame_size = 960
        else:
            if codec_type == 'opus' and not HAS_OPUS:
                print("[WARN] opuslib not available, falling back to ADPCM")
                self.codec_type = 'adpcm'
            self.encoder = AdpcmEncoder()
            self.decoder = AdpcmDecoder()
            self.frame_size = 256

    def audio_callback(self, indata, frames, time_info, status):
        """SounddLunice input callback - runs in audio thread."""
        if self.recording:
            pcm = (indata[:, 0] * 32767).astype(np.int16)
            # Process in frame_size chunks
            for i in range(0, len(pcm) - self.frame_size + 1, self.frame_size):
                chunk = pcm[i:i + self.frame_size]
                if self.codec_type == 'opus':
                    pcm_bytes = chunk.tobytes()
                    opus_data = self.encoder.encode(pcm_bytes, self.frame_size)
                    header = struct.pack('<H', len(opus_data))
                    encoded = header + opus_data
                else:
                    encoded = self.encoder.encode(chunk)

                try:
                    self.audio_queue.put_nowait(encoded)
                except asyncio.QueueFull:
                    pass

    def play_audio(self, encoded_data: bytes):
        """Decode one or more [len:2 LE][opus] frames and queue for playback.

        The cloud paces audio in batches of several frames per binary WS
        message (ws_manager._send_audio_local), so a downlink message can carry
        multiple concatenated [len][opus] units — decode them all.
        """
        if self.codec_type == 'opus' and HAS_OPUS:
            off, n = 0, len(encoded_data)
            while off + 2 <= n:
                frame_len = struct.unpack('<H', encoded_data[off:off + 2])[0]
                off += 2
                if frame_len == 0 or off + frame_len > n:
                    break
                opus_data = encoded_data[off:off + frame_len]
                off += frame_len
                pcm_bytes = self.decoder.decode(opus_data, self.frame_size)
                pcm = np.frombuffer(pcm_bytes, dtype=np.int16)
                self.play_buffer.extend(pcm.tolist())
        else:
            pcm = self.decoder.decode(encoded_data)
            self.play_buffer.extend(pcm.tolist())

    async def send_loop(self):
        """Send encoded audio to server."""
        while self.running:
            try:
                data = await asyncio.wait_for(self.audio_queue.get(), timeout=0.1)
                if self.ws and self.recording:
                    # Prepend the 0xAA uplink batch tag the C5 adds (cloud strips
                    # it). Opus payload stays [len:2 LE][opus]. (S15-05)
                    if self.codec_type == 'opus':
                        await self.ws.send(bytes([AUDIO_UPLINK_TAG]) + data)
                    else:
                        await self.ws.send(data)
            except asyncio.TimeoutError:
                continue
            except Exception as e:
                print(f"[SEND ERROR] {e}")
                break

    async def playback_loop(self):
        """Play received audio through speakers."""
        while self.running:
            if self.play_buffer and len(self.play_buffer) >= self.frame_size:
                chunk_size = min(len(self.play_buffer), SAMPLE_RATE // 10)  # 100ms chunks
                chunk = np.array(self.play_buffer[:chunk_size], dtype=np.int16)
                self.play_buffer = self.play_buffer[chunk_size:]

                # Play through speakers
                float_data = chunk.astype(np.float32) / 32768.0
                sd.play(float_data, SAMPLE_RATE, blocking=True)
            else:
                await asyncio.sleep(0.01)

    async def run(self):
        print("=" * 50)
        print("  Luni V2 - DLunice Simulator")
        print("=" * 50)
        print(f"  Server:  {self.server_url}")
        print(f"  Codec:   {self.codec_type}")
        print(f"  Input:   Default microphone")
        print(f"  Output:  Default speakers")
        print("=" * 50)
        print("\nControls:")
        print("  SPACE = Push-to-talk (press to start, press again to stop)")
        print("  Q     = Quit\n")

        try:
            async with websockets.connect(self.server_url, ping_interval=20) as ws:
                self.ws = ws
                print("[CONNECTED] WebSocket connected")

                # Production auth handshake (matches firmware createAuthMessage
                # and the server's _authenticate_device in ws/device.py).
                handshake = json.dumps({
                    "type": "auth",
                    "id": str(int(time.time() * 1000)),
                    "ts": int(time.time() * 1000),
                    "payload": {
                        "device_token": self.device_token,
                        "mac": self.mac,
                        "fw_version": "2.0.0-sim",
                        "model": "Luni-C5",
                    },
                })
                await ws.send(handshake)
                print(f"[AUTH] Sent auth (mac={self.mac}, "
                      f"token={'set' if self.device_token else 'EMPTY'})")

                # Start audio input stream
                stream = sd.InputStream(
                    samplerate=SAMPLE_RATE,
                    channels=CHANNELS,
                    dtype='float32',
                    blocksize=self.frame_size,
                    callback=self.audio_callback
                )
                stream.start()

                # Start send and playback tasks
                send_task = asyncio.create_task(self.send_loop())
                play_task = asyncio.create_task(self.playback_loop())

                # Start keyboard listener in thread
                kb_thread = threading.Thread(target=self.keyboard_listener, daemon=True)
                kb_thread.start()

                # Receive loop
                try:
                    async for message in ws:
                        if isinstance(message, str):
                            # Production downlink is JSON (auth_result, set_emotion,
                            # tts_play, audio_stop, …) — NOT the legacy toy text
                            # SPEAKING/IDLE/DONE the old sim listened for (S15-05).
                            try:
                                evt = json.loads(message)
                            except json.JSONDecodeError:
                                print(f"  [SERVER?] {message}")
                                continue
                            etype = evt.get("type")
                            payload = evt.get("payload", {})
                            if etype == "auth_result":
                                status = payload.get("status")
                                print(f"  [AUTH] {status} {payload}")
                            elif etype == "audio_stop":
                                # C5 owns the turn: cloud closes PROCESSING here
                                # (END uplink → wait for audio → audio_stop).
                                print("  [TURN] audio_stop — turn closed")
                                self.playing = False
                            else:
                                print(f"  [EVENT] {etype}: {payload}")
                        elif isinstance(message, bytes):
                            # Binary downlink = paced [len:2 LE][opus] TTS audio.
                            if not self.playing:
                                self.playing = True
                                self.play_buffer.clear()
                                if hasattr(self.decoder, 'reset'):
                                    self.decoder.reset()
                            self.play_audio(message)
                except websockets.exceptions.ConnectionClosed:
                    print("[DISCONNECTED]")

                stream.stop()
                self.running = False
                send_task.cancel()
                play_task.cancel()

        except Exception as e:
            print(f"[ERROR] {e}")

    def keyboard_listener(self):
        """Listen for keyboard input (runs in separate thread)."""
        try:
            if sys.platform == 'win32':
                import msvcrt
                while self.running:
                    if msvcrt.kbhit():
                        key = msvcrt.getch()
                        if key == b' ':
                            self.toggle_recording()
                        elif key in (b'q', b'Q'):
                            self.running = False
                            break
                    time.sleep(0.05)
            else:
                import termios
                import tty
                fd = sys.stdin.fileno()
                old_settings = termios.tcgetattr(fd)
                try:
                    tty.setraw(fd)
                    while self.running:
                        ch = sys.stdin.read(1)
                        if ch == ' ':
                            self.toggle_recording()
                        elif ch in ('q', 'Q'):
                            self.running = False
                            break
                finally:
                    termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        except Exception:
            # Fallback: simple input
            while self.running:
                try:
                    cmd = input()
                    if cmd.strip() == '':
                        self.toggle_recording()
                    elif cmd.strip().lower() == 'q':
                        self.running = False
                        break
                except EOFError:
                    break

    def toggle_recording(self):
        if not self.recording:
            self.recording = True
            # Reset encoder
            if hasattr(self.encoder, 'reset'):
                self.encoder.reset()
            print("\n  [MIC] Recording... (press SPACE to stop)")
            # Send START
            asyncio.get_event_loop().call_soon_threadsafe(
                lambda: asyncio.ensure_future(self.ws.send("START")) if self.ws else None
            )
        else:
            self.recording = False
            print("  [MIC] Stopped recording")
            asyncio.get_event_loop().call_soon_threadsafe(
                lambda: asyncio.ensure_future(self.ws.send("END")) if self.ws else None
            )


async def main(args):
    sim = DLuniceSimulator(args.server, args.codec,
                           device_token=args.device_token, mac=args.mac)
    await sim.run()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Luni V2 DLunice Simulator")
    parser.add_argument("--server", type=str, default="ws://localhost:8000/ws/device",
                        help="Server WebSocket URL")
    parser.add_argument("--codec", choices=["opus", "adpcm"], default="opus",
                        help="Audio codec (default: opus — the production cloud "
                             "only speaks Opus; adpcm is legacy/toy-server only)")
    parser.add_argument("--device-token", type=str,
                        default=os.environ.get("LUNI_DEVICE_TOKEN", ""),
                        help="Device token for WS auth (or env LUNI_DEVICE_TOKEN)")
    parser.add_argument("--mac", type=str,
                        default=os.environ.get("LUNI_DEVICE_MAC", "SIMDEV000001"),
                        help="Device MAC / id, colon-less (or env LUNI_DEVICE_MAC)")
    args = parser.parse_args()
    asyncio.run(main(args))
