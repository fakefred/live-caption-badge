from flask import Flask, request
from vosk import Model, KaldiRecognizer
import json
import sounddevice as sd

app = Flask(__name__)
model = Model(lang="en-us")
device_info = sd.query_devices(None, "input")
rec = KaldiRecognizer(model, int(device_info["default_samplerate"]))

@app.route('/stream', methods=['POST'])
def stream_audio():
    data = request.data  # This is the raw audio data
    # Process the data as needed
    if rec.AcceptWaveform(data):
        print(rec.Result())
    else:
        print(rec.PartialResult())
    print("Received audio data")
    return "Audio received", 200

if __name__ == "__main__":
    app.run(host="localhost", port=8080)
