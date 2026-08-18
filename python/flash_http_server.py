from flask import Flask, request, jsonify
import wave
import datetime
import socket


def get_local_ip():
    # No packets are sent - this just asks the kernel which interface
    # would be used to reach the outside, then reads back its address
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()

app = Flask(__name__)

@app.route("/upload", methods=["POST"])
def upload_audio():
    try:
        # Get audio parameters from headers
        sample_rate = int(request.headers.get("X-Sample-Rate", 16000))
        channels = int(request.headers.get("X-Channels", 2))
        bits_per_sample = int(request.headers.get("X-Bits-Per-Sample", 32))

        # Get raw aduio data
        audio_data = request.get_data()

        print(f"Received {len(audio_data)} bytes")
        print(f"Format: {sample_rate}Hz, {channels} channels, {bits_per_sample}-bit")

        # Generate filename with timestamp
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"recording_{timestamp}.wav"

        # Save as WAV file
        with wave.open(filename, "wb") as wav_file:
            wav_file.setnchannels(channels)
            wav_file.setsampwidth(bits_per_sample // 8) # Convert bits to bytes
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(audio_data)

        duration = len(audio_data) / (sample_rate * channels * (bits_per_sample // 8));
        print(f"Saved to {filename} ({duration: .2f} seconds)")

        return jsonify({
            "status": "success",
            "filename": filename,
            "bytes_received": len(audio_data),
            "duration_seconds": duration
            }), 200

    except Exception as e:
        print(f"Error: {str(e)}")
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route("/", methods=["GET"])
def index():
    return "Audio upload server running"

if __name__ == "__main__":
    print(f"Starting HTTP server on http://{get_local_ip()}:8000 (all interfaces)")
    print(f"ESP32 should POST audio to http://{get_local_ip()}:8000/upload")
    app.run(host="0.0.0.0", port=8000, debug=True)

