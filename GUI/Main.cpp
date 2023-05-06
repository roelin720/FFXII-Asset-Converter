#include <iostream>

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <map>
#include "assimp/../../contrib/stb/stb_image.h"
#include "Icons/file_white.h"
#include "Icons/folder_white.h"
#include "Icons/file_white.h"
#include "portable-file-dialogs.h"
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include "PhyreIOUtils.h"
#include <iomanip>
#include <ctime>
#include <sstream>
#include "ctpl_stl.h"
#include "Process.h"
#include "PhyreInterface.h"
#include <fstream>
#include <io.h>

constexpr int max_path_size = 2048;
constexpr int thread_count_max = 6;

std::mutex mutex;
ctpl::thread_pool thread_pool(thread_count_max);
bool print_completion = false;

enum LogType 
{
	InfoLog = 1,
	ConverterInfoLog = 1 | 2,
	ErrorLog = 4,
	ConverterErrorLog = 4 | 8
};

struct Log : public std::string
{
	std::string datetime()
	{
		std::stringstream sstream;
		tm _tm = {};
		time_t time = std::time(nullptr);
		localtime_s(&_tm , &time);
		sstream << std::put_time(&_tm, "%d-%m-%Y %H-%M-%S");
		return sstream.str();
	}

	LogType type;
	Log(LogType type, const std::string& msg) : type(type) 
	{
		std::stringstream ss(msg);
		std::string s;

		if (!msg.empty())
		{
			for (std::string line; std::getline(ss, line, '\n');) 
			{
				s += (s.empty() ? datetime() + ": " : "                   : ") + line + "\n";
			}
		}
		assign(s);
	}
};

uint32_t folder_icon = 0;
uint32_t file_icon = 0;

uint32_t log_buffer_max = 2048;
std::list<Log> logs;
bool scroll_log = false;

uint32_t history_max = 128;
std::array<std::vector<std::string>, 3> history;

char orig_path[max_path_size] = {};
char rep_path[max_path_size] = {};
char out_path[max_path_size] = {};
char* paths[3] = {orig_path, rep_path, out_path};
int64_t history_pos[3] = { -1, -1, -1 };

constexpr auto mod = [](int64_t a, int64_t b) { return (a % b + b) % b; };

void push_log(const Log& log)
{
	logs.push_back(log);
	if (logs.size() >= history_max)
	{
		logs.pop_front();
	}
	scroll_log = true;
}

int find_in_history(const std::string& path, size_t path_type)
{
	for (int i = 0; i < history[path_type].size(); ++i)
	{
		if (path == history[path_type][i])
		{
			return i;
		}
	}
	return -1;
}

void push_history(const std::string& path, size_t path_type)
{
	int prev_index = find_in_history(path, path_type);
	if (prev_index >= 0)
	{
		auto it = history[path_type].begin() + prev_index;
		std::rotate(it, it + 1, history[path_type].end());
	}
	else
	{
		history[path_type].push_back(path);
		if (history[path_type].size() >= history_max)
		{
			history[path_type].erase(history[path_type].begin());
		}
	}
	history_pos[path_type] = history.size() - 1;
}

std::string last_history(size_t path_type)
{

	for (size_t i = 0; i < history.size(); ++i)
	{
		size_t cur_path_type = mod(int64_t(path_type) - int64_t(i), int64_t(history.size()));
		if (!history[cur_path_type].empty())
		{
			return history[cur_path_type].back();
		}
	}
	return "";
}

void load_history()
{
	using namespace PhyreIO;
	try
	{
		if (!std::filesystem::exists("input_history.bin"))
		{
			return;
		}
		std::ifstream stream("input_history.bin", std::ios::binary);
		for (int i = 0; i < history.size(); ++i)
		{
			history[i].resize(read<int32_t>(stream));

			for (int j = 0; j < history[i].size(); ++j)
			{
				history[i][j].resize(read<int32_t>(stream), '_');
				read(stream, history[i][j].data(), history[i][j].size());
			}
			if (history[i].empty() == false)
			{
				strcpy_s(paths[i], max_path_size, history[i].back().data());
			}
		}
	}
	catch (std::exception& e)
	{
		push_log(Log(ErrorLog, "Failed to load input history " + std::string(e.what())));
	}
}

