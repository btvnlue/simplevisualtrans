// SimpleVisualTransVT.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "SimpleVisualTransVT.h"
#include "WindowViewVT.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    WindowViewVT::StartWindow(hInstance, hPrevInstance);
    while (WindowViewVT::IsRunning()) {
        Sleep(1000);
    }
}


