// Copyright Nuuby. All Rights Reserved.

#include "Render/MainWindow.h"
#include "SDK/Modes.h"
#include "Core/Program.h"

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

    ImGui::Begin("Dead Space Modding Tools - By Nuuby");

    if (ImGui::BeginTabBar("##MainTabBar"))
    {
        #pragma region Server

        if (ImGui::BeginTabItem("Server"))
        {
            ImGui::Text("Server Tools");

            ImGui::Spacing();
            ImGui::SeparatorText("Level Loading");

            static ImGuiComboFlags flags = 0;
            flags &= ~(ImGuiComboFlags_HeightMask_ & ~ImGuiComboFlags_HeightLargest);

            static int level_idx = 0;
            static int point_idx = 0;

            if (ImGui::BeginCombo("Level", s_level_to_point[level_idx].level, flags))
            {
                static ImGuiTextFilter level_filter;
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    level_filter.Clear();
                }
                ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
                level_filter.Draw("##LevelFilter", -FLT_MIN);

                for (int n = 0; n < IM_COUNTOF(s_level_to_point); n++)
                {
                    const bool is_selected = (level_idx == n);
                    if (level_filter.PassFilter(s_level_to_point[n].level))
                    {
                        if (ImGui::Selectable(s_level_to_point[n].level, is_selected))
                        {
                            if (level_idx != n)
                            {
                                level_idx = n;
                                point_idx = 0;
                            }
                        }
                    }
                }
                ImGui::EndCombo();
            }

            const std::vector<const char*>& startpoints = s_level_to_point[level_idx].startpoints;
            const char* preview = startpoints.empty() ? "(no startpoints)" : startpoints[point_idx];

            static bool customStartPoint = false;
            if (customStartPoint)
            {
                ImGui::BeginDisabled();
            }

            if (ImGui::BeginCombo("StartPoints", preview, flags))
            {
                static ImGuiTextFilter point_filter;
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    point_filter.Clear();
                }
                ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
                point_filter.Draw("##PointFilter", -FLT_MIN);

                for (int n = 0; n < (int)startpoints.size(); n++)
                {
                    const bool is_selected = (point_idx == n);
                    if (point_filter.PassFilter(startpoints[n]))
                    {
                        if (ImGui::Selectable(startpoints[n], is_selected))
                        {
                            point_idx = n;
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (customStartPoint)
            {
                ImGui::EndDisabled();
            }

            ImGui::Checkbox("Use Custom Startpoint", &customStartPoint);

            static char startPointOverride[128] = "";
            if (customStartPoint)
            {
                ImGui::InputTextWithHint("Custom StartPoint", "MyStartPoint", startPointOverride, IM_COUNTOF(startPointOverride));
            }

            // We don't want to load a level while already loading a level
            static bool disableLevelButton =
                !(g_program->m_clientState == ClientState_None || g_program->m_clientState == ClientState_Ingame);

            if (disableLevelButton)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("LoadLevel"))
            {
                g_program->m_server->LoadLevel(s_level_to_point[level_idx].level, s_level_to_point[level_idx].startpoints[point_idx]);
            }
            if (disableLevelButton)
            {
                ImGui::EndDisabled();
            }

            ImGui::Spacing();

            /*
            ImGui::SeparatorText("Join Server");

            static char* ipAddress;
            ImGui::InputText("IP Address", ipAddress, IM_COUNTOF(ipAddress));

            .
            ClientSettings* clientSettings = Settings<ClientSettings>(TYPEINFO_CLIENTSETTINGS);

            if (ImGui::Button("Connect"))
            {
                clientSettings->ServerIp = ipAddress;
            }
            */
            ImGui::SeparatorText("Server Settings");

            SimulationTimeSettings* simTimeSettings = Settings<SimulationTimeSettings>(TYPEINFO_SIMULATIONTIMESETTINGS);
            ImGui::InputFloat("Time Scale", &simTimeSettings->TimeScale, 0.1f, 1.0f, "%.2f");

            ImGui::EndTabItem();
        }
        #pragma endregion

        #pragma region Player
        if (ImGui::BeginTabItem("Player"))
        {
            ImGui::Text("Modify Player Settings");
            ImGui::EndTabItem();
        }
        #pragma endregion

        #pragma region Console
        if (ImGui::BeginTabItem("Console"))
        {
            ImGui::Text("UnImplemented, a working Console will take serveral hours of reverse engineering DeadSpace TypeInfo");
            ImGui::EndTabItem();
        }
        #pragma endregion

        ImGui::EndTabBar();
    }

    ImGui::End();
}
} // namespace Kyber