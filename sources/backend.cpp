#include "imgui/backend.hpp"
#include "core/string.hpp"
#include "core/pair.hpp"
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

const char *s_VertexShader = 
    #include "shaders/imgui.vert.glsl"
;

const char *s_FragmentShader = 
    #include "shaders/imgui.frag.glsl"
;

static Array<ShaderBinding, 2> s_ShaderBindings = {
    ShaderBinding(0, 1, ShaderBindingType::UniformBuffer, ShaderStageBits::Vertex),
    ShaderBinding(1, 1, ShaderBindingType::Texture,       ShaderStageBits::Fragment)
};

static Array<VertexAttribute, 3> s_VertexAttributes = {
	VertexAttribute::Float32x2,
	VertexAttribute::Float32x2,
	VertexAttribute::UNorm8x4,
};

ImGuiBackend::ImGuiBackend(const RenderPass *pass):
	m_FramebufferPass(pass),
    m_SetLayout(
		DescriptorSetLayout::Create(s_ShaderBindings)
	),
    m_SetPool(
		DescriptorSetPool::Create({10, m_SetLayout.Get()})
	),
    m_VertexBuffer(
		Buffer::Create(sizeof(ImDrawVert) * 40, BufferMemoryType::DynamicVRAM, BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDestination)
	),
    m_IndexBuffer(
		Buffer::Create(sizeof(u32)        * 40, BufferMemoryType::DynamicVRAM, BufferUsageBits::IndexBuffer  | BufferUsageBits::TransferDestination)
	),
    m_TransformUniformBuffer(
		Buffer::Create(sizeof(TransformUniform), BufferMemoryType::DynamicVRAM, BufferUsageBits::UniformBuffer | BufferUsageBits::TransferSource)
	)
{
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

	RebuildFonts();

    {
		Array<const Shader*, 2> shaders;
		shaders[0] = Shader::Create(ShaderLang::GLSL, ShaderStageBits::Vertex,   {s_VertexShader,   String::Length(s_VertexShader)  } );
		shaders[1] = Shader::Create(ShaderLang::GLSL, ShaderStageBits::Fragment, {s_FragmentShader, String::Length(s_FragmentShader)} );

        GraphicsPipelineProperties props;
        props.Shaders = shaders;
        props.VertexAttributes = s_VertexAttributes;
        props.Pass = m_FramebufferPass;
        props.Layout = m_SetLayout.Get();

        m_Pipeline = GraphicsPipeline::Create(props);

		delete shaders[0];
		delete shaders[1];
    }


}

