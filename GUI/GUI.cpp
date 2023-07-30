#define GLFW_EXPOSE_NATIVE_WIN32
#define RAPIDJSON_ASSERT(x) if(!(x)) throw std::exception(#x);
#define RAPIDJSON_HAS_CXX11_NOEXCEPT 0
#include "GUI.h"
#include "ConverterInterface.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Icons/file_icon_data.h"
#include "Icons/folder_icon_data.h"
#include "Icons/play_icon_data.h"
#include "Icons/play_file_icon_data.h"
#include "Icons/play_muted_icon_data.h"
#include "Icons/play_unmuted_icon_data.h"
#include "Icons/copy_icon_data.h"
#include "Icons/visibility_icon_data.h"
#include "Icons/save_icon_data.h"
#include "FileIOUtils.h"
#include "Process.h"
#include "Audio.h"
#include "ConverterInterface.h"
#include <filesystem>
#include <format>
#include <thread>
#include "assimp/../../contrib/rapidjson/include/rapidjson/document.h"
#include "assimp/../../contrib/rapidjson/include/rapidjson/istreamwrapper.h"
#include "assimp/../../contrib/rapidjson/include/rapidjson/writer.h"
#include "assimp/../../contrib/rapidjson/include/rapidjson/stringbuffer.h"
#include "assimp/../../contrib/rapidjson/include/rapidjson/ostreamwrapper.h"
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include <sstream>
#include <thread>
#include <iostream>
#include "Icons/app_icon16_data.h"
#include "Icons/app_icon32_data.h"
#include "Icons/app_icon48_data.h"

PathHistory GUI::history;
ctpl::thread_pool GUI::thread_pool;
Logs GUI::logs;
std::recursive_mutex GUI::log_mutex;
std::mutex GUI::mute_mutex;

namespace
{
	void GLAPIENTRY opengl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	{
		if (type == GL_DEBUG_TYPE_ERROR)
		{
			fprintf(stderr, "GL ERROR: type = 0x%x, severity = 0x%x, message = %s\n", type, severity, message);
		}
	}

	inline constexpr int64_t mod(int64_t a, int64_t b)
	{
		return (a % b + b) % b;
	}

	int(*text_history_callback[3])(ImGuiInputTextCallbackData*) {
		[](ImGuiInputTextCallbackData* data) { return GUI::history.scroll_callback(data, ::PathID_ORIG); },
		[](ImGuiInputTextCallbackData* data) { return GUI::history.scroll_callback(data, ::PathID_INTR); },
		[](ImGuiInputTextCallbackData* data) { return GUI::history.scroll_callback(data, ::PathID_MOD); }
	};

	bool compare(const std::string& a, const std::string& b)
	{
		if (strlen(a.c_str()) != strlen(b.c_str()))
		{
			return false;
		}
		for (int i = 0; i < a.size(); ++i)
		{
			if (std::tolower(a[i]) != std::tolower(b[i]))
			{
				return false;
			}
		}

		return true;
	}

	void open_natively(const std::string& path)
	{
		try
		{
			std::string parent = std::filesystem::path(path).parent_path().string();
			std::replace(parent.begin(), parent.end(), '/', '\\');
			ShellExecuteA(NULL, "open", ("\"" + parent + "\"").c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}
		catch (const std::exception&)
		{
			ShellExecuteA(NULL, "open", ("\"" + path + "\"").c_str(), NULL, NULL, SW_SHOWDEFAULT);
		}
	}
}

GUI::GUI(): browser(*this) {}

void ErrorExit(const char* lpszFunction)
{
	LPVOID lpMsgBuf = nullptr;
	LPVOID lpDisplayBuf = nullptr;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(1);
}

bool GUI::init(GUITask task, std::string pipe_name)
{
	this->task = task;

	glfwSetErrorCallback([](int error, const char* description)
	{
		gbl_err << "Glfw Error " << error << ": " << description << std::endl;
	});
	if (glfwInit() == false)
	{
		gbl_err << "failed to initialise GLFW " << std::endl;
		return false;
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	GUIMessagePayload::WindowInit init;
	if (!pipe_name.empty())
	{
		if (!pipe.open(pipe_name))
		{
			glfwTerminate();
			gbl_err << "Failed to open pipe" << std::endl;
			return false;
		}

		GUIMessage message;
		if (!pipe.read(message))
		{
			gbl_err << "Failed to read pipe message" << std::endl;
			glfwTerminate();
			return false;
		}
		if (message != GUIMessage::WindowInit)
		{
			gbl_err << "Initial pipe message is not GUIMessage::Init" << std::endl;
			glfwTerminate();
			return false;
		}
		if (!pipe.read(init))
		{
			gbl_err << "Failed to read Init payload from pipe" << std::endl;
			glfwTerminate();
			return false;
		}

		for (size_t i = 0; i < _countof(init.hints); ++i)
		{
			if (init.hints[i].first == 0) break;
			glfwWindowHint(init.hints[i].first, init.hints[i].second);
		}
	}
	else
	{
		strcpy_s(init.title, init.title_max_len, "FFXII Asset Converter ver." CONVERTER_VERSION);
		init.width = 1280;
		init.height = 720;

		constexpr char window_mutex_name[] = "ConverterGUI_ba7be3b8-6fab-4656-8911-5bbe703069f9";

		//to prevent multiple instances
		window_mutex = CreateMutex(NULL, TRUE, window_mutex_name);
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if (HWND already_opened_window = FindWindowExA(nullptr, nullptr, nullptr, init.title))
			{
				SetForegroundWindow(already_opened_window);
			}
			return false;
		}
	}

	if ((CoInitialize(NULL) != S_OK)) //for audio management
	{
		gbl_err << "Failed to initialise COM library" << std::endl;
		return false;
	}

	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	window = glfwCreateWindow(init.width, init.height, init.title, nullptr, nullptr);
	if (window == NULL)
	{
		gbl_err << "Unable to create GLFW window " << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		CoUninitialize();
	}

	if (init.parent_window != NULL)
	{
		SetParent(glfwGetWin32Window(window), init.parent_window);
	}
	else
	{
		GLFWimage app_icons[] =
		{
			GLFWimage(app_icon16_data[0], app_icon16_data[1], (unsigned char*)&app_icon16_data[2]),
			GLFWimage(app_icon32_data[0], app_icon32_data[1], (unsigned char*)&app_icon32_data[2]),
			GLFWimage(app_icon48_data[0], app_icon48_data[1], (unsigned char*)&app_icon48_data[2])
		};
		glfwSetWindowIcon(window, 3, app_icons);
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (init.pos_x > 0 && init.pos_y > 0)
	{
		glfwSetWindowPos(window, init.pos_x, init.pos_y);
	}
	glfwShowWindow(window);
	glfwDefaultWindowHints();

	if (!gladLoadGL())
	{
		gbl_err << "Unable to initialize GLAD " << std::endl;
		glfwDestroyWindow(window);
		glfwTerminate();
		CoUninitialize();
		return false;
	}
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(opengl_message_callback, 0);
	
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImGui::GetStyle().Colors[ImGuiCol_Button].x *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].y *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].z *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].w = 1.0f;
	ImGui::GetStyle().AntiAliasedLines = false;

	load_icon_textures();

	switch (task)
	{
	case GUITask::Main:
	{
		ImGui::GetIO().IniFilename = "main_ui.ini";
		load_config();
		history.load();

		if (!browser.init(glfwGetWin32Window(window)))
		{
			gbl_err << "Failed to initialise file browser" << std::endl;
			return false;
		}

		GUILOG(Log_StartInfo, "After inputting the requisite file paths, click \"Pack\" or \"Unpack\" to execute processing.");
		GUILOG(Log_StartInfo, "If any errors are encountered, please report them, and revert to the command-line version of this program in the meantime.");

		std::thread mute_thread_tmp([this]()
		{
			while (!exiting)
			{
				mute_mutex.lock();
				if (play_muted)
				{
					Audio::SetMute(IO::FileName(config_path), true);
				}
				mute_mutex.unlock();
				Sleep(1000);
			}
		});
		mute_thread.swap(mute_thread_tmp);

		break;
	}
	default:
		ImGui::GetIO().IniFilename = nullptr;
	}

	return true;
}

