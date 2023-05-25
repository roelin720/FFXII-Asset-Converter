#include "PhyreGUITexture.h"
#include "glad/glad.h"
#include "assimp/../../contrib/stb/stb_image.h"

Phyre::GUITexture::GUITexture(const void* png_data, size_t data_size)
{
	int w, h, n;
	uint8_t* pixels = stbi_load_from_memory((const stbi_uc*)png_data, data_size, &w, &h, &n, 4);
	if (!pixels || w <= 0 || h <=0)
	{
		return;
	}
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(pixels);
	width = w;
	height = h;
}

Phyre::GUITexture::operator ImTextureID() const
{
	return (ImTextureID)(int64_t)id;
}

void Phyre::GUITexture::free()
{
	glDeleteTextures(1, &id);
	id = 0;
	width = 0;
	height = 0;
}
