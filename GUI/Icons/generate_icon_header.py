
import os
import sys
from pathlib import Path

print("Generating icon header")

assert len(sys.argv) > 1

file_path = Path(sys.argv[1])
	
file_bytes = []

if not file_path.is_file():
    raise Exception(f'file could not be read - {file_path}')

with open(file_path, 'rb') as file:
    file_bytes = file.read()
    file.close()

header = f"#pragma once\n"
header += f"#include <stdint.h>\n\n"
header += f"constexpr uint8_t {file_path.stem.replace('-','_')}[{str(len(file_bytes))}] =\n"
header += f"{{"

for i, byte in enumerate(file_bytes):
    if (i % 20) == 0:
        header += '\n\t'
    header += "0x" + str(hex(int(byte / 16))[2:]) + str(hex(byte % 16)[2:]) + ", "

header += f"}};"

output_path = str(file_path.stem) + ".h"

with open(output_path, 'w') as file:
    file.write(header)
    file.close()
		
print("Icon header generation complete")