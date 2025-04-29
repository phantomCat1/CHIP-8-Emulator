# CHIP-8-Emulator
This is an emulator for the CHIP8 interpreted language.
To understand the workings of the emulator, as well as how the keypad looks, you can check the following resources:
https://en.wikipedia.org/wiki/CHIP-8 

http://devernay.free.fr/hacks/chip8/C8TECH10.HTM

The keypad is mapped to the exact keys on the keyboard.

To get other CHIP8 roms, you can check out the following repository, containing various programs and full on games:
https://github.com/kripod/chip8-roms.git

---

To try out the emulator, follow these steps:

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/phantomCat1/CHIP-8-Emulator.git
   ```


2. **Build the Project**:
   ```bash
   make
   ```

3. **Start the emulator**:
   ```bash
   ./chip8 -p path/to/chip8/file
   ```
4. **Use of Flags**
    ```
    -p - used to specify the path to the file containing the CHIP8 instructions
    -w - used to specify the width of the display window in number of cells (default is 64)
    -h - used to specify the height of the display window in number of cells (default is 32)
    -sf - used to specify the scale factor of each cell (default is 20)
    ```
5. **Useful keys**:
   - To stop the emulator gracefully, press `esc`.
   - To pause the emulator, press the spacebar.
   - To increase the audio, press `p`.
   - To decrease the sound, press `o`.
   - To restart the program being emulated, press `=`.
