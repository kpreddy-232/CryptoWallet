# CryptoWallet - Trading Simulator & Portfolio Manager


CryptoWallet is a fully functional, highly optimized Cryptocurrency Trading Simulator built with a **C++ backend** and a **Vanilla JS/HTML/Tailwind frontend**. It allows users to create secure accounts, execute mock trades using real-time market data, and track their overall portfolio performance.

---

## Key Features

* **Real-Time Data Integration:** Uses C++ `curl` and `_popen` to interface with the CoinGecko REST API for live crypto prices.
* **In-Memory Caching:** Implements `std::unordered_map` and thread-safe `std::mutex` caching to minimize external API calls and prevent rate-limiting.
* **C-API SQLite Database:** Features a fully relational database schema (Users, Assets, Transactions, Watchlist) enforcing data integrity via Foreign Keys.
* **Secure Authentication:** Utilizes `picosha2` (SHA-256) for secure password hashing and token-based session mapping.
* **Hindsight Simulator:** A "DCA/Time-machine" calculator that computes theoretical returns using historical API chart data.
* **Data-Dense Minimalist UI:** A professional, brutalist financial-terminal design powered by Tailwind CSS and Chart.js.

## Tech Stack

* **Backend:** C++17, Crow (C++ Microframework), SQLite3 (Native C-API), PicoSHA2.
* **Frontend:** HTML5, Tailwind CSS, Vanilla JavaScript, Chart.js.

---

## How to Run Locally (Windows)

If you want to run this server locally on your laptop, follow these steps:

### 1. Prerequisites
Ensure you have the following installed on your machine:
* **C++ Compiler (MinGW or MSVC)** supporting C++17 or higher.
* **CMake** (v3.15+).
* **VS Code** with the C/C++ and CMake extensions installed.

### 2. Frontend Configuration
Before starting the server, ensure the frontend is pointing to your local machine:
1. Open `frontend/index.html`.
2. Locate the API constant at the bottom of the file (around line 430).
3. Ensure it is set to local host:
   ```javascript
   const API_BASE = "[http://127.0.0.1:8080/api](http://127.0.0.1:8080/api)";
3. Build the Backend
Open the project folder in VS Code.

Open a new Terminal in VS Code.

Run the following CMake commands to compile the server:

# Terminal:

    .\Debug\wallet_server.exe
    mkdir build
    cd buils
    cmake ..
    cmake --build .
  

4. Run the Application
Start the compiled executable from the build folder:


# Terminal:

    .\Debug\wallet_server.exe

OR (depending on your compiler setup):

    .\wallet_server.exe


Wait for the terminal to display: Starting Server on port 8080...

Simply double-click the frontend/index.html file to open it in your web browser.

The app is now fully functional and running entirely on your local machine!
