cmake_minimum_required (VERSION 3.8)

project (Converter)
set(CONVERTER_VERSION "2.0.1")

option(POPULATE_BIN "place binaries in /bin and zip binaries with bundled_files to /bin" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

set(ROOT_PATH "${CMAKE_CURRENT_LIST_DIR}")
set(ADDITIONAL_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}")
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty")
set(THIRD_PARTY_BINARY_DIR "${CMAKE_BINARY_DIR}/3rdParty")
list(PREPEND CMAKE_MODULE_PATH "${THIRD_PARTY_DIR}")

set(CMAKE_CXX_STANDARD 23)
add_compile_definitions(NOMINMAX)

file(READ "${CMAKE_CURRENT_LIST_DIR}/bundled_files/README.txt" README_CONTENTS)
string(REGEX REPLACE "version[ \t]*([0-9]+\\.)*[0-9]+" "version ${CONVERTER_VERSION}" NEW_README_CONTENTS "${README_CONTENTS}")
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/bundled_files/README.txt" "${NEW_README_CONTENTS}")

include(Assimp)
include(DirectXTex)
include(ArgParser)
include(GLM)
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/assimp/code")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/assimp/include")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${THIRD_PARTY_BINARY_DIR}/assimp/contrib/zlib")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${THIRD_PARTY_DIR}/assimp/code/AssetLib/AssimpImportHook")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${glm_SOURCE_DIR}")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${directxtex_SOURCE_DIR}")
LIST(APPEND ADDITIONAL_INCLUDE_DIRECTORIES "${arg_parser_SOURCE_DIR}")

find_package(PythonInterp REQUIRED)

add_custom_command(
	OUTPUT 
		"${ROOT_PATH}/Enums.h"							
    COMMAND 
		"${PYTHON_EXECUTABLE}" "${ROOT_PATH}/Enums.py" "${ROOT_PATH}/Enums.txt" "${ROOT_PATH}/Enums.h"
	DEPENDS 
		"${ROOT_PATH}/Enums.py"
		"${ROOT_PATH}/Enums.txt"
	COMMENT 
		"Generating enums"
)

ADD_CUSTOM_TARGET(GenerateEnums ALL
	DEPENDS 
		"${CMAKE_CURRENT_LIST_DIR}/Enums.h"
    COMMENT 
		"Checking if enum generation is required"
)

set(CONVERTER_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/Enums.h"
	"${CMAKE_CURRENT_LIST_DIR}/ConverterInterface.h"
	"${CMAKE_CURRENT_LIST_DIR}/ConverterInterface.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Scene.h"
	"${CMAKE_CURRENT_LIST_DIR}/Scene.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Texture2D.h"
	"${CMAKE_CURRENT_LIST_DIR}/Texture2D.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/AssimpConversion.h"
	"${CMAKE_CURRENT_LIST_DIR}/AssimpConversion.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FileIOUtils.h"
	"${CMAKE_CURRENT_LIST_DIR}/FileIOUtils.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FileStructs.h"
	"${CMAKE_CURRENT_LIST_DIR}/FileStructs.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FileObjectBlock.h"
	"${CMAKE_CURRENT_LIST_DIR}/FileObjectBlock.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/VBFArchive.h"
	"${CMAKE_CURRENT_LIST_DIR}/VBFArchive.cpp"
	"${THIRD_PARTY_DIR}/md5/md5.h"
	"${THIRD_PARTY_DIR}/md5/md5.cpp"
)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/CMD")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/GUI")

