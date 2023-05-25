#pragma once
#include "imgui.h"
#include <stdint.h>
#include <string>

namespace Phyre
{
	struct GUITexture
	{
		uint32_t id = 0;
		uint32_t width = 0;
		uint32_t height = 0;

		GUITexture() noexcept = default;
		GUITexture(const void* png_data, size_t data_size);

		operator ImTextureID() const;

		void free();
	};
}