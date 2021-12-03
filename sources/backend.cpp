#include "imgui/backend.hpp"
#include "core/string.hpp"
#include "graphics/api/graphics_api.hpp"
#include "graphics/api/swapchain.hpp"
#include "graphics/api/command_buffer.hpp"
#include "graphics/api/semaphore.hpp"
#include "graphics/api/gpu.hpp"
#include "graphics/api/render_pass.hpp"
#include "graphics/api/framebuffer.hpp"
#include "graphics/api/shader.hpp"
#include "graphics/api/graphics_pipeline.hpp"
#include "graphics/api/buffer.hpp"
#include "graphics/api/descriptor_set.hpp"

const char *s_VertexShader = R"(
#version 440 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;

layout(location = 0)out vec4 v_Color;
layout(location = 1)out vec2 v_UV;

layout(binding = 0) uniform Transform{ 
    vec2 u_Scale; 
    vec2 u_Translate; 
};

void main()
{
	v_Color = a_Color;
    v_UV = a_UV;
    gl_Position = vec4(a_Pos * u_Scale + u_Translate, 0, 1);
}
)";

const char *s_FragmentShader = R"(
#version 440 core

layout(location = 0)in vec4 v_Color;
layout(location = 1)in vec2 v_UV;

layout(location = 0) out vec4 f_Color;

layout(binding = 1) uniform sampler2D u_Texture;

void main()
{
    f_Color = v_Color * texture(u_Texture, v_UV);
}
)";

static Array<ShaderBinding, 2> s_ShaderBindings = {
    ShaderBinding(0, 1, ShaderBindingType::UniformBuffer, ShaderStageBits::Vertex),
    ShaderBinding(1, 1, ShaderBindingType::Texture,       ShaderStageBits::Fragment)
};

static Array<VertexAttribute, 3> s_VertexAttributes = {
	VertexAttribute::Float32x2,
	VertexAttribute::Float32x2,
	VertexAttribute::UNorm8x4,
};

ImGuiBackend::SemaphoreRing::SemaphoreRing(){
    FullRing[1] = &LoopingPart[0];
    FullRing[2] = &LoopingPart[1];
}

void ImGuiBackend::SemaphoreRing::Begin(const Semaphore *first){
    SX_CORE_ASSERT(FullRing[0] == nullptr, "ImGuiBackend: SemaphoreRing should be ended");
    Index = 0;
    FullRing[0] = first;
}

void ImGuiBackend::SemaphoreRing::End(){
    FullRing[0] = nullptr;
}

const Semaphore *ImGuiBackend::SemaphoreRing::Current(){
    return FullRing[Index];
}

const Semaphore *ImGuiBackend::SemaphoreRing::Next(){
    return FullRing[NextIndex()];
}

void ImGuiBackend::SemaphoreRing::Advance(){
    Index = NextIndex();
}

u32 ImGuiBackend::SemaphoreRing::NextIndex(){
    u32 next_index = Index + 1;
    if(next_index == 3)
        next_index = 1;
    return next_index;
}


