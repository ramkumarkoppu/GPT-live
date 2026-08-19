import asyncio
import websockets
import wave

WS_HOST = "0.0.0.0"   # local mosquitto on this Pi; ESP32 connects to 192.168.1.123
WS_PORT = 12345

SAMPLE_RATE = 48000
CHANNELS = 2
SAMPLE_WIDTH = 4 # 32-bit = 4 bytes
BYTES_PER_SEC = SAMPLE_RATE * CHANNELS * SAMPLE_WIDTH   # 384000

async def handle_connection(websocket):
    print(f"Client connected: {websocket.remote_address}")
    audio_data = bytearray()

    async for message in websocket:
        if isinstance(message, (bytes, bytearray)):
            audio_data.extend(message)
            if (len(audio_data) % 64000) < len(message):
                print(f"Received {len(audio_data)} bytes ({len(audio_data) / BYTES_PER_SEC:.1f} seconds)")
        else:
            print(f"Received text: {message}")
    print(f"Client disconnected. Total received: {len(audio_data)} bytes")

    if len(audio_data) > 0:
        audio_filename = "output.wav"
        print(f"Saving to {audio_filename}...")
        with wave.open(audio_filename, "wb") as wav_file:
            wav_file.setnchannels(CHANNELS)
            wav_file.setsampwidth(SAMPLE_WIDTH)
            wav_file.setframerate(SAMPLE_RATE)
            wav_file.writeframes(bytes(audio_data))
        print(f"Done! Audio saved to {audio_filename}")
    else:
        print("No data received!")

async def main():
    print(f"WebSocket server listening on {WS_HOST}:{WS_PORT}...")
    async with websockets.serve(handle_connection, WS_HOST, WS_PORT):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())

