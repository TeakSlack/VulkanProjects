import os
import platform
import sys
import subprocess

import FileUtil

class SetupVulkan:
    requiredVulkanVersion = "1.3"
    installVulkanVersion = "1.3.296.0"
    vulkanDirectory = "./vendor/VulkanSDK"
    vulkanUrls = {
        "Windows": f"https://sdk.lunarg.com/sdk/download/{installVulkanVersion}/windows/VulkanSDK-{installVulkanVersion}-Installer.exe",
        "Linux": f"https://sdk.lunarg.com/sdk/download/{installVulkanVersion}/linux/vulkansdk-linux-x86_64-{installVulkanVersion}.tar.xz"
    }

    @classmethod
    def Validate(cls):
        return cls.ValidateVulkan()
    
    @classmethod
    def ValidateVulkan(cls):
        vulkanSdk = os.environ.get('VULKAN_SDK')
        if vulkanSdk is None:
            print("Vulkan SDK is not installed!")
            cls.InstallVulkan()
            return False
        if cls.requiredVulkanVersion not in vulkanSdk:
            print(f"Vulkan SDK is out of date! Required version is <{cls.requiredVulkanVersion}")
            return False
        print(f"Valid Vulkan SDK found at {vulkanSdk}")
        return True
    
    @classmethod
    def InstallVulkan(cls):
        hasPermission = False
        while not hasPermission:
            reply = str(input(f"Would you like to install Vulkan {cls.installVulkanVersion} (this may take a while)? [Y/n]: ")).lower().strip()[:1]
            if reply == 'n':
                return False
            hasPermission = (reply == 'y' or reply == '')

        url = cls.vulkanUrls[platform.system()]
        vulkanPath = f"{cls.vulkanDirectory}/vulkan-{cls.installVulkanVersion}-{platform.system().lower()}"

        if platform.system() == "Windows":
            vulkanPath += ".exe"
        elif platform.system() == "Linux":
            vulkanPath += ".tar.xz"

        FileUtil.DownloadFile(url, vulkanPath)

        if platform.system() == "Windows":
            os.startfile(os.path.abspath(vulkanPath))
            print("Re-run this script after installation.")
            sys.exit(0)
        elif platform.system() == "Linux":
            FileUtil.TarFile(vulkanPath, type='xz')
            cls.AddVulkanPath(vulkanPath)

    @classmethod 
    def AddVulkanPath(cls, vulkanPath): # only supports bash or zsh
        hasPermission = False
        while not hasPermission:
            reply = str(input(f"Would you like to add Vulkan to your shells PATH? [Y/n]: ")).lower().strip()[:1]
            if reply == 'n':
                return False
            hasPermission = (reply == 'y' or reply == '')
            
        basePath = os.path.dirname(vulkanPath)
        basePath = os.path.abspath(basePath)

        shell = os.environ.get('SHELL')
        if 'bash' in shell:
            shellConfig = '.bashrc'
        elif 'zsh' in shell:
            shellConfig = '.zshrc'
        
        shellPath = os.path.expanduser('~/' + shellConfig)
        with open(shellPath, 'a') as f:
            f.write('source ' + basePath + f"/{cls.installVulkanVersion}/setup-env.sh")
            f.close()