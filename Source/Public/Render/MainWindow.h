// Copyright Nuuby. All Rights Reserved.

#pragma once

namespace Kyber
{
class MainWindow
{
public:
    void Draw();

    bool IsEnabled() const 
    { 
        return m_enabled; 
    }

    void SetEnabled(bool enabled) 
    { 
        m_enabled = enabled; 
    }
    
    void Toggle() 
    {
        m_enabled = !m_enabled;
    }

private:
    bool m_enabled = true;
};

extern MainWindow* g_mainWindow;
} // namespace Kyber
