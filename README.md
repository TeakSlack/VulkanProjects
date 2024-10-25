# Vulkan Projects

## Building

### Requirements

To build this project, users will need Python <=3.3 installed. Windows users will need Visual Studio 2022 (v143) and Linux users will need `g++` and `make`. Additionally, users will need `premake5` and the Vulkan SDK, however if these aren't installed the setup scripts will install them for you.

To setup the platform-specific projects for Vulkan Projects, navigate to the `scripts` directory and run either `Setup-Win.bat` on Windows or `Setup-Linux.sh` on Linux distros. This will install all of the necessary packages and modules to generate the required projects for either Windows or Linux. Either a Visual Studio 2022 solution or Makefile will then be present when the setup is finished. After the setup has been ran once, users can then use either `GenProjects-Win.bat` or `GenProjects-Linux.sh` to regenerate the projects.

For Windows starting in the project's base directory:

```bash
cd scripts
Setup-Win.bat
```

For Linux starting in the project's base directory:

```bash
cd scripts
./Setup-Linux.sh
```

From here, either Visual Studio 2022 or `make` can be used to generate project binaries which can then be ran on their host operating system. 

**Important**: The `pip` installation module of the script may not work on Linux. The user can choose to either not install the necessary packages, but will be required to use their system's package manager to install the required packages. For example on Debian based distros, the following may be used to install `tqdm` and `requests`, although only `requests` is required for the setup:

```bash
sudo apt install python3-requests
sudo apt install python3-tqdm
```

Additionally, if the Vulkan SDK is not already present on the system, the setup script will prompt you to install it. On Linux systems, the Vulkan SDK is installed to the `./vendor/VulkanSDK` directory but is not sourced into the shell's `PATH`. It is recommended to relocate the Vulkan SDK to a safer directory and have the Vulkan SDK be sourced into the shells `PATH`. Users should consult the [Getting Started with the Linux Tarball Vulkan SDK](https://vulkan.lunarg.com/doc/view/1.3.296.0/linux/getting_started.html) guide provided by the Khronos Group. Specifically, the *"Copying SDK Files to System Directories"* and accompanying sections may prove useful.

## Running

While the integrated setup scripts provide everything necessary to build the project for a user's system, it does not include everything required to run the output executables. 

For Windows users, they should consult their system GPU vendor's website for appropriate driver downloads. The process is rather streamlined and shouldn't take long.

For Linux users, the same steps should be followed although it may be more in-depth due to complexities with proprietary drivers not being made publicly available. 