Result ImGuiBackend::Initialize(const RenderPass *pass){
    m_Context = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_Context);

	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	ImGui::GetStyle().WindowRounding = 8;
	//ImGui::GetStyle().ScaleAllSizes((screen_dpi.x > screen_dpi.y ? screen_dpi.x : screen_dpi.y) / s_BaseDPI);

    io.BackendPlatformName = "StraitXImGui-Backend";

	io.DisplayFramebufferScale = ImVec2(1, 1);

	io.KeyMap[ImGuiKey_Tab] = (int)Key::Tab;
    io.KeyMap[ImGuiKey_LeftArrow] = (int)Key::Left;
    io.KeyMap[ImGuiKey_RightArrow] = (int)Key::Right;
    io.KeyMap[ImGuiKey_UpArrow] = (int)Key::Up;
    io.KeyMap[ImGuiKey_DownArrow] = (int)Key::Down;
    io.KeyMap[ImGuiKey_PageUp] = (int)Key::PageUp;
    io.KeyMap[ImGuiKey_PageDown] = (int)Key::PageDown;
    io.KeyMap[ImGuiKey_Home] = (int)Key::Home;
    io.KeyMap[ImGuiKey_End] = (int)Key::End;
    io.KeyMap[ImGuiKey_Insert] = (int)Key::Insert;
    io.KeyMap[ImGuiKey_Delete] = (int)Key::Delete;
    io.KeyMap[ImGuiKey_Backspace] = (int)Key::Backspace;
    io.KeyMap[ImGuiKey_Space] = (int)Key::Space;
    io.KeyMap[ImGuiKey_Enter] = (int)Key::Enter;
    io.KeyMap[ImGuiKey_Escape] = (int)Key::Escape;
    io.KeyMap[ImGuiKey_KeyPadEnter] = (int)Key::KeypadEnter;
    io.KeyMap[ImGuiKey_A] = (int)Key::A;
    io.KeyMap[ImGuiKey_C] = (int)Key::C;
    io.KeyMap[ImGuiKey_V] = (int)Key::V;
    io.KeyMap[ImGuiKey_X] = (int)Key::X;
    io.KeyMap[ImGuiKey_Y] = (int)Key::Y;
    io.KeyMap[ImGuiKey_Z] = (int)Key::Z;

    {

        
		unsigned char *pixels = nullptr;
		int width = 0;
		int height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        SamplerProperties font_text_sampler_props;
		font_text_sampler_props.MagFiltering = FilteringMode::Nearest;
		font_text_sampler_props.MinFiltering = FilteringMode::Nearest;

        m_TextureSampler = Sampler::Create(font_text_sampler_props);
        
		m_ImGuiFont = Texture2D::Create(width, height, TextureFormat::RGBA8, TextureUsageBits::Sampled | TextureUsageBits::TransferDst, TextureLayout::ShaderReadOnlyOptimal);
		m_ImGuiFont->Copy(pixels, Vector2u(width, height));

		io.Fonts->SetTexID(m_ImGuiFont);
	}


    m_FramebufferPass = pass;

    m_SetLayout = DescriptorSetLayout::Create(s_ShaderBindings);

    m_SetPool = DescriptorSetPool::Create({1, m_SetLayout});
    m_Set = m_SetPool->Alloc();

    m_Shaders[0] = Shader::Create(ShaderLang::GLSL, ShaderStageBits::Vertex,   {s_VertexShader,   String::Length(s_VertexShader)  } );
    m_Shaders[1] = Shader::Create(ShaderLang::GLSL, ShaderStageBits::Fragment, {s_FragmentShader, String::Length(s_FragmentShader)} );

    {
        GraphicsPipelineProperties props;
        props.Shaders = m_Shaders;
        props.VertexAttributes = s_VertexAttributes;
        props.Pass = m_FramebufferPass;
        props.Layout = m_SetLayout;

        m_Pipeline = GraphicsPipeline::Create(props);
    }

    m_CmdPool = CommandPool::Create();
    m_CmdBuffer = m_CmdPool->Alloc();

    m_SemaphoreRing.Construct();
    m_DrawingFence = new Fence();

    m_VertexBuffer = Buffer::Create(sizeof(ImDrawVert) * 40, BufferMemoryType::DynamicVRAM, BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDestination);
    m_IndexBuffer  = Buffer::Create(sizeof(u32)        * 40, BufferMemoryType::DynamicVRAM, BufferUsageBits::IndexBuffer  | BufferUsageBits::TransferDestination);
    m_TransformUniformBuffer = Buffer::Create(sizeof(TransformUniform), BufferMemoryType::DynamicVRAM, BufferUsageBits::UniformBuffer | BufferUsageBits::TransferSource);

    m_Set->UpdateUniformBinding(0, 0, m_TransformUniformBuffer);
	m_Set->UpdateTextureBinding(1, 0, m_ImGuiFont, m_TextureSampler);

    m_DrawingFence->Signal();

    return Result::Success;
}

void ImGuiBackend::Finalize(){
    m_DrawingFence->WaitAndReset();

    delete m_VertexBuffer;
    delete m_IndexBuffer;
    delete m_TransformUniformBuffer;

    delete m_DrawingFence;
    m_SemaphoreRing.Destruct();

    m_CmdPool->Free(m_CmdBuffer);
    delete m_CmdPool;

    delete m_Pipeline;

    for(auto shader: m_Shaders)
        delete shader;
    
    m_SetPool->Free(m_Set);
    delete m_SetPool;
    delete m_SetLayout;

    delete m_TextureSampler;
    delete m_ImGuiFont;

    ImGui::DestroyContext(m_Context);
}

