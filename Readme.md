# IntelliShell

IntelliShell is an AI-powered interactive terminal shell written in C++. It integrates AI suggestions to help users run commands even if they don't know the exact syntax. Simply type natural language instructions, and IntelliShell suggests and executes the corresponding shell commands with safety checks.

---

## Features

* **AI-driven command suggestions** from natural language input
* **Safety checks** for potentially dangerous commands
* **Interactive prompt** showing `user@machine` and current working directory
* **Command history** with `readline` support (up-arrow navigation)
* **Option to edit** AI-suggested commands before execution
* **Clean, colorful terminal output** for easy reading
* **Adapter architecture** to integrate any AI backend (a default Python adapter is included)

---

## Requirements

* C++17 compatible compiler (e.g., `g++`)
* `readline` library
* `nlohmann/json` header-only library
* Python 3 with dependencies for the AI adapter (see `requirements.txt`)

---

## Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/lucif3r17/IntelliShell.git
    cd IntelliShell
    ```
2.  **Ensure you have dependencies installed:**

    On Debian/Ubuntu:
    ```bash
    sudo apt install g++ libreadline-dev
    ```
3.  **Download `json.hpp`** using
    ```bash
    sudo apt install nlohmann-json3-dev
    ```
4.  **Build the shell:**
    ```bash
    make
    ```
5.  **Set up the Python environment** for the adapter:
    ```bash
    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt
    ```

---

## Usage

1.  **Run the shell executable:**
    ```bash
    ./ai_shell ./ai_adapter.py
    ```
2.  **Type commands in natural language,** e.g.,
    ```bash
    ls all files
    ```
    The AI adapter will suggest the actual shell command to run. You can then choose to run, edit, or cancel it.
3.  **Type `exit` or `quit`** to leave.