void save_history()
{
	using namespace PhyreIO;
	try 
	{
		std::ofstream stream("input_history.bin", std::ios::binary);		
		for (int i = 0; i < history.size(); ++i)
		{
			write<int32_t>(stream, history[i].size());
			for (int j = 0; j < history[i].size(); ++j)
			{
				write<int32_t>(stream, history[i][j].size());
				write(stream, history[i][j].data(), history[i][j].size());
			}
		}
	}
	catch (std::exception& e)
	{
		push_log(Log(ErrorLog, "Failed to save input history " + std::string(e.what())));
	}
}

void GLAPIENTRY opengl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	if (type == GL_DEBUG_TYPE_ERROR) 
	{
		fprintf(stderr, "GL ERROR: type = 0x%x, severity = 0x%x, message = %s\n", type, severity, message);
	}
}

uint32_t load_icon_texture(const uint8_t* data, int len)
{
	int w, h, n;
	uint8_t* img = stbi_load_from_memory(data, len, &w, &h, &n, 4);
	if (!img || !w || !h)
	{
		return 0;
	}
	uint32_t texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(img);
	return texture;
}

void load_icon_textures()
{
	file_icon = load_icon_texture(file_white, sizeof(file_white));
	folder_icon = load_icon_texture(folder_white, sizeof(folder_white));
}

void free_icon_textures()
{
	glDeleteTextures(1, &file_icon);
	glDeleteTextures(1, &folder_icon);
}

int text_history_callback_base(ImGuiInputTextCallbackData* data, int path_type)
{
	if (history[path_type].size()) 
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
		{
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				history_pos[path_type] = mod(int64_t(history_pos[path_type]) + 1, history[path_type].size());
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, history[path_type][history_pos[path_type]].c_str());
				data->SelectAll();
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				history_pos[path_type] = mod(int64_t(history_pos[path_type]) - 1, history[path_type].size());
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, history[path_type][history_pos[path_type]].c_str());
				data->SelectAll();
			}
		}
	}
	return 0;
}

int(*text_history_callback[3])(ImGuiInputTextCallbackData*) {
	[](ImGuiInputTextCallbackData* data) { return text_history_callback_base(data, 0); },
	[](ImGuiInputTextCallbackData* data) { return text_history_callback_base(data, 1); },
	[](ImGuiInputTextCallbackData* data) { return text_history_callback_base(data, 2); }
};

bool validate_paths(bool pack, bool unpack)
{
	if (!PhyreIO::VerifyFileAccessible(orig_path))
	{
		push_log(Log(ErrorLog, "Original asset path is not accessible"));
		return false;
	}

	bool processing_folder = PhyreIO::IsDirectory(orig_path);

	if (!processing_folder && !PhyreIO::VerifyPhyreHeader(orig_path))
	{
		push_log(Log(ErrorLog, "the original asset file has an unknown file structure"));
		return false;
	}

	if (pack && !PhyreIO::VerifyFileAccessible(rep_path))
	{
		push_log(Log(ErrorLog, "The replacement asset path is not accessible"));
		return false;
	}

	if (pack && processing_folder && !PhyreIO::IsDirectory(rep_path))
	{
		push_log(Log(ErrorLog, "The original asset path is a folder but the replacement asset path is a file"));
		return false;
	}

	if (pack && !processing_folder && PhyreIO::IsDirectory(rep_path))
	{
		push_log(Log(ErrorLog, "The original asset path is a file but the replacement asset path is a folder"));
		return false;
	}

	if (unpack && !PhyreIO::VerifyParentFolderAccessible(rep_path))
	{
		push_log(Log(ErrorLog, "The replacemnt asset path's parent folder is not accessible"));
		return false;
	}

	if (pack && !PhyreIO::VerifyParentFolderAccessible(out_path))
	{
		push_log(Log(ErrorLog, "The output-mod asset path's parent folder is not accessible"));
		return false;
	}

	if (pack && !PhyreIO::VerifyDifferent(orig_path, out_path))
	{
		push_log(Log(ErrorLog, "The original and output-mod asset paths must be different"));
		return false;
	}

	return true;
}