void ImGuiBackend::NewFrame(float dt, Vector2s mouse_position, Vector2s window_size){
    ImGui::SetCurrentContext(m_Context);
    ImGuiIO& io = GetIO();
    io.DisplaySize = ImVec2(window_size.x, window_size.y);
    io.DeltaTime = dt;
    io.MousePos = ImVec2(mouse_position.x/io.DisplayFramebufferScale.x, mouse_position.y/io.DisplayFramebufferScale.y);

	ImGui::NewFrame();
}

void ImGuiBackend::RenderFrame(const Framebuffer *fb, const Semaphore *wait, const Semaphore *signal){
	m_SemaphoreRing->Begin(wait);

	ImGui::Render();

	const ImDrawData *data = ImGui::GetDrawData();

	ImVec2 clip_off = data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = data->FramebufferScale;

	TransformUniform uniform;
	uniform.u_Scale.x = clip_scale.x * 2.0f / data->DisplaySize.x;
	uniform.u_Scale.y = clip_scale.y * 2.0f / data->DisplaySize.y;
	uniform.u_Translate.x = -1.0f - data->DisplayPos.x * uniform.u_Scale.x;
	uniform.u_Translate.y = -1.0f - data->DisplayPos.x * uniform.u_Scale.y;

	m_TransformUniformBuffer->Copy(&uniform, sizeof(uniform));

	BeginDrawing(fb);
	
	for(int i = 0; i<data->CmdListsCount; i++){
		
		ImDrawList* cmd_list = data->CmdLists[i];

		size_t vertex_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		size_t index_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

		if(vertex_size > m_VertexBuffer->Size())
			m_VertexBuffer->Realloc(vertex_size);

		if(index_size > m_IndexBuffer->Size())
			m_IndexBuffer->Realloc(index_size);

		m_VertexBuffer->Copy(cmd_list->VtxBuffer.Data, vertex_size);
		m_IndexBuffer->Copy(cmd_list->IdxBuffer.Data, index_size);

		for(const ImDrawCmd &cmd: cmd_list->CmdBuffer){
			
			if(cmd.UserCallback){
				cmd.UserCallback(cmd_list, &cmd);
				continue;
			}

			if(m_LastTexId != cmd.GetTexID()){
				m_LastTexId = (Texture2D*)cmd.GetTexID();

				EndDrawing(m_SemaphoreRing->Current(), m_SemaphoreRing->Next());
				m_SemaphoreRing->Advance();
				BeginDrawing(fb);

				m_Set->UpdateTextureBinding(1, 0, m_LastTexId, m_TextureSampler);
			}

			ImVec4 clip_rect;
			clip_rect.x = (cmd.ClipRect.x - clip_off.x) * clip_scale.x;
			clip_rect.y = (cmd.ClipRect.y - clip_off.y) * clip_scale.y;
			clip_rect.z = (cmd.ClipRect.z - clip_off.x) * clip_scale.x;
			clip_rect.w = (cmd.ClipRect.w - clip_off.y) * clip_scale.y;

			if (clip_rect.x < 0.0f)
				clip_rect.x = 0.0f;
			if (clip_rect.y < 0.0f)
				clip_rect.y = 0.0f;


			Vector2f offset(clip_rect.x, clip_rect.y);
			Vector2f extent(clip_rect.z - clip_rect.x, clip_rect.w - clip_rect.y);

			m_CmdBuffer->Bind(m_Set);
			m_CmdBuffer->SetScissor(offset.x, offset.y, extent.x, extent.y);
			m_CmdBuffer->BindVertexBuffer(m_VertexBuffer);
			m_CmdBuffer->BindIndexBuffer(m_IndexBuffer, IndicesType::Uint16);
			m_CmdBuffer->DrawIndexed(cmd.ElemCount, cmd.IdxOffset);
		}
		
		EndDrawing(m_SemaphoreRing->Current(), m_SemaphoreRing->Next());
		m_SemaphoreRing->Advance();
		BeginDrawing(fb);
	}
	EndDrawing(m_SemaphoreRing->Current(), signal);
	m_SemaphoreRing->End();
}

