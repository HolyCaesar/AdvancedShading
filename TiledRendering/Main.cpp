
#include "stdafx.h"
#include "RenderingDemo.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    RenderingDemo sample(1280, 720, L"D3D12 Training Ground");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}
