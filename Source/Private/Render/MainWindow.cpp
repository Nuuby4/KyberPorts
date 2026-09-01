// Copyright Nuuby. All Rights Reserved.

#include "Render/MainWindow.h"

#include "imgui/imgui.h"

namespace Kyber
{
MainWindow* g_mainWindow = nullptr;

void MainWindow::Draw()
{
    if (!m_enabled)
    {
        return;
    }

    ImGui::Begin("Kyber");
    ImGui::Text("Kyber overlay is running.");
    ImGui::End();
}
} // namespace Kyber
