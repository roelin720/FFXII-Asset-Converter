FFXII Asset Converter version 2.0.1 by Roelin hosted by Nexus
	This is a mod development tool to convert FFXII models and textures to and from common formats.
	HD texture formats can be produced, meshes can be completely remodelled and assets can be injected directly to and from the VBF, all with ease and haste.
	A command line and GUI version of the tool are available.

Important Notes:
	YOU WILL NEED MSVC RUNTIME LIBRARIES INSTALLED TO RUN THE GUI. If you don't already have them installed, they can be found at https://aka.ms/vs/17/release/vc_redist.x64.exe
	The command line interface works differently now. please consult the "Command Line Usage" section.
	Unfortunately, models are no longer scaled by 100 (to not be tiny), since doing so currently causes too many issues.
	Cube-maps are not currently supported.

UPDATE 2.0.0:
	(for more information, caveats and explanations, consult the "Details" section below.)
	New conversion features:
		Map models are now supported.
		Textures can now be automatically loaded for a mesh and assigned to it's materials.
		Unlimited bone (vertex group) assignment to meshes.
		The scene graph is now loaded (nodes, their names, hierarchy and transforms), and node transforms are saved.
	New GUI features:
		You can now drag-and-drop between the browser and windows explorer to directly extract/inject VBF files/folders.
		Various file-browser features like "Rename", "Create new folder" and "Trash" have have been added for QOL.
		A 'copy-logs' button has been added, along with a button to disable the view of newly added warning logs.
	And also various bug fixes.

USAGE:
	General:
		Unpacking - The tool converts the original-asset(s) into an intermediary file format, like '.fbx' for a '.dae.phyre' model, and '.png' for a '.dds.phyre' image.
		Packing - The tool uses the original-asset(s) and intermediary-asset(s) in order to save any intermediary modifications into mod-asset(s).
		
		The paths to the asset(s) may be files or folders. If you supply a folder, intermediary asset formats will automatically be assigned ('.fbx' for models, '.png' for textures). 
		The original asset and mod-asset paths may indirectly directly into the VBF.
﻿		E.g:  (...)/FFXII_TZA.vbf/gamedata/d3d11/artdata/chr/chara/c10/c1004/mws/c1004.dae.phyre
		In which case, the asset files will be extracted, then injected back into the vbf after modifications are applied.
		
		The tool might have issues with non '.png' and '.fbx', files, so I suggest avoiding using anything else for now.
	
	Command Line:
		The following commands are available:
﻿		unpack [original asset path] [output path] 
﻿		pack [original asset path] [reference path] [output path]
﻿		help
		
		An optional argument for unpacking '-r' or '--unpack_refs' may be used to automatically unpack textures the model references (always done in the GUI).
		
		example: FFXIIConvert.exe unpack folder1/c1004.dae.phyre folder2/c1004.fbx -r
		example: FFXIIConvert.exe pack folder1/c1004.dae.phyre folder2/c1004.fbx folder3/new_c1004.dae.phyre
	
	GUI:
		Hopefully the usage of the GUI is intuitive. It mostly functions as a wrapper over the command line tool, but has some extended features, most notably in the file browser that provides a seamless transition from the native file browser to inside the VBF. Dragging and dropping is fully supported between the native file browser and this GUI, even to and from inside the VBF. The file browser is accessed by clicking a file or folder icons for selecting asset paths.
		Play Options:
		To hasten the edit-to-testing pipeline further, you can launch the game directly from the GUI after supplying it with the exe directory via the "play" buttons on the right.
		In addition, There's a button to mute the game remotely, because I find the intro song to get grating after so many tests.

DETAILS:
	Textures:
		There is no known upper limit to the size of any given texture, so please test frequently to see if the game will be ok with it, lest you get a "broken game data" error message from the game.
		If a texture is a normal map, it's automatically converted to and from the standard blue-toned form of a normal map.
	
	Materials:
		Texture references made by models can be automatically evaluated (,optionally unpacked) and assigned to materials.
		In order to for all of this to work without hassle, particularly if you're editing a map model, it's easiest to just extract the entire VBF into a folder and reference the 'dae.phyre' model from there.
		In order for textures to automatically be unpacked:
		the model's '.ah' (and potentially '_srm.ah') file must be present (in the vbf, it's either in the model's folder or it's parent folder), and the textures the model references must be in the same position relative to the model in the folder hierarchy. Sometimes, really long texture references like 'Asp_light_7_b4d5579dcdcef7ad_994e6f911de96d1d_155f0.dds.phyre' cannot be found directly near the model, but are actually referencing textures from the 'shared/' folder in the vbf, with shorter names like 'Asp_light_7.dds.phre', so that folder needs to exist and be positioned correctly.
		In order for textures to be automatically assigned to materials:
		The texture must be present in an intermediary format (.png) in the same relative position to the model it would have in the vbf. This should be done already if automatic texture unpacking is performed. The material texture paths stored in the outputted material are unfortunately absolute, since relative texture paths can't seem to currently work.
	
		No material information is saved to the mod output at the moment.
	
	Models:
	Bones:
		There used to be a limit on the number of bones assignable to a mesh, but no longer. There must however, be at least one bone reference by a mesh if it originally referenced bones, which also must be one of the influencing bones of the original skeleton (For example, usually not the root bone). If the mesh originally referenced bones, it cannot do so once modified.
		Vertices:
		There is no known upper limit to the amount of vertices a mesh has, so please test frequently to see if the game will be ok with it, lest you get a "broken game data" error message from the game. Also please make sure your meshes are triangulated just in case any unexpected errors occur. 
	Scene-Graph:
		In order for meshes to properly load, the names and hierarchical structure of the nodes in the scene graph must stay the same, or the tool won't be able to find the meshes that correspond with them. The positions, scale and rotations of the nodes are also saved to the mod file.

TIPS:
	Blend mode:
		The default blend mode in blender might make semi-opaque/translucent models difficult to see properly, so I suggest running either of the following scripts in Blender:
			import bpy
			for mat in bpy.data.materials: mat.blend_method = "CLIP"
		or
			import bpy
			for mat in bpy.data.materials: mat.blend_method = "HASHED"
		- The former being more useful in my opinion.
		These scripts will set the blend mode for every material in the scene to either 'clip' or 'hashed'.
	Vertex colours:
		Be mindful that vertex colours seem to dictate something like brightness, so if parts of the model are glowing in-game, you should probably darken their per-vertex colours.
	Modifying meshes:
		I wouldn't recommend jumping in with a super high-poly, 8k textured mesh since it might cause an error. Small tests and increases are a good idea.
	
Notes:
	I am currently investigating adapting this tool to work for FFX, which I think could easily be a reality soon.

Please let me know of any bugs or issues!
Licenses and Copyright notices may be found in the LICENSES folder.

Source Code: https://github.com/roelin720/FFXII-Asset-Converter