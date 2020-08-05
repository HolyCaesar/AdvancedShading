#pragma once
#include <memory>

class Win32FrameWork;

namespace IGuiCore
{
	extern Win32FrameWork* g_appPtr;

	extern bool g_bEnableGui;
	extern bool g_bShowMainMenuBar;

	void Init(Win32FrameWork* appPtr);
	void Terminate();

	void ShowMainMenuBar();
	void ShowMainGui();

	// Customized Functions
	void ShowForwardPlusWidgets();

	// Helper Functions
	void ShowAboutWindow(bool* p_open);
	void ShowMainMenuFile();

}

