import sys
from pathlib import Path
import os
import subprocess

assert(len(sys.argv) == 4)

viewer_path = Path(sys.argv[1]).absolute()
in_root = Path(sys.argv[2]).absolute()
out_root = Path(sys.argv[3]).absolute()

def find_dae_files(directory):
    png_files = []
    for root, _, files in os.walk(directory):
        for file in files:
            if file.lower().endswith(".dae.phyre"):
                png_files.append(os.path.join(root, file))
    return png_files


def copy_folder_structure(source_dir, destination_dir):
    if not os.path.exists(source_dir):
        print(f"Source directory '{source_dir}' does not exist.")
        return

    if not os.path.exists(destination_dir):
        os.makedirs(destination_dir)

    for item in os.listdir(source_dir):
        source_item = os.path.join(source_dir, item)
        destination_item = os.path.join(destination_dir, item)

        if os.path.isdir(source_item):
            copy_folder_structure(source_item, destination_item)

print("copying folder structure")
#copy_folder_structure(in_root, out_root)

print("finding dae files")
dae_files = find_dae_files(in_root)

i = 0
for dae_file in dae_files:
	print(i, "/", len(dae_files), " ", str(dae_file))
	i += 1
        
	snapshot_folder = out_root / Path(dae_file).relative_to(in_root)
		
	if not os.path.exists(snapshot_folder):
		os.makedirs(snapshot_folder)
		
	snapshot_path = snapshot_folder / "snapshot.png"
	   
	result = subprocess.run([viewer_path, 
	                    "snapshot", dae_file, 
                        "-o", snapshot_path,
	                    "-x", "128",
	        			"-y", "128",
	        			"-c", "16"],
                        capture_output=True, text=True    
						)
	if result != 0:
		print("failed")
		print(result.stdout)
		print(result.stderr)


    


