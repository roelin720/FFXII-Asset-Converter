#include "PhyreGUIIcon.h"
#include "glad/glad.h"

Phyre::GUIIcon::GUIIcon(const uint8_t* icon_data, size_t data_size)
{
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icon_data[0], icon_data[1], 0, GL_RGBA, GL_UNSIGNED_BYTE, icon_data + 2);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
	width = icon_data[0];
	height = icon_data[1];
}

Phyre::GUIIcon::operator ImTextureID() const
{
	return (ImTextureID)(int64_t)id;
}

void Phyre::GUIIcon::free()
{
	glDeleteTextures(1, &id);
	id = 0;
	width = 0;
	height = 0;
}
