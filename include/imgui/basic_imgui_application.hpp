#pragma once

#include "imgui/backend.hpp"
#include "graphics/render_window.hpp"

class BasicImguiApplication {
	RenderWindow m_Window;
	ImGuiBackend m_Backend{m_Window.FramebufferPass()};
public:
	BasicImguiApplication(u32 width, u32 height, const char *title);

	virtual ~BasicImguiApplication() = default;

	virtual void Tick(float dt){ }

	virtual void OnImGui(){ }

	virtual void OnPreRender(){ }

	virtual void OnRender(CommandBuffer *cmd_buffer){ }

	virtual void OnPostRender(){ }

	virtual void OnEvent(const Event& e);

	int Run();

	const RenderWindow& Window()const{
		return m_Window;
	}
};