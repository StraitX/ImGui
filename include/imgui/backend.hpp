#ifndef STRAITX_IMGUI_BACKEND_HPP
#define STRAITX_IMGUI_BACKEND_HPP

#include "imgui/widgets.hpp"
#include "core/math/vector2.hpp"
#include "core/array.hpp"
#include "core/raw_var.hpp"
#include "core/noncopyable.hpp"
#include "core/result.hpp"
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
    UniquePtr<DescriptorSet, DescriptorSetDeleter> m_Set;

    UniquePtr<GraphicsPipeline> m_Pipeline = nullptr;

    UniquePtr<CommandPool> m_CmdPool;
    UniquePtr<CommandBuffer, CommandBufferDeleter> m_CmdBuffer;

    class SemaphoreRing{
    private:
        Semaphore LoopingPart[2] = {};
        const Semaphore *FullRing[3] = {nullptr, nullptr, nullptr};
        u32 Index = 0;
    public:
        SemaphoreRing();

        void Begin(const Semaphore *first);

        void End();

        const Semaphore *Current();
        const Semaphore *Next();

        void Advance();

        u32 NextIndex();
    };

    SemaphoreRing m_SemaphoreRing;

    Fence m_DrawingFence;

    UniquePtr<Buffer> m_VertexBuffer;
    UniquePtr<Buffer> m_IndexBuffer;
    UniquePtr<Buffer> m_TransformUniformBuffer;
public:
    ImGuiBackend(const RenderPass *pass);

    ~ImGuiBackend();

    void NewFrame(float dt, Vector2s mouse_pos, Vector2s window_size);

    void RenderFrame(const Framebuffer *fb, const Semaphore *wait, const Semaphore *signal);
    
    bool HandleEvent(const Event &e);

    ImGuiIO &GetIO();

    void RebuildFonts();
private:
    void BeginDrawing(const Framebuffer *fb);
    
    void EndDrawing(const Semaphore *wait, const Semaphore *signal);
};

#endif//STRAITX_IMGUI_BACKEND_HPP