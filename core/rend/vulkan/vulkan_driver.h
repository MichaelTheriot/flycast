/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include "rend/imgui_driver.h"
#include "imgui_impl_vulkan.h"
#include "vulkan_context.h"

class VulkanDriver final : public ImGuiDriver
{
public:
	~VulkanDriver() {
		ImGui_ImplVulkan_Shutdown();
	}

	void newFrame() override {
	}

	void renderDrawData(ImDrawData *drawData) override {
		ImGui_ImplVulkan_RenderDrawData(drawData);
	}
	void present() override {
		VulkanContext::Instance()->Present();
	}
};