void GUI::run()
{
	// Main loop
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);

		switch (task)
		{
		case GUITask::Main:			draw_main_ui(); break;
		case GUITask::Confirmation:	draw_confirmation_ui(); break;
		case GUITask::Progress:		draw_progress_ui(); break;
		}

		ImGui::End();

		ImGui::Render();
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
}

void GUI::free()
{
	exiting = true;

	if (pipe.opened())
	{
		if (last_post == GUIMessage::Request)
		{
			//this exists to prevent a write deadlocks
			do
			{
				char buffer[1024];
				if (!pipe.read(buffer, sizeof(buffer)))
				{
					break;
				}
			} while (pipe.peek() > 0);
		}
		pipe.write(last_post = GUIMessage::Exit);
		pipe.close();
	}

	if (task == GUITask::Main)
	{
		history.save();
		browser.free();
	}

	thread_pool.stop();

	free_icon_textures();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	window = nullptr;

	if (mute_thread.joinable()) mute_thread.join();

	CoUninitialize();

	if (window_mutex != NULL)
	{
		ReleaseMutex(window_mutex);
		CloseHandle(window_mutex);
	}

	exiting = false;
}

void GUI::draw_main_ui()
{
	if (draw_file_dialog())
	{
		return;
	}

#ifndef NDEBUG
	draw_debug_widgets();
#endif

	ImGui::Spacing();

	bool input_disabled = processing;
	if (input_disabled) ImGui::BeginDisabled();

	draw_input_path("Original Asset/Folder", history.current[PathID_ORIG], PathID_ORIG);
	if (draw_processing_button("Unpack"))
	{
		GUILOG(Log_Info, std::string(20, '-'));
		run_command(false , true);
		history.push(history.current[PathID_ORIG], PathID_ORIG);
		history.push(history.current[PathID_INTR], PathID_INTR);
		history.save();
	}
	draw_input_path("Intermediary Asset/Folder", history.current[PathID_INTR], PathID_INTR);
	if (draw_processing_button("Pack"))
	{
		GUILOG(Log_Info, std::string(20, '-'));
		run_command(true, false);
		history.push(history.current[PathID_ORIG], PathID_ORIG);
		history.push(history.current[PathID_INTR], PathID_INTR);
		history.push(history.current[PathID_MOD], PathID_MOD);
		history.save();
	}
	draw_input_path("Mod Output Asset/Folder", history.current[PathID_MOD], PathID_MOD);

	ImGui::SameLine();
	draw_play_buttons();

	if (input_disabled) ImGui::EndDisabled();

	ImGui::Spacing();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::BeginChild("BottomPane", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar())
	{
		ImVec2 button_size = ImVec2(14.0f, 14.0f);
		float item_spacing = ImGui::GetStyle().ItemSpacing.x;
		float item_padding = ImGui::GetStyle().ItemInnerSpacing.x;

		ImGui::Text("Logs");
		ImGui::SameLine();

		ImGui::SetCursorPosX(
			ImGui::GetCursorPosX() + ImGui::GetContentRegionMax().x - 38.0f -
			((button_size.x + item_padding + item_spacing) * 2.0f)
		);

		ImVec2 v_min = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 2.0f);
		ImVec2 v_max = ImVec2(v_min.x + button_size.x, v_min.y + button_size.y);
		ImGui::GetForegroundDrawList()->AddImage(visibility_icon, v_min, v_max, ImVec2(0, 0), ImVec2(1, 1));

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::SetNextItemWidth(button_size.x);
		if (ImGui::BeginMenu("A##visiblity_options"))
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			if (ImGui::MenuItem("Warnings", NULL, &warnings_visible))
			{
				save_config();
			}
			ImGui::PopStyleColor();

			ImGui::EndMenu();
		}
		ImGui::PopStyleColor();

		static float copy_display_timeout = 0.0f;

		if (ImGui::ImageButton(copy_icon, button_size))
		{
			copy_display_timeout = 3.0f;

			std::string log_sum;
			for (auto& log : logs.data)
			{
				if (log.type != Log_StartInfo && (warnings_visible || (log.type != Log_Warn && log.type != Log_ConvWarn)))
				{
					log_sum += log;
				}
			}

			glfwSetClipboardString(window, log_sum.c_str());
		}
		if (ImGui::IsItemHovered()) 
		{
			if (copy_display_timeout > 0.0f)
			{
				copy_display_timeout -= ImGui::GetIO().DeltaTime;
				ImGui::BeginTooltip();
				ImGui::Text("Copied!");
				ImGui::EndTooltip();
			}
		}
		else copy_display_timeout = 0.0f;

		ImGui::EndMenuBar();
	}
	
	log_mutex.lock();
	for (auto& log : logs.data)
	{
		if (log.type < Log_COUNT && log.type >= 0) 
		{
			ImGui::PushStyleColor(ImGuiCol_Text, LogColours[log.type]);
		}
		else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); 

		if (warnings_visible || (log.type != Log_Warn && log.type != Log_ConvWarn))
		{
			ImGui::TextWrapped(log.c_str());
		}

		ImGui::PopStyleColor();
	}
	log_mutex.unlock();

	if (processing) //show progress indicator
	{
		constexpr const char* indicator = "***********";
		uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		uint64_t index = (time % 1000) / (1000 / 10);

		ImGui::PushStyleColor(ImGuiCol_Text, LogColours[Log_Info]);
		ImGui::TextEx(indicator, indicator + index + 1);
		ImGui::PopStyleColor();
	}

	if (logs.scroll_log)
	{
		ImGui::SetScrollHereY();
		logs.scroll_log = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleColor(2);
}

