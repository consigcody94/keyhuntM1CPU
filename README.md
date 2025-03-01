➡️ Keyhunt M1/M2 CPU 

Keyhunt for macOS (M1/M2) – Apple Silicon Support

🔑 KeyhuntM1CPU is a macOS-optimized fork of the original Keyhunt tool, designed to hunt private keys for cryptocurrencies using the secp256k1 elliptic curve. This version is specifically configured for Apple Silicon (M1/M2), offering native macOS support and improved CPU performance.

🚀 Features

✅ Optimized for macOS  (Apple M1 & M2)
✅ Fixed Makefile for Apple Clang compiler
✅ Compatible with Bitcoin & Ethereum key searches
✅ Supports multiple key hunting modes:
	•	🔹 Address mode (compressed/uncompressed)
	•	🔹 RMD160 mode
	•	🔹 XPoint mode
	•	🔹 BSGS mode (Baby Step Giant Step)
	•	🔹 Minikeys & Vanity Search

🛠 Installation & Setup

1️⃣ Install Dependencies

First, install Homebrew and required libraries:

/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install gcc make openssl@3 gmp

2️⃣ Clone the Repository

git clone https://github.com/consigcody94/keyhuntM1CPU.git
cd keyhuntM1CPU

3️⃣ Compile Keyhunt (macOS M1/M2)

make legacy

If successful, Keyhunt is now ready to run on your Mac.

🖥️ Running Keyhunt on macOS

Basic Usage:

🔹 Puzzle 66 (Address Mode, Random Search)

./keyhunt -m address -f tests/66.txt -b 66 -l compress -R -q -s 10

🔹 Puzzle 125 (BSGS Mode)

./keyhunt -m bsgs -f tests/125.txt -b 125 -q -s 10 -R

🔹 Run with Multiple Threads for Speed Optimization

./keyhunt -m bsgs -f tests/125.txt -b 125 -q -s 10 -t 8

⚡ macOS GPU Acceleration (Experimental)

macOS uses Metal instead of CUDA, so GPU acceleration is still being tested. To check GPU support:

system_profiler SPDisplaysDataType | grep Metal

Further development is needed to fully utilize Metal or OpenCL for acceleration.

📌 Notes & Known Issues
	•	No CUDA Support on macOS – Metal or OpenCL alternatives may be required.
	•	BSGS mode is significantly faster than Address mode.
	•	Compiled and tested on M1 MacBook Air & M2 MacBook Pro

📜 License & Credits

🔗 Original Keyhunt Repository: albertobsd/keyhunt
🔗 Forked & Maintained by: @consigcody94
📝 License: MIT

💡 If you find this project useful, feel free to ⭐ star the repo! 🚀

