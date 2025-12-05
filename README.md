# ESP32-C3-LCD-0.71 Computer Temps Display

## Description
This small project uses the Waveshare ESP32-C3-LCD-0.71 display connected via USB to display the
current CPU and GPU temperatures of a Windows computer. The Python application retrieves the temperature
data using the WinTmp library and sends it to the ESP32-C3 display for real-time monitoring.

## Requirements
- Windows (WinTmp works only on Windows and requires admin rights)
- Python 3.12+ (Build with this version)
- Recommended packages in `requirements.txt` (e.g., `Pillow`, other GUI/tray libs)
- Arduino IDE (for uploading the sketch to ESP32-C3-LCD-0.71)
- ESP32-C3-LCD-0.71 display from Waveshare

## Installation
1. Clone the repository
    git clone https://github.com/<your>/repo.git

2. Install required Python packages with requirements.txt
    pip install -r requirements.txt
    Make sure you run this in an environment where you have admin rights.

3. In the folder `Computer Temp Display` is where the python script `main.py` located and
    in the folder `esp32c3_code` is the Arduino sketch for the ESP32-C3-LCD-0.71.

3. See Waveshare documentation for the ESP32C3 setup on Arduino IDE
    https://www.waveshare.com/wiki/ESP32-C3-LCD-0.71

4. Upload the provided Arduino sketch to the ESP32-C3-LCD-0.71

5. Connect the ESP32-C3-LCD-0.71 to your computer via USB.

## Usage
1. Run the Python application:
    python main.py

2. The application will ask for the COM port where the ESP32-C3-LCD-0.71 is connected on the first run.

3. The application will start displaying CPU and GPU temperatures on the ESP32-C3-LCD-0.71 display.

4. You can minimize the application to the system tray for continuous monitoring.

5. To exit the application, right-click the system tray icon and select "Exit".

## Notes
- Ensure you have the necessary permissions to access hardware sensors on your system.
- The application may require additional configuration based on your specific hardware setup.
- For any issues or contributions, please refer to the GitHub repository.
- You can compile the Python script to an executable using PyInstaller:
    pyinstaller --onefile --noconsole main.py
- The executable will be found in the `dist` folder after compilation.

## Credits

This project uses the following third\-party libraries:

- `Pillow` — image handling — https://python-pillow.org/ — License: `PIL`/`HPND`/`BSD` (see `LICENSES/Pillow.txt`)
- `winTmp` — Windows temperature access — https://pypi.org/project/winTmp/ — License: `MIT`
- `pystray` — system tray icon — https://pypi.org/project/pystray/ — License: `MIT`

License copies for redistributed packages are included in the `LICENSES/` folder in releases. See each package page for full license details.