bool aquire_confirmation(bool pack, bool unpack)
{
	std::error_code ec;
	if (unpack && (
		PhyreIO::IsDirectory(rep_path) && !std::filesystem::is_empty(rep_path, ec) ||
		!PhyreIO::IsDirectory(rep_path) && std::filesystem::exists(rep_path, ec))
	) {
		int msgboxID = MessageBoxA(
			NULL,"Overwrite existing replacement asset files?", "Overwrite confirmation",
			MB_ICONWARNING | MB_YESNO
		);

		if (msgboxID != IDYES) return false;
	}

	return true;
}

bool run_command(bool pack, bool unpack)
{
	namespace fs = std::filesystem;

	if (!validate_paths(pack, unpack) || !aquire_confirmation(pack, unpack))
	{
		return false;
	}
	bool processing_folder = PhyreIO::IsDirectory(orig_path);

	if (processing_folder &&
		unpack && !PhyreIO::CopyFolderHierarchy(rep_path, orig_path) ||
		pack && !PhyreIO::CopyFolderHierarchy(out_path, orig_path)
	) {
		push_log(Log(ErrorLog, "Failed to copy the original asset path's folder hierarchy"));
		return false;
	}

	char app_path[max_path_size] = {};
	auto path_size(GetModuleFileNameA(nullptr, app_path, 1023));

	std::vector<std::string> commands;

	if (!processing_folder)
	{
		if (unpack)
		{
			std::string cmd = std::string(app_path) + " --unpack " + orig_path + " " + rep_path;
			commands.push_back(cmd);
		}
		if (pack)
		{
			std::string cmd = std::string(app_path) + " --pack " + orig_path + " " + rep_path + " " + out_path;
			commands.push_back(cmd);
		}
	}
	else
	{
		if (unpack) 
		{
			for (const auto& entry : fs::recursive_directory_iterator(orig_path))
			{
				if (!entry.is_regular_file())
				{
					continue;
				}
				std::string orig_file = entry.path().string();
				std::string rep_file;

				if (PhyreIO::GetExtension(orig_file) == "dae.phyre")
				{
					rep_file = std::string(rep_path) + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".fbx").string();
				}
				else if (PhyreIO::GetExtension(orig_file) == "dds.phyre")
				{
					rep_file = std::string(rep_path) + "/" + fs::relative(orig_file, orig_path).replace_extension().replace_extension(".png").string();
				}
				else
				{
					continue;
				}

				std::string cmd = std::string(app_path) + " --unpack " + orig_file + " " + rep_file;
				commands.push_back(cmd);

				push_history(orig_file, 0);
				push_history(rep_file, 1);
			}
		}
		if (pack) 
		{
			for (const auto& entry : fs::recursive_directory_iterator(rep_path))
			{
				if (!entry.is_regular_file())
				{
					continue;
				}
				std::string rep_file = entry.path().string();
				std::string extensions[] = { "dae.phyre", "dds.phyre" };

				for (const std::string& ext : extensions)
				{
					std::string orig_file = std::string(orig_path) + "/" + fs::relative(rep_file, rep_path).replace_extension(ext).string();
					if (!fs::exists(orig_file))
					{
						continue;
					}
					std::string out_file = std::string(out_path) + "/" + fs::relative(orig_file, orig_path).string();

					std::string cmd = std::string(app_path) + " --pack " + orig_file + " " + rep_file + " " + out_file;
					commands.push_back(cmd);

					push_history(orig_file, 0);
					push_history(rep_file, 1);
					push_history(out_file, 2);
				}
			}
		}
	}

	if (commands.empty())
	{
		push_log(Log(ErrorLog, "No files were found"));
		return false;
	}

	if (unpack)
	{
		push_history(orig_path, 0);
		push_history(rep_path, 1);
	}
	if (pack)
	{
		push_history(orig_path, 0);
		push_history(rep_path, 1);
		push_history(out_path, 2);
	}

	push_log(Log(InfoLog, "Processing files..."));

	//for(const std::string& cmd : commands)
	for(int i = 0; i < commands.size(); ++i)
	{
		std::string id = std::to_string(i) + "_" + std::to_string(GetCurrentProcessId());
		std::string cmd = commands[i];
		thread_pool.push([cmd, id](int) 
		{
			Process process;
			std::string cout, cerr;
			process.Execute(cmd, cout, cerr, id);
			{
				std::scoped_lock lock(mutex);
				if (cout.empty() == false)
				{
					push_log(Log(ConverterInfoLog, cout));
				}
				if (cerr.empty() == false)
				{
					push_log(Log(ConverterErrorLog, cerr));
				}
			}
		});
	}

	save_history();
	print_completion = true;

	return true;
}

