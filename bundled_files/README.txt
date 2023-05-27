FFXII Asset Converter version 1.3.1 by Roelin hosted by Nexus

A tool to convert most FFXII models and textures to and from common formats.
With this tool, it can now be possible to create HD character textures and completely reshape any model.

General usage:
	- Input paths may be folders or files.
	- Can convert dae.phyre models to fbx. May be capable of other formats allowed by Assimp, but these have not been tested.
	- Can convert dds.phyre images to png. May be capable of also bmp, jpg, tiff and wmp, but these have not been tested.
	- If folders are input, models within will convert to fbx by default, and likewise images convert to png.

	- The term "original_asset_path" refers to the original, unmodified asset (.dae.phyre, .dds.phyre)
	- The term "replacement_path" refers to the unpacked, editable form of the asset (.fbx, .png, etc.)
	- The term "mod_output_path" refers to the repacked, modified form of the asset (.dae.phyre, .dds.phyre)

	- The program unpacks the original asset to a replacement asset, from which it may be edited via programs like blender. 
	  It then uses the original asset to pack modifications of the replacement asset into a new mod output asset.

	- It's possible for the original_asset_path and/or mod_output_path to index directly into a vbf:
		E.g: (...)/FFXII_TZA.vbf/gamedata/d3d11/artdata/chr/chara/c10/c1004/mws/c1004.dae.phyre
	  In which case, the asset files will be extracted, then injected back into the vbf after modifications are applied.

Command Line Usage:
	-u, --unpack INPUT<original_asset_path> OUTPUT<replacement_path>
	-p, --pack INPUT<original_asset_path> INPUT<replacement_path> OUTPUT<mod_output_path>
	-h, --help
	
	example: FFXIIConvert.exe --unpack folder1/c1004.dae.phyre folder2/c1004.fbx
	example: FFXIIConvert.exe --pack folder1/c1004.dae.phyre folder2/c1004.fbx folder3/new_c1004.dae.phyre

comments:
	This tool improves upon dae.phyre converter predecessors by:
		-Allowing for the modification of bone weights and indexes, as long as the number bones a mesh replacements is <= the amount of the original mesh.
		-Allowing for the deletion of meshes.
		-Scaling the model to the correct, full size.
		-Making sure the uvs aren't inverted.
	This tool improves upon dds.phyre converter predecessors by:
﻿		-Allowing for the resizing of textures
		-Exporting textures directly to and from common formats
		-Inverting and converting the 2-channel tangent space normal maps to standard 3 channel normal maps
			-Be warned that since the program cannot accurately re-pack modifications to 2 channels, if you do make modifications, you'll need to re-bake to the 2 channel version manually

Please let me know of any bugs or issues!

Licenses and Copyright notices may be found in the LICENSES folder