void GUI::draw_confirmation_ui()
{
	static GUIMessagePayload::Confirmation confirmation;
	static GUIMessage message = GUIMessage::INVALID;

	if (message == GUIMessage::INVALID)
	{
		if (!pipe.read(message))
		{
			gbl_log << "Failed to read pipe message" << std::endl;
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			pipe.close();
			return;
		}
		if (message != GUIMessage::Confirmation)
		{
			gbl_log << "Unexpected message recieved" << std::endl;
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			pipe.write(last_post = GUIMessage::Exit);
			return;
		}
		if (!pipe.read(confirmation))
		{
			gbl_log << "Failed to read pipe message payload" << std::endl;
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			pipe.close();
			return;
		}
		ImGui::GetStyle().Colors[ImGuiCol_Border].w = 1.0f;
	}

	ImRect inner_border(ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight()));
	inner_border.Expand(ImVec2(-4.0f, -4.0f));

	ImVec4 border_colour = ImGui::GetStyle().Colors[ImGuiCol_Border];
	ImGui::GetWindowDrawList()->AddRect(inner_border.Min, inner_border.Max, ImColor(border_colour), 0.0f, 0, 1.0f);

	ImGui::Spacing();
	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(confirmation.label).x) * 0.5f);
	ImGui::Text(confirmation.label);
	ImGui::Spacing();

	ImGui::Separator();
	
	constexpr float button_width = 100.0f;

	uint32_t button_count = 0;
	GUIMessage button_types[] = {GUIMessage::Yes, GUIMessage::No, GUIMessage::Cancel};

	for (auto type : button_types)
	{
		button_count += ((uint32_t)confirmation.button_types & (uint32_t)type) ? 1 : 0;
	}

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - (button_count * button_width + (button_count - 1) * ImGui::GetStyle().ItemSpacing.x)) / 2.0f);

	for (auto type : button_types)
	{
		if ((uint32_t)confirmation.button_types & (uint32_t)type)
		{
			if (type == GUIMessage::Yes) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

			if (ImGui::Button(EnumToString(type), ImVec2(button_width, 0)))
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				pipe.write(last_post = type);
			}
			ImGui::SameLine();

			if (type == GUIMessage::Yes) ImGui::PopStyleVar();
		}
	}
}

void GUI::draw_progress_ui()
{
	static GUIMessagePayload::Progress progress;
	static bool made_first_request = false;

	if (made_first_request == false)
	{
		pipe.write(last_post = GUIMessage::Request);
		made_first_request = true;
	}

	if (pipe.peek() > 0)
	{
		GUIMessage message;
		if(!pipe.read(message))
		{
			gbl_log << "Failed to read pipe message" << std::endl;
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			pipe.close();
			return;
		}
		if (message == GUIMessage::Exit)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			return;
		}
		else if (message == GUIMessage::Progress)
		{
			if (!pipe.read(progress))
			{
				gbl_log << "Failed to read pipe message payload" << std::endl;
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				pipe.close();
				return;
			}
			pipe.write(last_post = GUIMessage::Request);
		}
	}

	std::vector<std::string> label_segments = IO::Split(progress.label, "\n");
	for (std::string& seg : label_segments)
	{
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize(seg.c_str()).x) * 0.5f);
		ImGui::Text(seg.c_str());
	}

	float bar_width = ImGui::GetWindowSize().x - ImGui::GetStyle().FramePadding.x * 2.0f;
	float elapsed = float(progress.elpased) / float(progress.destination);
	std::string overlay = std::to_string(progress.elpased) + "/" + std::to_string(progress.destination);

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - bar_width) * 0.5f);
	ImGui::ProgressBar(elapsed, ImVec2(bar_width, 0), overlay.c_str());

	float button_width = 100.0f;

	if (progress.cancel_disabled) ImGui::BeginDisabled();

	ImGui::SetCursorPosX((ImGui::GetWindowSize().x - button_width) * 0.5f);
	if (ImGui::Button("Cancel", ImVec2(button_width, 0)))
	{
		pipe.write(last_post = GUIMessage::Exit);
	}

	if (progress.cancel_disabled) ImGui::EndDisabled();
}

void GUI::draw_debug_widgets()
{
	ImGui::Separator();

	if (ImGui::Button(convert_via_cmd_interface ? "Converting via CMD Interface" : "Converting via GUI Interface"))
	{
		convert_via_cmd_interface ^= true;
		save_config();
	}

	ImGui::Separator();
}