bool ProcessingButton(std::string name)
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

bool InputAsset(const char* label, char* buf, size_t path_type)
{
	float spacing_size = 20.0f;
	ImGui::PushID(label);
	if (ImGui::ImageButton((ImTextureID)(int64_t)file_icon, ImVec2(spacing_size, spacing_size)))
	{
		std::error_code ec;
		auto files = pfd::open_file("Choose files...", std::filesystem::is_regular_file(buf, ec) ? buf : last_history(path_type),
			((path_type == 1) ? 
				std::vector<std::string>{ "Standard files (.fbx .png)", "*.fbx *.png", "All Files", "*" } : 
				std::vector<std::string>{ "Asset files (.dae.phyre .dds.phyre)", "*.dae.phyre *.dds.phyre", "All Files", "*" }),
			pfd::opt::force_path).result();

		if (files.size() && files[0].size() < max_path_size && std::filesystem::exists(files[0], ec) && !ec.value())
		{
			push_history(files[0], path_type);
			strcpy_s(buf, max_path_size, files[0].c_str());
		}
	}
	ImGui::SameLine();
	if (ImGui::ImageButton((ImTextureID)(int64_t)folder_icon, ImVec2(spacing_size, spacing_size)))
	{
		std::error_code ec;
		std::filesystem::path last;
		if (std::filesystem::is_directory(buf, ec))
		{
			last = buf;
		}
		else
		{
			std::filesystem::path last = last_history(path_type);
			if (std::filesystem::is_regular_file(last, ec))
			{
				last = last.parent_path();
			}
			if (ec.value())
			{
				last = "";
			}
		}
		auto folder = pfd::select_folder("Choose a folder...", last.string(), pfd::opt::force_path).result();

		if (folder.size())
		{
			push_history(folder, path_type);
			strcpy_s(buf, max_path_size, folder.c_str());
		}
	}
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, spacing_size - ImGui::GetFontSize()));
	std::string hint = "file or folder" + std::string(path_type == 1 ? "(.fbx, .png)" : "(.dae.phyre, .dds.phyre)");
	bool edited = ImGui::InputTextWithHint(label, hint.c_str(), buf,max_path_size, ImGuiInputTextFlags_CallbackHistory, text_history_callback[path_type]);
	ImGui::PopStyleVar();
	ImGui::PopID();
	return edited;
}

