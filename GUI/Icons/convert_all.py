import os
from pathlib import Path
from PIL import Image

current_directory = os.getcwd()
file_list = os.listdir(current_directory)
png_files = [file for file in file_list if file.endswith(".png")]

for png_file in png_files:
	print(png_file)
	
	image = Image.open(png_file)	
	rgba_image = image.convert("RGBA")
	image_bytes = bytearray(rgba_image.tobytes())
	
	image_bytes.insert(0, rgba_image.height)
	image_bytes.insert(0, rgba_image.width)

	print(png_file)
	header = f"#pragma once\n"
	header += f"#include <stdint.h>\n\n"
	header += f"constexpr uint8_t {Path(png_file).stem.replace('-','_')}[{str(len(image_bytes))}] =\n"
	header += f"{{"
	
	for i, byte in enumerate(image_bytes):
		if (i % 20) == 0:
			header += '\n\t'
		header += "0x" + str(hex(int(byte / 16))[2:]) + str(hex(byte % 16)[2:]) + ", "
	
	header += f"}};"
	
	output_path = str(Path(png_file).stem) + ".h"
	
	with open(output_path, 'w') as file:
		file.write(header)
		file.close()
		
print("Icon header generation complete")