# Vulkan Projects

A collection of projects based upon the Khronos Group's Vulkan API. This guide covers the setup, building, and troubleshooting steps for both Windows and Linux.
## Requirements
* **Python:** Version 3.3 or higher
* **Windows:** Visual Studio 2022 (v143)
* **Linux:** `g++` and `make`
* **Other Tools:** `premake5` and the Vulkan SDK (auto-installed by setup scripts if missing)
## Installation
To set up the project environment, run the appropriate setup script for your operating system:

* **Windows:**
```bash
cd scripts
.\Setup-Win.bat
```

* **Linux:**
```bash
cd scripts
./Setup-Linux.sh
```

These scripts will install required dependencies and generate project files (Visual Studio solution on Windows or Makefile on Linux). For regenerating project files later, use:

- **Windows**: `GenProjects-Win.bat`
- **Linux**: `GenProjects-Linux.sh`
## Building the Project

After setup, build the project as follows:
- **Windows**: Open the solution file in Visual Studio 2022 and build.
- **Linux**: Run `make` in the project directory.
## Running the Executable

Once built, ensure that your system has the correct GPU drivers:

- **Windows**: Download and install the latest drivers from your GPU vendor.
- **Linux**: Follow distribution-specific methods to install proprietary drivers if needed.

## Troubleshooting

### Linux Dependency Installation

Some Python dependencies (`tqdm` and `requests`) may require manual installation:

```bash
sudo apt install python3-requests python3-tqdm
```
### Vulkan SDK Setup on Linux

If not already installed, the setup script will prompt to download the Vulkan SDK, placing it in `./vendor/VulkanSDK`. This directory is not automatically added to `PATH`. For best practices, refer to the [Khronos Group Linux setup guide](https://vulkan.lunarg.com/doc/view/1.3.296.0/linux/getting_started.html) for detailed instructions.
## License

This project is licensed under the MIT License. See the LICENSE file for details.