
# SOURCE CODE MODIFICATIONS:
#	in FBXEXPORTER, in order to fix a bug that doesn't allow blender to import with blendshape:
#		the Blendshape pDiff magnitude check has been removed &
#		the line containing:
#			blendshape_name + FBX::SEPARATOR + "Blendshape"
#		has been chaned to
#			blendshape_name + FBX::SEPARATOR + "Geometry"
#	The number of exportable colours has been changes to be greater than 1 by placing the code segment in a for loop
#	Also added this code in FBXExporter.cpp:
#        for (unsigned int lr = 1; lr < m->GetNumColorChannels(); ++lr) {
#            FBX::Node layerExtra("Layer", int32_t(lr));
#            layerExtra.AddChild("Version", int32_t(100));
#            FBX::Node leExtra("LayerElement");
#            leExtra.AddChild("Type", "LayerElementColor");
#            leExtra.AddChild("TypedIndex", int32_t(lr));
#            layerExtra.AddChild(leExtra);
#            layerExtra.Dump(outstream, binary, indent);
#        }
#	above where a similar block exists for UVs
#	UnitScaleFactor has been changed to from 1 to 100 in the fbx exporter
#   AI_MAX_NUMBER_OF_TEXTURECOORDS has been increased to 16
#	mesh.h has been modified to change:
#		AI_MAX_NUMBER_OF_TEXTURECOORDS = 0x8
#		AI_MAX_NUMBER_OF_COLOR_SETS = 0x8
#	to
#		AI_MAX_NUMBER_OF_TEXTURECOORDS = 0x10
#		AI_MAX_NUMBER_OF_COLOR_SETS = 0x10	
#   ProcessHelper.cpp has been modifed to change:
#       static_assert(8 >= AI_MAX_NUMBER_OF_COLOR_SETS);
#       static_assert(8 >= AI_MAX_NUMBER_OF_TEXTURECOORDS);
#   to
#       static_assert(16 >= AI_MAX_NUMBER_OF_COLOR_SETS);
#       static_assert(16 >= AI_MAX_NUMBER_OF_TEXTURECOORDS);
#
#	JoinVerticesProcess.cpp has been modifed to change:
#		static_assert(AI_MAX_NUMBER_OF_COLOR_SETS    == 8, "AI_MAX_NUMBER_OF_COLOR_SETS    == 8");
#		static_assert(AI_MAX_NUMBER_OF_TEXTURECOORDS == 8, "AI_MAX_NUMBER_OF_TEXTURECOORDS == 8");
#	to
#	    static_assert(AI_MAX_NUMBER_OF_COLOR_SETS    == 16, "AI_MAX_NUMBER_OF_COLOR_SETS    == 16");
#		static_assert(AI_MAX_NUMBER_OF_TEXTURECOORDS == 16, "AI_MAX_NUMBER_OF_TEXTURECOORDS == 16");

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_STATIC_LIB OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/assimp")

