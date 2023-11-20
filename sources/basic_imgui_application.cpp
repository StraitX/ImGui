#include "imgui/basic_imgui_application.hpp"
#include "core/os/clock.hpp"
#include "graphics/api/gpu.hpp"

BasicImguiApplication::BasicImguiApplication(u32 width, u32 height, const char* title):
	m_Window(width, height, title)
{}


void BasicImguiApplication::OnEvent(const Event& e) {
	m_Backend.HandleEvent(e);

	if (e.Type == EventType::WindowClose)
		m_Window.Close();
}

int BasicImguiApplication::Run() {
	m_Window.SetEventsHandler({this, &BasicImguiApplication::OnEvent});

	UniquePtr<CommandPool> m_Pool{
		CommandPool::Create()
	};

	UniquePtr<CommandBuffer, CommandBufferDeleter> m_CmdBuffer{
		m_Pool->Alloc(), {m_Pool.Get()}
	};

	Semaphore acq, pst;
	Fence fence;

	Clock cl;
	while (m_Window.IsOpen()) {
		float dt = cl.GetElapsedTime().AsSeconds();
		cl.Restart();

		Tick(dt);

		auto size = m_Window.Size();
		
		if (size.x && size.y) {

			m_Backend.NewFrame(dt, Mouse::RelativePosition(m_Window), m_Window.Size());

			OnImGui();

			OnPreRender();

			m_Window.AcquireNextFramebuffer(&acq);

			m_CmdBuffer->Begin();
			{
				m_Backend.CmdRenderFrame(m_CmdBuffer.Get(), m_Window.CurrentFramebuffer());
				OnRender(m_CmdBuffer.Get());
			}
			m_CmdBuffer->End();

			GPU::Execute(m_CmdBuffer.Get(), acq, pst, fence);
			fence.WaitAndReset();
			m_Window.PresentCurrentFramebuffer(&pst);

			OnPostRender();
		}

		m_Window.DispatchEvents();
	}


	GPU::WaitIdle();

	return 0;
}