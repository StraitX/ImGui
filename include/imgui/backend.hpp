#ifndef STRAITX_IMGUI_BACKEND_HPP
#define STRAITX_IMGUI_BACKEND_HPP

#include "imgui/widgets.hpp"
#include "core/math/vector2.hpp"
#include "core/array.hpp"
#include "core/raw_var.hpp"
#include "core/noncopyable.hpp"
#include "core/result.hpp"
#include "core/os/events.hpp"
#include "graphics/api/semaphore.hpp"

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
	Texture2D *m_ImGuiFont      = nullptr;
    Sampler   *m_TextureSampler = nullptr;
	ImTextureID m_LastTexId = nullptr;
    ImGuiContext *m_Context = nullptr;

    const RenderPass  *m_FramebufferPass    = nullptr;
    const Framebuffer *m_CurrentFramebuffer = nullptr;
    const DescriptorSetLayout *m_SetLayout  = nullptr;
    DescriptorSetPool         *m_SetPool    = nullptr;
    DescriptorSet             *m_Set        = nullptr;

    Array<const Shader *, 2> m_Shaders = {nullptr, nullptr};
    GraphicsPipeline *m_Pipeline       = nullptr;

    CommandPool   *m_CmdPool   = nullptr;
    CommandBuffer *m_CmdBuffer = nullptr;

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

    RawVar<SemaphoreRing> m_SemaphoreRing;

    Fence *m_DrawingFence = nullptr;

    Buffer *m_VertexBuffer = nullptr;
    Buffer *m_IndexBuffer  = nullptr;
    Buffer *m_TransformUniformBuffer = nullptr;
public:
    Result Initialize(const RenderPass *pass);

    void Finalize();

    void BeginFrame(const Framebuffer *fb, Vector2s mouse_position, float dt, const Semaphore *wait_semaphore);

    void EndFrame(const Semaphore *signal_semaphore);

    void HandleEvent(const Event &e);

    ImGuiIO &GetIO();
private:

    void BeginDrawing();
    
    void EndDrawing(const Semaphore *wait, const Semaphore *signal);
};

#endif//STRAITX_IMGUI_BACKEND_HPP