bool ImGuiBackend::HandleEvent(const Event &e){
	ImGuiIO &io = GetIO();

	switch(e.Type){
	case EventType::TextEntered:
		io.AddInputCharacter(e.TextEntered.Unicode);
		break;
	case EventType::KeyPress:
		io.KeysDown[(size_t)e.KeyPress.KeyCode] = true;
		
		io.KeyAlt = io.KeysDown[(size_t)Key::LeftAlt] || io.KeysDown[(size_t)Key::RightAlt];
		io.KeyShift = io.KeysDown[(size_t)Key::LeftShift] || io.KeysDown[(size_t)Key::RightShift];
		io.KeyCtrl = io.KeysDown[(size_t)Key::LeftControl] || io.KeysDown[(size_t)Key::RightControl];
		io.KeySuper = io.KeysDown[(size_t)Key::LeftSuper] || io.KeysDown[(size_t)Key::RightSuper];
		break;
	case EventType::KeyRelease:
		io.KeysDown[(size_t)e.KeyRelease.KeyCode] = false;

		io.KeyAlt = io.KeysDown[(size_t)Key::LeftAlt] || io.KeysDown[(size_t)Key::RightAlt];
		io.KeyShift = io.KeysDown[(size_t)Key::LeftShift] || io.KeysDown[(size_t)Key::RightShift];
		io.KeyCtrl = io.KeysDown[(size_t)Key::LeftControl] || io.KeysDown[(size_t)Key::RightControl];
		io.KeySuper = io.KeysDown[(size_t)Key::LeftSuper] || io.KeysDown[(size_t)Key::RightSuper];
		break;
	case EventType::FocusOut:
	case EventType::FocusIn:
		for(int i = 0; i<lengthof(io.KeysDown); i++)
			io.KeysDown[i] = 0;
		io.KeyAlt = false;
		io.KeyShift = false;
		io.KeyCtrl = false;
		io.KeySuper = false;
		break;
	case EventType::MouseWheel:
    	io.MouseWheel += (float)e.MouseWheel.Delta/2.f;
		break;
	case EventType::MouseButtonPress:
		if(e.MouseButtonRelease.Button == Mouse::Left)
			io.MouseDown[ImGuiMouseButton_Left] = true;
		if(e.MouseButtonRelease.Button == Mouse::Right)
			io.MouseDown[ImGuiMouseButton_Right] = true;
		if(e.MouseButtonRelease.Button == Mouse::Middle)
			io.MouseDown[ImGuiMouseButton_Middle] = true;
		break;
	case EventType::MouseButtonRelease:
		if(e.MouseButtonRelease.Button == Mouse::Left)
			io.MouseDown[ImGuiMouseButton_Left] = false;
		if(e.MouseButtonRelease.Button == Mouse::Right)
			io.MouseDown[ImGuiMouseButton_Right] = false;
		if(e.MouseButtonRelease.Button == Mouse::Middle)
			io.MouseDown[ImGuiMouseButton_Middle] = false;
		break;
	default:
		(void)0;
	}

	return io.WantCaptureMouse;
}

ImGuiIO &ImGuiBackend::GetIO(){
	ImGui::SetCurrentContext(m_Context);
	return ImGui::GetIO();
}

void ImGuiBackend::BeginDrawing(const Framebuffer *fb){
	m_DrawingFence->WaitAndReset();

	auto window_size = fb->Size();
	m_CmdBuffer->Begin();
	m_CmdBuffer->Bind(m_Pipeline);
	m_CmdBuffer->SetViewport(0, 0, window_size.x, window_size.y);

	m_CmdBuffer->BeginRenderPass(m_FramebufferPass, fb);
}

void ImGuiBackend::EndDrawing(const Semaphore *wait, const Semaphore *signal){
	m_CmdBuffer->EndRenderPass();
	m_CmdBuffer->End();

	GPU::Execute(m_CmdBuffer, *wait, *signal, *m_DrawingFence);
}
