import os
import sys
import platform
import subprocess

from SetupPython import SetupPython
from SetupPremake import SetupPremake
from SetupVulkan import SetupVulkan

# Change directory to project root
os.chdir('../')

SetupPython.Validate()
hasPremake = SetupPremake.Validate()
SetupVulkan.Validate()

if not hasPremake:
    print("Premake not installed.")
    sys.exit(1)

print("Running premake...")

if platform.system() == "Windows":
    subprocess.call([os.path.abspath('./vendor/premake/bin/premake5.exe'), 'vs2022'])
elif platform.system() == "Linux":
    subprocess.call([os.path.abspath('./vendor/premake/bin/premake5'), 'gmake2'])