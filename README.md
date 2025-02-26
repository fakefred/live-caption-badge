# Live Caption Badge

The Live Caption Badge is my team's capstone project for EECS 473, from
September to December 2024. It is a device that transmits and transcribes
speech in a noisy environment in real-time for the hard of hearing.

For technical details, please read [our report](report.pdf) (10.6 MiB).

## Technical Highlights

- ESP32 with ESP-IDF and ESP-ADF
- FreeRTOS
- Audio streaming over HTTP
- Peer discovery over BLE
- Real-time speech-to-text with Vosk
- E-Paper over SPI
- Audio over I2S

## Files

- `firmware` is the firmware our product actually ran on Design Expo.
- `audio`, `BLEnWifi`, `echo`, `epaper` contain the code we used to test
  various components.
- `pcb` is our PCB design. It is a 4-layer board.
- `transcribe` contains the code for the central server `server_esp.py`,
  which ran on a laptop on Design Expo. It is responsible for transcribing
  text and relaying audio. We also used `hack.py` to apply hotfixes to the
  badges.
