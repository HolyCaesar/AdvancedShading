#include "stdafx.h"
#include "GUICore.h"

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#include "Win32FrameWork.h"
#include "TiledRendering.h"

#include <ctype.h>          // toupper
#include <limits.h>         // INT_MIN, INT_MAX
#include <math.h>           // sqrtf, powf, cosf, sinf, floorf, ceilf
#include <stdio.h>          // vsnprintf, sscanf, printf
#include <stdlib.h>         // NULL, malloc, free, atoi

#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>         // intptr_t
#else
#include <stdint.h>         // intptr_t
#endif

// Play it nice with Windows users (Update: May 2018, Notepad now supports Unix-style carriage returns!)
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

// Helpers
#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf    _snprintf
#endif
#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf   _vsnprintf
#endif



namespace IGuiCore
{
	// App pointer
	Win32FrameWork* g_appPtr = nullptr;

	void Init(Win32FrameWork* appPtr) { g_appPtr = appPtr; }
	void Terminate() { g_appPtr = nullptr; }

	// Control Variables
	bool g_bEnableGui = true;
	bool g_bShowMainMenuBar = true;

	void ShowMainMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ShowMainMenuFile();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
				if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void ShowMainGui()
	{
		// Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
		// Most ImGui functions would normally just crash if the context is missing.
		IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing dear imgui context. Refer to examples app!");

		// Decide which module to display
		if (!g_bEnableGui) return;
		//if (g_bShowMainMenuBar) ShowMainMenuBar();

		// Demonstrate the various window flags. Typically you would just use the default!
		static bool no_titlebar = false;
		static bool no_scrollbar = false;
		static bool no_menu = false;
		static bool no_move = false;
		static bool no_resize = false;
		static bool no_collapse = false;
		static bool no_close = false;
		static bool no_nav = false;
		static bool no_background = false;
		static bool no_bring_to_front = false;

		ImGuiWindowFlags window_flags = 0;
		if (no_titlebar)        window_flags |= ImGuiWindowFlags_NoTitleBar;
		if (no_scrollbar)       window_flags |= ImGuiWindowFlags_NoScrollbar;
		if (!no_menu)           window_flags |= ImGuiWindowFlags_MenuBar;
		if (no_move)            window_flags |= ImGuiWindowFlags_NoMove;
		if (no_resize)          window_flags |= ImGuiWindowFlags_NoResize;
		if (no_collapse)        window_flags |= ImGuiWindowFlags_NoCollapse;
		if (no_nav)             window_flags |= ImGuiWindowFlags_NoNav;
		if (no_background)      window_flags |= ImGuiWindowFlags_NoBackground;
		if (no_bring_to_front)  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (no_close)           g_bEnableGui = NULL; // Don't pass our bool* to Begin // TODO: g_bEnableGui is a pointer

		// We specify a default position/size in case there's no data in the .ini file.
		// We only do it to make the demo applications a little more welcoming, but typically this isn't required.
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

		// Main body of the Demo window starts here.
		if (!ImGui::Begin("Rendering Demo", &g_bEnableGui, window_flags))
		{
			// Early out if the window is collapsed, as an optimization.
			ImGui::End();
			return;
		}

		/**************************/
		// Menu Bar
		/**************************/
		// Rendering Technique apps (accessible from the "Tools" menu)
		//static bool show_app_metrics = false;
		//static bool show_app_style_editor = false;
		static bool show_app_about = false;
		if (show_app_about) ShowAboutWindow(&show_app_about);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Menu"))
			{
				ShowMainMenuFile();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Examples"))
			{
				//ImGui::MenuItem("Main menu bar", NULL, &show_app_main_menu_bar);
				//ImGui::MenuItem("Console", NULL, &show_app_console);
				//ImGui::MenuItem("Log", NULL, &show_app_log);
				//ImGui::MenuItem("Simple layout", NULL, &show_app_layout);
				//ImGui::MenuItem("Property editor", NULL, &show_app_property_editor);
				//ImGui::MenuItem("Long text display", NULL, &show_app_long_text);
				//ImGui::MenuItem("Auto-resizing window", NULL, &show_app_auto_resize);
				//ImGui::MenuItem("Constrained-resizing window", NULL, &show_app_constrained_resize);
				//ImGui::MenuItem("Simple overlay", NULL, &show_app_simple_overlay);
				//ImGui::MenuItem("Manipulating window titles", NULL, &show_app_window_titles);
				//ImGui::MenuItem("Custom rendering", NULL, &show_app_custom_rendering);
				//ImGui::MenuItem("Documents", NULL, &show_app_documents);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Tools"))
			{
				//ImGui::MenuItem("Metrics", NULL, &show_app_metrics);
				//ImGui::MenuItem("Style Editor", NULL, &show_app_style_editor);
				ImGui::MenuItem("About Dear ImGui", NULL, &show_app_about);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		/**************************/
		// Window options
		/**************************/
		if (ImGui::CollapsingHeader("Window options"))
		{
			ImGui::Checkbox("No titlebar", &no_titlebar); ImGui::SameLine(150);
			ImGui::Checkbox("No scrollbar", &no_scrollbar); ImGui::SameLine(300);
			ImGui::Checkbox("No menu", &no_menu);
			ImGui::Checkbox("No move", &no_move); ImGui::SameLine(150);
			ImGui::Checkbox("No resize", &no_resize); ImGui::SameLine(300);
			ImGui::Checkbox("No collapse", &no_collapse);
			ImGui::Checkbox("No close", &no_close); ImGui::SameLine(150);
			ImGui::Checkbox("No nav", &no_nav); ImGui::SameLine(300);
			ImGui::Checkbox("No background", &no_background);
			ImGui::Checkbox("No bring to front", &no_bring_to_front);
		}

		/**************************/
		// Functional options
		/**************************/
		static bool show_tiled_forward_rendering = false;
		static bool show_normal_rendering = false;
		static bool show_deferred_rendering_ = false;
		if (ImGui::CollapsingHeader("Rendering Technique"))
		{
			static int rendering_technique = 2;
			ImGui::RadioButton("Normal Rendering", &rendering_technique, 0); //ImGui::SameLine();
			ImGui::RadioButton("Deferred Rendering", &rendering_technique, 1); //ImGui::SameLine();
			ImGui::RadioButton("Tiled Forward Rendering", &rendering_technique, 2);

			switch (rendering_technique)
			{
			case 0:
			{
				// TODO
				show_tiled_forward_rendering = false;
				show_normal_rendering = true;
				show_deferred_rendering_ = false;
				break;
			}
			case 1:
			{
				// TODO
				show_tiled_forward_rendering = false;
				show_normal_rendering = false;
				show_deferred_rendering_ = true;
				break;
			}
			case 2:
			{
				// TODO: some intialization to the main program
				show_tiled_forward_rendering = true;
				show_normal_rendering = false;
				show_deferred_rendering_ = false;
				break;
			}
			}
		}

		/**************************/
		// Tiled Forward rendering 
		/**************************/
		if (show_tiled_forward_rendering) ShowForwardPlusWidgets();

		// End of ShowDemoWindow()
		ImGui::End();
	}

	//
	// Customized Functions
	//
	void ShowForwardPlusWidgets()
	{
		if (ImGui::CollapsingHeader("Tiled Forward Rendering"))
		{
			return;
		}

		if (ImGui::TreeNode("Grid Frustum Pass" ))
		{
			ImGui::Text("This is something");
			auto appPtr = reinterpret_cast<TiledRendering*>(g_appPtr);
			// Checkout the tutorial https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples
			ImGui::Image((ImTextureID)appPtr->testHandle.ptr, ImVec2(80, 45));
			ImGui::TreePop();
		}
	}

	//
	// Helper Functions
	//
	void ShowMainMenuFile()
	{
		ImGui::MenuItem("(dummy menu)", NULL, false, false);
		if (ImGui::MenuItem("New")) {}
		if (ImGui::MenuItem("Open", "Ctrl+O")) {}
		if (ImGui::BeginMenu("Open Recent"))
		{
			ImGui::MenuItem("fish_hat.c");
			ImGui::MenuItem("fish_hat.inl");
			ImGui::MenuItem("fish_hat.h");
			if (ImGui::BeginMenu("More.."))
			{
				ImGui::MenuItem("Hello");
				ImGui::MenuItem("Sailor");
				if (ImGui::BeginMenu("Recurse.."))
				{
					ShowMainMenuFile();
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Save", "Ctrl+S")) {}
		if (ImGui::MenuItem("Save As..")) {}

		ImGui::Separator();
		if (ImGui::BeginMenu("Options"))
		{
			static bool enabled = true;
			ImGui::MenuItem("Enabled", "", &enabled);
			ImGui::BeginChild("child", ImVec2(0, 60), true);
			for (int i = 0; i < 10; i++)
				ImGui::Text("Scrolling Text %d", i);
			ImGui::EndChild();
			static float f = 0.5f;
			static int n = 0;
			ImGui::SliderFloat("Value", &f, 0.0f, 1.0f);
			ImGui::InputFloat("Input", &f, 0.1f);
			ImGui::Combo("Combo", &n, "Yes\0No\0Maybe\0\0");
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Colors"))
		{
			float sz = ImGui::GetTextLineHeight();
			for (int i = 0; i < ImGuiCol_COUNT; i++)
			{
				const char* name = ImGui::GetStyleColorName((ImGuiCol)i);
				ImVec2 p = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), ImGui::GetColorU32((ImGuiCol)i));
				ImGui::Dummy(ImVec2(sz, sz));
				ImGui::SameLine();
				ImGui::MenuItem(name);
			}
			ImGui::EndMenu();
		}

		// Here we demonstrate appending again to the "Options" menu (which we already created above)
		// Of course in this demo it is a little bit silly that this function calls BeginMenu("Options") twice.
		// In a real code-base using it would make senses to use this feature from very different code locations.
		if (ImGui::BeginMenu("Options")) // <-- Append!
		{
			static bool b = true;
			ImGui::Checkbox("SomeOption", &b);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Disabled", false)) // Disabled
		{
			IM_ASSERT(0);
		}
		if (ImGui::MenuItem("Checked", NULL, true)) {}
		if (ImGui::MenuItem("Quit", "Alt+F4")) {}
	}

	void ShowAboutWindow(bool* p_open)
	{
		if (!ImGui::Begin("About", p_open, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::End();
			return;
		}
		ImGui::Text("This Demo is developed by Zhihang Deng");
		ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
		ImGui::Separator();
		ImGui::Text("By Omar Cornut and all Dear ImGui contributors.");
		ImGui::Text("Dear ImGui is licensed under the MIT License, see LICENSE for more information.");


		ImGui::End();
	}



}
