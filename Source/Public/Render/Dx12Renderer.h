#pragma once

namespace Kyber
{
class Dx12Renderer
{
public:
    bool m_initialized = false;

    Dx12Renderer();
    ~Dx12Renderer();
};
} // namespace Kyber

extern Kyber::Dx12Renderer* g_renderer;