GUIMessage GUI::confirm(const std::string& label, GUIMessage button_types)
{
	PipeServer pipe;
	HANDLE confirm_proc = INVALID_HANDLE_VALUE;
	GUIMessagePayload::WindowInit init;

	int w = 0, h = 0;
	glfwGetWindowSize(window, &w, &h);

	init.width = w / 3;
	init.height = 70;
	init.pos_x = (w - init.width) / 2;
	init.pos_y = (h - init.height) / 2;
	init.parent_window = glfwGetWin32Window(window);

	init.hints[0] = { GLFW_DECORATED, GLFW_FALSE };
	init.hints[1] = { GLFW_FLOATING, GLFW_TRUE };

	strcpy_s(init.title, init.title_max_len, "Confirmation");

	if (!pipe.create("Confirmation" + std::to_string(GetCurrentProcessId())))
	{
		GUILOG(Log_Error, "Failed to create confirmation box pipe");
		return GUIMessage::INVALID;
	}
	confirm_proc = Process::RunGUIProcess(GUITask::Confirmation, pipe.name);
	if (confirm_proc == INVALID_HANDLE_VALUE)
	{
		GUILOG(Log_Error, "Failed to create confirmation box process");
		return GUIMessage::INVALID;
	}
	if (!pipe.connect())
	{
		GUILOG(Log_Error, "Failed to connect confirmation box pipe");
		Process::TerminateGUIProcess(confirm_proc);
		return GUIMessage::INVALID;
	}
	if (!pipe.write(GUIMessage::WindowInit) || !pipe.write(init))
	{
		GUILOG(Log_Error, "Failed to initialise confirmation box pipe");
		Process::TerminateGUIProcess(confirm_proc);
		return GUIMessage::INVALID;
	}

	GUIMessagePayload::Confirmation confirmation;
	confirmation.button_types = button_types;
	strcpy_s(confirmation.label, confirmation.label_max_len, label.c_str());

	pipe.write(GUIMessage::Confirmation);
	pipe.write(confirmation);

	GUIMessage message;
	if (!pipe.read(message))
	{
		GUILOG(Log_Error, "Failed to read confirmation read pipe message");
		Process::TerminateGUIProcess(confirm_proc);
		return GUIMessage::INVALID;
	}

	Process::TerminateGUIProcess(confirm_proc, 2000);

	return message;
}

bool GUI::confirm(const std::string& label)
{
	GUIMessage message = confirm(label, GUIMessage((uint32_t)GUIMessage::Yes | (uint32_t)GUIMessage::Cancel));
	return message == GUIMessage::Yes;
}

void GUI::alert(const std::string& label)
{
	confirm(label, GUIMessage::Yes);
}

bool GUI::draw_file_dialog()
{
	if (dialog_pathID == PathID_INVALID)
	{
		return false;
	}

	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Browser Window", nullptr, ImGuiWindowFlags_NoDecoration);

	FileBrowser::DialogCode code = browser.run_dialog();

	ImGui::End();

	if (code != FileBrowser::DialogCode_None)
	{
		if (code != FileBrowser::DialogCode_Cancel)
		{
			std::string path = IO::Join(browser.cur_parent_folder) + "/" + browser.cur_filename;
			if (dialog_pathID == PathID_PLAY)
			{
				config_path = path;
				save_config();
			}
			else
			{
				history.push(path, dialog_pathID);
				strcpy_s(history.current[dialog_pathID], history.max_path_size, path.c_str());
			}
		}
		dialog_pathID = PathID_INVALID;
	}

	return true;
}

void GUI::load_config()
{
	config_path = "";
	try
	{
		if (std::filesystem::exists("config.json"))
		{
			std::ifstream stream("config.json");
			rapidjson::IStreamWrapper isw{ stream };
			rapidjson::Document doc;
			doc.ParseStream(isw);

			if (doc.HasMember("play_path"))
			{
				auto path = doc["play_path"].GetString();
				if (std::filesystem::exists(path))
				{
					config_path = path;
				}
			}
			play_muted = doc.HasMember("play_muted") ? doc["play_muted"].GetBool() : play_muted;
			warnings_visible = doc.HasMember("warnings_visible") ? doc["warnings_visible"].GetBool() : warnings_visible;
			convert_via_cmd_interface = doc.HasMember("convert_via_cmd_interface") ? doc["convert_via_cmd_interface"].GetBool() : convert_via_cmd_interface;
		}

	}
	catch (const std::exception&){}
}

void GUI::save_config()
{
	try
	{
		rapidjson::Document doc;
		std::string json = 
			"{ \"play_path\" : \"" + config_path + "\"" + 
			", \"play_muted\" : " + (play_muted ? "true" : "false") +
			", \"warnings_visible\" : " + (warnings_visible ? "true" : "false") +
			", \"warnings_visible\" : " + (warnings_visible ? "true" : "false") +
			", \"convert_via_cmd_interface\" : " + (convert_via_cmd_interface ? "true" : "false") +
			"} ";
		doc.Parse(json.c_str());

		std::ofstream stream("config.json");
		rapidjson::OStreamWrapper osw(stream);

		rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
		doc.Accept(writer);
	}
	catch (const std::exception&) {}
}

void  GUI::load_icon_textures()
{
	file_icon = GUIIcon(file_icon_data, sizeof(file_icon_data));
	folder_icon = GUIIcon(folder_icon_data, sizeof(folder_icon_data));
	play_icon = GUIIcon(play_icon_data, sizeof(play_icon_data));
	play_file_icon = GUIIcon(play_file_icon_data, sizeof(play_file_icon_data));
	play_muted_icon = GUIIcon(play_muted_icon_data, sizeof(play_muted_icon_data));
	play_unmuted_icon = GUIIcon(play_unmuted_icon_data, sizeof(play_unmuted_icon_data));
	visibility_icon = GUIIcon(visibility_icon_data, sizeof(visibility_icon_data));
	copy_icon = GUIIcon(copy_icon_data, sizeof(copy_icon_data));
	save_icon = GUIIcon(save_icon_data, sizeof(save_icon_data));
}

void  GUI::free_icon_textures()
{
	file_icon.free();
	folder_icon.free();
	play_icon.free();
	play_file_icon.free();
	play_muted_icon.free();
	play_unmuted_icon.free();
	visibility_icon.free();
	copy_icon.free();
	save_icon.free();
}

