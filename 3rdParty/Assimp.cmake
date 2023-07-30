
# SOURCE CODE MODIFICATIONS:
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
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_INJECTED_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_EXPORTER ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/assimp")

