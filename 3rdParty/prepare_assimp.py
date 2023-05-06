import sys
from pathlib import Path
import shutil

#This script overwrites and copies files necessary into the assimp src folder before building assimp in order to inject the AssimpImportHook

print("Injecting the assimp importer hook...")

importer_dir = Path(sys.argv[1]).resolve()
assimp_dir = Path(sys.argv[2]).resolve()

shutil.copytree(importer_dir, str(assimp_dir) + "/code/AssetLib/" + str(importer_dir.name), dirs_exist_ok=True)

shutil.copyfile(str(importer_dir) + "/code_lists.txt", str(assimp_dir) + "/code/CmakeLists.txt")

shutil.copyfile(str(importer_dir) + "/importer_registry.txt", str(assimp_dir) + "/code/Common/ImporterRegistry.cpp")

print("Injection complete")