bool GUI::draw_processing_button(std::string name)
{
	float spacing_size = 20.0f;
	float button_width = 100.0f;
	float arrow_head_size = 10.0f;
	float indentation = 2.0f * (spacing_size + 2.0f * ImGui::GetStyle().ItemSpacing.x);
	ImVec2 start_pos = ImVec2(ImGui::GetCursorPos().x + indentation, ImGui::GetCursorPos().y);
	ImVec2 button_pos = ImVec2(start_pos.x, start_pos.y + spacing_size * 0.5f);
	ImVec2 button_size = ImVec2(button_width, spacing_size);
	ImVec2 end_pos = ImVec2(start_pos.x - indentation, start_pos.y + spacing_size * 2.5f);
	ImU32  arrow_colour = (ImU32)(ImColor)ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	ImVec2 arrow_start = ImVec2(start_pos.x + button_width * 0.5f, start_pos.y);
	ImVec2 arrow_end = ImVec2(arrow_start.x, end_pos.y);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

	draw_list->AddLine(arrow_start, ImVec2(arrow_end.x, arrow_end.y - arrow_head_size * 0.5f), arrow_colour, 4.0f);
	draw_list->AddTriangleFilled(
		ImVec2(arrow_end.x - arrow_head_size * 0.75f, arrow_end.y - arrow_head_size),
		ImVec2(arrow_end.x + arrow_head_size * 0.75f, arrow_end.y - arrow_head_size),
		arrow_end,
		arrow_colour
	);

	ImGui::SetCursorPos(button_pos);
	bool clicked = ImGui::Button(name.c_str(), button_size);

	ImGui::SetCursorPos(end_pos);
	ImGui::PopStyleVar();
	return clicked;
}

void GUI::draw_play_buttons()
{
	bool play_disabled = config_path.empty();
	float width = play_file_icon.width * 3.0f + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().ItemInnerSpacing.x * 8.0f;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
	
	if (ImGui::ImageButton(play_file_icon, ImVec2((float)play_file_icon.width, (float)play_file_icon.height)))
	{
		browser.set_current_path(config_path);
		browser.filters = { {"Game EXE (*.exe)", { "exe" }}, { "All Files (*)", {"*"} } };
		browser.require_existing_file = true;
		dialog_pathID = PathID_PLAY;
	}
	ImGui::SameLine();
	if (play_disabled) ImGui::BeginDisabled();
	if (ImGui::ImageButton(play_icon, ImVec2((float)play_icon.width, (float)play_icon.height)))
	{
		if (!std::filesystem::exists(config_path))
		{
			GUILOG(Log_Error, "path does not exist - \"" + config_path + "\"");
			config_path = "";
			save_config();
		}
		else
		{
			std::string parent = std::filesystem::absolute(config_path).parent_path().string();
			std::replace(parent.begin(), parent.end(), '/', '\\');
			ShellExecuteA(NULL, "open", ("\"" + config_path + "\"").c_str(), NULL, parent.c_str(), SW_SHOWDEFAULT);
		}
	}

	GUIIcon& sound_icon = play_muted ? play_muted_icon : play_unmuted_icon;

	ImGui::SameLine();
	if (ImGui::ImageButton(sound_icon, ImVec2((float)sound_icon.width, (float)sound_icon.height)))
	{
		mute_mutex.lock();
		play_muted = !play_muted;
		Audio::SetMute(IO::FileName(config_path), play_muted);
		mute_mutex.unlock();
		save_config();
	}

	if (play_disabled) ImGui::EndDisabled();
}

bool GUI::draw_input_path(const char* label, char* buf, PathID pathID)
{
	float spacing_size = 20.0f;
	ImGui::PushID(label);
	if (ImGui::ImageButton(file_icon, ImVec2(spacing_size, spacing_size)))
	{
		browser.set_current_path(buf);
		if (pathID == PathID_INTR)
		{
			browser.filters = { {"Standard files (*.fbx *.png)", { "fbx", "png" }}, { "All Files (*)", {"*"} } };
		}
		else
		{
			browser.filters = { {"Asset files (*.dae.phyre *.dds.phyre)", { "dae.phyre", "dds.phyre" }}, { "All Files (*)", {"*"} } };
		}
		browser.filter_index = 0;

		if (pathID == PathID_ORIG)
		{
			browser.require_existing_file = true;
			//browser.confirm_overwrite = false;
		}
		else
		{
			browser.require_existing_file = false;
			//browser.confirm_overwrite = true;
		}
		dialog_pathID = pathID;
	}
	ImGui::SameLine();
	if (ImGui::ImageButton(folder_icon, ImVec2(spacing_size, spacing_size)))
	{
		browser.set_current_path(buf);
		browser.filters = {};
		browser.filter_index = 0;

		if (pathID == PathID_ORIG)
		{
			browser.require_existing_file = true;
			//browser.confirm_overwrite = false;
		}
		else
		{
			browser.require_existing_file = false;
			//browser.confirm_overwrite = true;
		}
		dialog_pathID = pathID;
	}
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, spacing_size - ImGui::GetFontSize()));
	std::string hint = "file or folder" + std::string(pathID == 1 ? "(.fbx, .png)" : "(.dae.phyre, .dds.phyre)");
	bool edited = ImGui::InputTextWithHint(label, hint.c_str(), buf, history.max_path_size, ImGuiInputTextFlags_CallbackHistory, text_history_callback[pathID]);
	
	if (std::filesystem::exists(buf))
	{
		if (ImGui::BeginPopupContextItem(("##context" + std::to_string(pathID) + label).c_str()))
		{ 
			if (ImGui::Selectable("Open Folder in Explorer")) open_natively(buf);
			ImGui::EndPopup();
		}
	}
	
	ImGui::PopStyleVar();
	ImGui::PopID();
	return edited;
}