static void helloWorld() 
{
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);

	ImGui::Spacing();

	bool processing_files = thread_pool.n_idle() != thread_count_max;

	if (processing_files) ImGui::BeginDisabled();
	if (!processing_files && print_completion)
	{
		push_log(Log(InfoLog, "Processing Complete."));
		print_completion = false;
	}

	InputAsset("Original Asset/Folder", orig_path, 0);
	if (ProcessingButton("Unpack"))
	{
		run_command(false, true);
	}
	InputAsset("Replacement Asset/Folder", rep_path, 1);
	if (ProcessingButton("Pack"))
	{
		run_command(true, false);
	}
	InputAsset("Mod Output Asset/Folder", out_path, 2);

	if (processing_files) ImGui::EndDisabled();

	ImGui::Spacing();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
	ImGui::BeginChild("BottomPane", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Logs"))
		{
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	for (auto& log : logs)
	{
		if (log.type == ErrorLog)			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.5f, 1.0f));
		if (log.type == InfoLog)			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 1.0f, 1.0f));
		if (log.type == ConverterErrorLog)	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
		if (log.type == ConverterInfoLog)	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

		ImGui::TextWrapped(log.c_str());

		ImGui::PopStyleColor();
	}
	if (scroll_log)
	{
		ImGui::SetScrollHereY();
		scroll_log = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::End();
}

FILE* hfopen(const char* pipe_name)
{
	if (WaitNamedPipeA(TEXT(pipe_name), NMPWAIT_USE_DEFAULT_WAIT) == false)
	{
		return nullptr;
	}
	HANDLE hpipe = CreateFile(TEXT(pipe_name),
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (hpipe == INVALID_HANDLE_VALUE) return nullptr;
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hpipe), 0);
	if (fd < 0) return nullptr;
	return _fdopen(fd, "w");
}

int main(int argc, char* argv[])
{
	if (argc >= 6)
	{
		std::cout << "RUNNING PROCESS " << argv[argc - 2] << " " << argv[argc - 1] << std::endl;

		FILE* log_pipe = hfopen(argv[argc - 2]);
		if (log_pipe == nullptr)
		{
			std::cerr << "Failed to redirect cout stream" << std::endl;
			return -1;
		}
		FILE* err_pipe = hfopen(argv[argc - 1]);
		if (err_pipe == nullptr)
		{
			std::cerr << "Failed to redirect cerr stream" << std::endl;
			return -1;
		}
		std::ofstream log_stream(log_pipe);
		std::streambuf* old_clog = std::clog.rdbuf(log_stream.rdbuf());
		
		std::ofstream err_stream(err_pipe); 
		std::streambuf* old_cerr = std::cerr.rdbuf(err_stream.rdbuf());

		if (PhyreInterface::Initialise())
		{
			std::cout << "INITED" << std::endl;

			PhyreInterface::Run(argc - 2, (const char**)argv);
		}

		log_stream.flush();
		err_stream.flush();
		fclose(log_pipe);
		fclose(err_pipe);
		std::clog.rdbuf(old_clog);
		std::cerr.rdbuf(old_cerr);
		std::cout << "PROCESS COMPLETE" << std::endl;
		return -1;
	}
	// Setup window
	glfwSetErrorCallback([](int error, const char* description) {
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
	});
	glfwInit();

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(1280, 720, "FFXII Asset Converter ver." CONVERTER_VERSION, nullptr, nullptr);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	// Initialize OpenGL loader
	if (!gladLoadGL()) {
		std::cout << "Unable to initialize GLAD " << std::endl;
		return false;
	}
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(opengl_message_callback, 0);
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImGui::GetStyle().Colors[ImGuiCol_Button].x *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].y *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].z *= 0.35f;
	ImGui::GetStyle().Colors[ImGuiCol_Button].w = 1.0f;

	load_icon_textures();
	load_history();

	// Main loop
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);

	push_log(Log(InfoLog, "After inputting the requisite file paths, click \"Pack\" or \"Unpack\" to execute processing."));
	push_log(Log(InfoLog, "If any errors are encountered, please report them, and revert to the command-line version of this program in the meantime."));

	while (!glfwWindowShouldClose(window)) 
	{
		// Poll and handle events (inputs, window resize, etc.)
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Run our Dear ImGui application
		helloWorld();

		// Rendering
		ImGui::Render();
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
	save_history();
	// Cleanup
	free_icon_textures();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
}