ImGuiBackend::~ImGuiBackend(){
	for (DescriptorSet* set : m_Sets)
		m_SetPool->Free(set);

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

void ImGuiBackend::CmdRenderFrame(CommandBuffer* cmd_buffer, const Framebuffer* fb){
	m_FreeSetIndex = 0;
	m_TexturesCache.Clear();
	ImGui::Render();
	const ImDrawData *data = ImGui::GetDrawData();

	for (int i = 0; i < data->CmdListsCount; i++) {

		for (const ImDrawCmd& cmd : data->CmdLists[i]->CmdBuffer) {
			const Texture2D* texture = (const Texture2D *)cmd.GetTexID();

			if(texture == m_ImGuiFont.Get())
				continue;

			m_TexturesCache.Add({texture, texture->Layout()});
			cmd_buffer->ChangeLayout(texture, TextureLayout::ShaderReadOnlyOptimal);
		}
	}


	ImVec2 clip_off = data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = data->FramebufferScale;

	TransformUniform uniform;
	uniform.u_Scale.x = clip_scale.x * 2.0f / data->DisplaySize.x;
	uniform.u_Scale.y = clip_scale.y * 2.0f / data->DisplaySize.y;
	uniform.u_Translate.x = -1.0f - data->DisplayPos.x * uniform.u_Scale.x;
	uniform.u_Translate.y = -1.0f - data->DisplayPos.x * uniform.u_Scale.y;

	m_TransformUniformBuffer->Copy(&uniform, sizeof(uniform));
	
	size_t vertex_buffer_size = 0;
	size_t index_buffer_size  = 0;

	for (int i = 0; i < data->CmdListsCount; i++) {
		ImDrawList* cmd_list = data->CmdLists[i];

		vertex_buffer_size += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		index_buffer_size  += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
	}

	if(vertex_buffer_size > m_VertexBuffer->Size())
		m_VertexBuffer->Realloc(vertex_buffer_size);

	if(index_buffer_size > m_IndexBuffer->Size())
		m_IndexBuffer->Realloc(index_buffer_size);

	size_t vertex_offset = 0;
	size_t index_offset  = 0;
	for (int i = 0; i < data->CmdListsCount; i++) {

		ImDrawList* cmd_list = data->CmdLists[i];

		size_t vertex_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		size_t index_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

		m_VertexBuffer->Copy(cmd_list->VtxBuffer.Data, vertex_size, vertex_offset);
		m_IndexBuffer->Copy(cmd_list->IdxBuffer.Data,  index_size,  index_offset);

		vertex_offset += vertex_size;
		index_offset += index_size;
	}

	auto window_size = fb->Size();
	cmd_buffer->Bind(m_Pipeline.Get());
	cmd_buffer->Bind(
		MakeNewSet(m_ImGuiFont.Get())
	);
	cmd_buffer->SetViewport(0, 0, window_size.x, window_size.y);

	cmd_buffer->BeginRenderPass(m_FramebufferPass, fb);
	cmd_buffer->BindVertexBuffer(m_VertexBuffer.Get());
	cmd_buffer->BindIndexBuffer(m_IndexBuffer.Get(), IndicesType::Uint16);
	
	size_t global_index_offset = 0;
	size_t global_vertex_offset = 0;
	for(int i = 0; i<data->CmdListsCount; i++){
		ImDrawList* cmd_list = data->CmdLists[i];

		size_t vertex_count = cmd_list->VtxBuffer.Size;
		size_t index_count = cmd_list->IdxBuffer.Size;

		for(const ImDrawCmd &cmd: cmd_list->CmdBuffer){
			
			if(cmd.UserCallback){
				cmd.UserCallback(cmd_list, &cmd);
				continue;
			}

			if(m_LastTexId != cmd.GetTexID()){
				cmd_buffer->Bind(
					MakeNewSet((Texture2D*)cmd.GetTexID())
				);
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

			cmd_buffer->SetScissor(offset.x, offset.y, extent.x, extent.y);
			cmd_buffer->DrawIndexed(cmd.ElemCount, cmd.IdxOffset + global_index_offset, global_vertex_offset);
		}
		global_vertex_offset += vertex_count;
		global_index_offset += index_count;
	}
	cmd_buffer->EndRenderPass();

	for (auto tex_layout : m_TexturesCache)
		cmd_buffer->ChangeLayout(tex_layout.First, tex_layout.Second);
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

void ImGuiBackend::RebuildFonts() {
	auto& io = GetIO();
	unsigned char *pixels = nullptr;
	int width = 0;
	int height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	
	if (!m_TextureSampler) {
		SamplerProperties props;
		props.MagFiltering = FilteringMode::Nearest;
		props.MinFiltering = FilteringMode::Nearest;

		m_TextureSampler = Sampler::Create(props);
	}
	
	m_ImGuiFont = Texture2D::Create(width, height, TextureFormat::RGBA8, TextureUsageBits::Sampled | TextureUsageBits::TransferDst, TextureLayout::ShaderReadOnlyOptimal);
	m_ImGuiFont->Copy(pixels, Vector2u(width, height));

	io.Fonts->SetTexID(m_ImGuiFont.Get());
}

DescriptorSet* ImGuiBackend::MakeNewSet(Texture2D* texture){
	if (m_FreeSetIndex == m_Sets.Size())
		m_Sets.Add(m_SetPool->Alloc());

	DescriptorSet *set = m_Sets[m_FreeSetIndex++];

	set->UpdateUniformBinding(0, 0, m_TransformUniformBuffer.Get());
	set->UpdateTextureBinding(1, 0, texture, m_TextureSampler.Get());
	return set;
}