bool GUI::validate_paths(bool pack, bool unpack)
{
	bool processing_folder = false;

	if (std::string(history.current[PathID_ORIG]).contains(".vbf"))
	{
		std::string arc_path, file_name;
		if (!VBFUtils::Separate(history.current[PathID_ORIG], arc_path, file_name))
		{
			GUILOG(Log_Error, "Vbf asset path is invalid");
			return false;
		}
		
		FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
		const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);
		if (!node)
		{
			GUILOG(Log_Error, "Failed to find " + file_name + " in vbf");
			return false;
		}
		processing_folder = !node->children.empty();
	}
	else
	{
		if (!IO::VerifyFileAccessible(history.current[PathID_ORIG]))
		{
			GUILOG(Log_Error, "Original asset path is not accessible");
			return false;
		}

		processing_folder = IO::IsDirectory(history.current[PathID_ORIG]);

		if (!processing_folder && !IO::VerifyHeader(history.current[PathID_ORIG]))
		{
			GUILOG(Log_Error, "the original asset file has an unknown file structure");
			return false;
		}
	}

	if (std::string(history.current[PathID_INTR]).contains(".vbf"))
	{
		GUILOG(Log_Error, "The intermediary asset path cannot be in a vbf");
		return false;
	}

	if (pack && !IO::VerifyFileAccessible(history.current[PathID_INTR]))
	{
		GUILOG(Log_Error, "The intermediary asset path is not accessible");
		return false;
	}

	if (pack && processing_folder && !IO::IsDirectory(history.current[PathID_INTR]))
	{
		GUILOG(Log_Error, "The original asset path is a folder but the intermediary asset path is a file");
		return false;
	}

	if (pack && !processing_folder && IO::IsDirectory(history.current[PathID_INTR]))
	{
		GUILOG(Log_Error, "The original asset path is a file but the intermediary asset path is a folder");
		return false;
	}

	if (unpack && !IO::VerifyParentFolderAccessible(history.current[PathID_INTR]))
	{
		GUILOG(Log_Error, "The replacemnt asset path's parent folder is not accessible");
		return false;
	}

	if (pack) 
	{
		if (std::string(history.current[PathID_MOD]).contains(".vbf"))
		{
			std::string arc_path, file_name;
			if (!VBFUtils::Separate(history.current[PathID_MOD], arc_path, file_name))
			{
				GUILOG(Log_Error, "VBF asset path is invalid");
				return false;
			}

			FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			if (!arc_rec)
			{
				return false;
			}
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);;
			if (!node)
			{
				GUILOG(Log_Error, "Failed to find " + file_name + " in vbf");
				return false;
			}
		}
		else
		{
			if (!IO::VerifyParentFolderAccessible(history.current[PathID_MOD]))
			{
				GUILOG(Log_Error, "The output-mod asset path's parent folder is not accessible");
				return false;
			}

			if (!IO::VerifyDifferent(history.current[PathID_ORIG], history.current[PathID_MOD]))
			{
				GUILOG(Log_Error, "The original and output-mod asset paths must be different");
				return false;
			}
		}
	}

	return true;
}

