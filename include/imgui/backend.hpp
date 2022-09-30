#ifndef STRAITX_IMGUI_BACKEND_HPP
#define STRAITX_IMGUI_BACKEND_HPP

#include "imgui/widgets.hpp"
#include "core/math/vector2.hpp"
#include "core/array.hpp"
#include "core/raw_var.hpp"
#include "core/noncopyable.hpp"
#include "core/result.hpp"
#include "core/pair.hpp"
#include "core/os/events.hpp"
#include "core/unique_ptr.hpp"
#include "graphics/api/semaphore.hpp"
#include "graphics/api/descriptor_set.hpp"
#include "graphics/api/command_buffer.hpp"
#include "graphics/api/fence.hpp"

class DescriptorSetLayout;
class DescriptorSet;
class DescriptorSetPool;
class RenderPass;
class Framebuffer;
class Shader;
class GraphicsPipeline;
class CommandPool;
class CommandBuffer;
class Fence;
class Buffer;
class Texture2D;
class Sampler;

class ImGuiBackend: public NonCopyable{
private:
    struct TransformUniform{
		Vector2f u_Scale;
		Vector2f u_Translate;
	};
private:
	UniquePtr<Texture2D> m_ImGuiFont = nullptr;
    UniquePtr<Sampler> m_TextureSampler = nullptr;
    Texture2D* m_LastTexId = nullptr;
    ImGuiContext *m_Context = nullptr;

    const RenderPass  *m_FramebufferPass    = nullptr;
    UniquePtr<DescriptorSetLayout> m_SetLayout;
    UniquePtr<DescriptorSetPool> m_SetPool;
    List<Pair<DescriptorSet*, const Texture2D *>> m_Sets;
    size_t m_FreeSetIndex = 0;

    UniquePtr<GraphicsPipeline> m_Pipeline = nullptr;

    UniquePtr<Buffer> m_VertexBuffer;
    UniquePtr<Buffer> m_IndexBuffer;
    UniquePtr<Buffer> m_TransformUniformBuffer;

    List <Pair<const Texture2D*, TextureLayout>> m_TexturesCache;
public:
    ImGuiBackend(const RenderPass *pass);

    ~ImGuiBackend();

    void NewFrame(float dt, Vector2s mouse_pos, Vector2s window_size);

    void CmdRenderFrame(CommandBuffer* cmd_buffer, const Framebuffer *fb);
    
    bool HandleEvent(const Event &e);

    ImGuiIO &GetIO();

    void RebuildFonts();
private:
    DescriptorSet* MakeNewSet(Texture2D* texture);
};

#endif//STRAITX_IMGUI_BACKEND_HPP