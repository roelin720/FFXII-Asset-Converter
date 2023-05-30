#pragma once
#include "imgui.h"
#include <stdint.h>
#include <string>

namespace Phyre
{
	struct GUIIcon
	{
		uint32_t id = 0;
		uint32_t width = 0;
		uint32_t height = 0;

		GUIIcon() noexcept = default;
		GUIIcon(const uint8_t* icon_data, size_t data_size);

		operator ImTextureID() const;

		void free();
	};
}