bool GUI::run_command(bool pack, bool unpack)
{
	namespace fs = std::filesystem;

#ifndef NDEBUG
	if (convert_via_cmd_interface)
	{
		GUILOG(Log_Info, "Converting via CMD interface and logging to console");

		processing = true;

		thread_pool.push([this, pack, unpack](int)
		{
			std::string orig_path = IO::Normalise(history.current[PathID_ORIG]);
			std::string intr_path = IO::Normalise(history.current[PathID_INTR]);
			std::string mod_path = IO::Normalise(history.current[PathID_MOD]);
			char app_path[history.max_path_size] = {};
			DWORD app_path_size = GetModuleFileNameA(nullptr, app_path, history.max_path_size);

			Sleep(1000); //to let path history be saved

			if (pack)
			{
				const char* argv[] = { app_path, "pack", orig_path.c_str(), intr_path.c_str(), mod_path.c_str() };
				ConverterInterface::Run(_countof(argv), argv);
			}
			else
			{
				const char* argv[] = { app_path, "unpack", orig_path.c_str(), intr_path.c_str(), "-r" };
				ConverterInterface::Run(_countof(argv), argv);
			}

			processing = false;
		});		

		return true;
	}
#endif

	std::vector<std::string> potential_vbf_paths = pack ? 
		std::vector<std::string>{ history.current[PathID_ORIG], history.current[PathID_MOD] } : 
		std::vector<std::string>{ history.current[PathID_ORIG]};

	for (const std::string& path : potential_vbf_paths)
	{
		if (std::string(path).contains(".vbf"))
		{
			std::string arc_path, file_name;
			if (!VBFUtils::Separate(path, arc_path, file_name))
			{
				GUILOG(Log_Error, "failed to deduce archive paths for " + path);
				return false;
			}

			auto find = browser.archives.find(arc_path);
			if (find == browser.archives.end())
			{
				processing = true;
				auto find = browser.archives.insert({ arc_path, FileBrowser::ArchiveRecord{} }).first;

				thread_pool.push([this, pack, unpack, arc_path, find](int)
				{
					std::stringstream err_stream;
					std::streambuf* old_cerr = gbl_err.ostream->rdbuf(err_stream.rdbuf());

					GUILOG(Log_Info, "Loading vbf " + arc_path);

					find->second.valid = find->second.archive.load(arc_path);
					find->second.loaded = true;

					if (!find->second.valid)
					{
						gbl_err.ostream->rdbuf(old_cerr);
						GUILOG(Log_Error, err_stream.str());
						GUILOG(Log_Error, "Failed to load vbf " + arc_path);
					}
					gbl_err.ostream->rdbuf(old_cerr);
					processing = false;

					run_command(pack, unpack);
				});
				return false;
			}
			while (!find->second.loaded)
			{
				GUILOG(Log_Info, "Waiting for vbf to load - " + arc_path);
				Sleep(2000);
			}

			if (!find->second.valid)
			{
				GUILOG(Log_Error, "Failed to load vbf previously " + arc_path);
				return false;
			}
		}
	}

	if (!validate_paths(pack, unpack))
	{
		return false;
	}

	std::error_code ec;
	if (unpack && 
		(IO::IsDirectory(history.current[PathID_INTR]) && !std::filesystem::is_empty(history.current[PathID_INTR], ec) ||
		!IO::IsDirectory(history.current[PathID_INTR]) && std::filesystem::exists(history.current[PathID_INTR], ec)	)
	) {
		if (!confirm("Overwrite existing intermediary asset file(s)?"))
		{
			return false;
		}
	}

	processing = true;

	thread_pool.push([this, pack, unpack](int) 
	{
		std::string tmp_src_dir;
		std::string tmp_dst_dir;
		std::string unmodified_orig_path = IO::Normalise(history.current[PathID_ORIG]);
		std::string unmodified_intr_path = IO::Normalise(history.current[PathID_INTR]);
		std::string unmodified_mod_path = IO::Normalise(history.current[PathID_MOD]);
		std::string orig_path = unmodified_orig_path;
		std::string intr_path = unmodified_intr_path;
		std::string mod_path = unmodified_mod_path;

		bool processing_folder = false;

		if (unmodified_orig_path.contains(".vbf"))
		{
			std::string arc_path, file_name;
			VBFUtils::Separate(unmodified_orig_path, arc_path, file_name);
			FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);

			tmp_src_dir = IO::CreateTmpPath("tmp_vbf_src");
			if (tmp_src_dir.empty())
			{
				GUILOG(Log_Error, "Failed to create temp path tmp_vbf_src");
				processing = false;
				return false;
			}

			GUILOG(Log_Info, "Extracting vbf file(s)...");
			
			std::stringstream err_stream;
			std::streambuf* old_cerr = gbl_err.ostream->rdbuf(err_stream.rdbuf());

			if (!arc_rec->archive.extract_all(tmp_src_dir, *node))
			{
				gbl_err.ostream->rdbuf(old_cerr);
				GUILOG(Log_Error, err_stream.str());
				std::filesystem::remove(tmp_src_dir);
				processing = false;
				return false;
			}
			gbl_err.ostream->rdbuf(old_cerr);

			orig_path = tmp_src_dir + (IO::IsDirectory(orig_path) ? "" : "/" + IO::FileName(orig_path));

			processing_folder = !node->children.empty();
		}

		if (pack && mod_path.contains(".vbf"))
		{
			tmp_dst_dir = IO::CreateTmpPath("tmp_vbf_dst");
			if(tmp_dst_dir.empty()) 
			{
				GUILOG(Log_Error, "Failed to create temp path tmp_vbf_dst");
				processing = false;
				return false;
			}
			mod_path = tmp_dst_dir + (IO::IsDirectory(mod_path) ? "" : "/" + IO::FileName(mod_path));
		}

		processing_folder = IO::IsDirectory(orig_path);

		if (processing_folder &&
			unpack && !IO::CopyFolderHierarchy(intr_path, orig_path) ||
			pack && !IO::CopyFolderHierarchy(mod_path, orig_path)
		) {
			GUILOG(Log_Error, "Failed to copy the original asset path's folder hierarchy");
			processing = false;
			return false;
		}

		char app_path[history.max_path_size] = {};
		DWORD app_path_size = GetModuleFileNameA(nullptr, app_path, history.max_path_size);
		std::string process_id_suffix = std::to_string(GetCurrentProcessId());

		if (unpack)
		{
			std::vector<std::array<std::string, 2>> jobs;

			if (!processing_folder)
			{
				jobs.push_back({ IO::Normalise(orig_path), IO::Normalise(intr_path) });
			}
			else
			{
				for (const auto& entry : fs::recursive_directory_iterator(orig_path))
				{
					if (!entry.is_regular_file())
					{
						continue;
					}
					std::string orig_file = entry.path().string();
					std::string intr_file;

					if (IO::GetExtension(orig_file) == "dae.phyre")
					{
						intr_file = std::string(intr_path) + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".fbx").string();
					}
					else if (IO::GetExtension(orig_file) == "dds.phyre")
					{
						intr_file = std::string(intr_path) + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".png").string();
					}
					else
					{
						continue;
					}
					jobs.push_back({ IO::Normalise(orig_file), IO::Normalise(intr_file) });
				}
			}

			if (jobs.empty())
			{
				GUILOG(Log_Error, "No files were found");
				processing = false;
				return false;
			}

			GUILOG(Log_Info, "Finding texture reference(s)...");

			std::vector<std::array<std::string, 3>> ref_unpack_jobs;
			std::map<std::string, std::string> ref_file_unpack_map;

			for (auto& job : jobs)
			{
				if (!job[0].ends_with("dae.phyre"))
				{
					continue;
				}
				std::map<std::string, std::string> ref_name_id_map;
				std::map<std::string, std::string> ref_name_file_map;

				IO::FindLocalReferences(ref_name_id_map, job[0]);
				IO::EvaluateReferencesFromLocalSearch(ref_name_file_map, ref_name_id_map, job[0], "dds.phyre");

				for (auto& name_file : ref_name_file_map)
				{
					std::string orig_rel_root = std::filesystem::relative(name_file.second, fs::path(job[0]).parent_path()).parent_path().string();
					std::string intr_root = fs::absolute(fs::path(job[1]).parent_path().string() + "/" + orig_rel_root).string();
					fs::path intr_file = intr_root + "/" + fs::path(name_file.second).filename().string();

					if (IO::GetExtension(name_file.first) == "dae.phyre")
					{
						intr_root = intr_file.replace_extension().replace_extension(".fbx").string();
					}
					else if (IO::GetExtension(name_file.first) == "dds.phyre")
					{
						intr_root = intr_file.replace_extension().replace_extension(".png").string();
					}

					ref_file_unpack_map[name_file.second] = IO::Normalise(intr_file.string());
				}
			}

			if (ref_file_unpack_map.empty())
			{
				GUILOG(Log_Warn, "Failed to find texture reference(s)");
			}

			for (const auto& file_unpack : ref_file_unpack_map)
			{
				std::string orig_file = IO::Normalise(file_unpack.first);
				auto job_it = jobs.end();

				for (auto it = jobs.begin(); it != jobs.end(); ++it)
				{
					if (it->at(0) == orig_file)
					{
						job_it = it;
						break;
					}
				}

				if (job_it != jobs.end() || !fs::exists(file_unpack.second))
				{
					fs::create_directories(file_unpack.second.substr(0, file_unpack.second.find_last_of("/")));

					ref_unpack_jobs.push_back({ file_unpack.first, file_unpack.second });
				}

				if (job_it != jobs.end())
				{
					jobs.erase(job_it);
				}
			}

			if (ref_unpack_jobs.empty())
			{

			}

			if (!ref_unpack_jobs.empty())
			{
				GUILOG(Log_Info, "Unpacking texture reference(s)...");

				std::vector<bool> ref_tasks_completed(ref_unpack_jobs.size(), false);

				for (int i = 0; i < ref_unpack_jobs.size(); ++i)
				{
					std::string id = std::to_string(i) + std::format("_{:#x}_", (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()) + process_id_suffix;
					std::string cmd = std::string(app_path) + " unpack \"" + ref_unpack_jobs[i][0] + "\" \"" + ref_unpack_jobs[i][1] + "\"";

					thread_pool.push([this, cmd, id, i, &ref_tasks_completed, &ref_unpack_jobs](int)
					{
						std::string out, warn, err;
						int err_code = Process::RunConvertProcess(cmd, out, warn, err, id);

						log_mutex.lock();
						if (err_code != 0 && err.empty())	GUILOG(Log_ConvError, "An unhandled error caused unpacking " + ref_unpack_jobs[i][0] + " to fail")
						if (!err.empty())					GUILOG(Log_ConvError, err);
						if (!warn.empty())					GUILOG(Log_ConvWarn, warn);
						if (err_code == 0 && !out.empty())	GUILOG(Log_ConvInfo, out);
						log_mutex.unlock();
						
						ref_tasks_completed[i] = true;
					});
				}

				while (!std::all_of(ref_tasks_completed.begin(), ref_tasks_completed.end(), [](bool v) { return v; }))
				{
					Sleep(250);
				}
			}

			GUILOG(Log_Info, "Unpacking file(s)...");

			std::vector<bool> tasks_completed(jobs.size(), false);

			for (int i = 0; i < jobs.size(); ++i)
			{
				std::string id = std::to_string(i) + std::format("_{:#x}_", (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()) + process_id_suffix;
				std::string cmd = std::string(app_path) + " unpack \"" + jobs[i][0] + "\" \"" + jobs[i][1] + "\"";
				
				thread_pool.push([this, cmd, id, i, &tasks_completed, &jobs](int)
				{
					std::string out, warn, err;
					int err_code = Process::RunConvertProcess(cmd, out, warn, err, id);
					
					log_mutex.lock();
					if (err_code != 0 && err.empty())	GUILOG(Log_ConvError, "An unhandled error caused unpacking " + jobs[i][0] + " to fail")
					if (!err.empty())					GUILOG(Log_ConvError, err);
					if (!warn.empty())					GUILOG(Log_ConvWarn, warn);
					if (err_code == 0 && !out.empty())	GUILOG(Log_ConvInfo, out);
					log_mutex.unlock();

					tasks_completed[i] = true;
				});
			}

			while (!std::all_of(tasks_completed.begin(), tasks_completed.end(), [](bool v) { return v; }))
			{
				Sleep(250);
			}
		}
		else if (pack)
		{
			std::vector<std::array<std::string, 3>> jobs;

			if (!processing_folder)
			{
				jobs.push_back({ IO::Normalise(orig_path), IO::Normalise(intr_path), IO::Normalise(mod_path) });
			}
			else
			{
				for (const auto& entry : fs::recursive_directory_iterator(intr_path))
				{
					if (!entry.is_regular_file())
					{
						continue;
					}
					std::string intr_file = entry.path().string();
					std::string extensions[] = { "dae.phyre", "dds.phyre" };

					for (const std::string& ext : extensions)
					{
						std::string orig_file = std::string(orig_path) + "/" + fs::relative(intr_file, intr_path).replace_extension(ext).string();
						if (!fs::exists(orig_file))
						{
							continue;
						}
						std::string out_file = std::string(mod_path) + "/" + fs::relative(orig_file, orig_path).string();

						jobs.push_back({ IO::Normalise(orig_file), IO::Normalise(intr_file), IO::Normalise(out_file) });
					}
				}
			}

			if (jobs.empty())
			{
				GUILOG(Log_Error, "No files were found");
				processing = false;
				return false;
			}

			GUILOG(Log_Info, "Packing file(s)...");

			std::vector<bool> tasks_completed(jobs.size(), false);

			for (int i = 0; i < jobs.size(); ++i)
			{
				std::string id = std::to_string(i) + std::format("_{:#x}_", (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()) + process_id_suffix;
				std::string cmd = std::string(app_path) + " pack \"" + jobs[i][0] + "\" \"" + jobs[i][1] + "\" \"" + jobs[i][2] + "\"";
				
				thread_pool.push([this, cmd, id, i, &tasks_completed, &jobs](int)
				{
					std::string out, warn, err;
					int err_code = Process::RunConvertProcess(cmd, out, warn, err, id);
					
					log_mutex.lock();
					if (err_code != 0 && err.empty())	GUILOG(Log_ConvError, "An unhandled error caused packing " + jobs[i][0] + " to fail")
					if (!err.empty())					GUILOG(Log_ConvError, err);
					if (!warn.empty())					GUILOG(Log_ConvWarn, warn);
					if (err_code == 0 && !out.empty())	GUILOG(Log_ConvInfo, out);
					log_mutex.unlock();

					tasks_completed[i] = true;
				});
			}

			while (!std::all_of(tasks_completed.begin(), tasks_completed.end(), [](bool v) { return v; }))
			{
				Sleep(250);
			}
		}

		if (tmp_dst_dir.size())
		{
			std::string arc_path, file_name;
			VBFUtils::Separate(unmodified_mod_path, arc_path, file_name);
			FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);

			std::stringstream err_stream;
			std::streambuf* old_cerr = gbl_err.ostream->rdbuf(err_stream.rdbuf());

			GUILOG(Log_Info, "Injecting files into vbf - " + arc_path);

			if (!arc_rec->archive.inject_all(tmp_dst_dir, *node))
			{
				gbl_err.ostream->rdbuf(old_cerr);
				GUILOG(Log_Error, err_stream.str());
				GUILOG(Log_Error, "Failed to inject into " + arc_path);
				std::filesystem::remove_all(tmp_dst_dir);
				processing = false;
				return false;
			}
			if (!arc_rec->archive.update_header())
			{
				gbl_err.ostream->rdbuf(old_cerr);
				GUILOG(Log_Error, err_stream.str());
				GUILOG(Log_Error, "Failed to update vbf header of " + arc_path);
				std::filesystem::remove_all(tmp_dst_dir);
				processing = false;
				return false;
			}
			gbl_err.ostream->rdbuf(old_cerr);
			std::filesystem::remove_all(tmp_dst_dir);
			tmp_dst_dir = "";
		}
		if (tmp_src_dir.size())
		{
			std::filesystem::remove_all(tmp_src_dir);
			tmp_src_dir = "";
		}

		processing = false;
		GUILOG(Log_Info, "Processing Complete.");

		return true;
	});

	return true;
}