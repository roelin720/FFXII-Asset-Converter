#include "PhyreGUI.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "assimp/../../contrib/stb/stb_image.h"
#include "Icons/file_white.h"
#include "Icons/folder_white.h"
#include "Icons/file_white.h"
#include "PhyreIOUtils.h"
#include "Process.h"
#include "PhyreInterface.h"
#include <filesystem>
#include <format>
#include <thread>

Phyre::FileBrowser Phyre::GUI::browser("file_browser");
Phyre::PathHistory Phyre::GUI::history;
ctpl::thread_pool Phyre::GUI::thread_pool;
Phyre::Logs Phyre::GUI::logs;
std::mutex Phyre::GUI::log_mutex;

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
		[](ImGuiInputTextCallbackData* data) { return Phyre::GUI::history.scroll_callback(data, Phyre::PathID_ORIG); },
		[](ImGuiInputTextCallbackData* data) { return Phyre::GUI::history.scroll_callback(data, Phyre::PathID_REP); },
		[](ImGuiInputTextCallbackData* data) { return Phyre::GUI::history.scroll_callback(data, Phyre::PathID_MOD); }
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
			system(("explorer \"" + parent + "\"").c_str());
		}
		catch (const std::exception& e)
		{
			system(("explorer \"" + path + "\"").c_str());
		}
	}
}

Phyre::GUI::GUI()
{
}

bool Phyre::GUI::run()
{
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
	if (!gladLoadGL()) 
	{
		std::cerr << "Unable to initialize GLAD " << std::endl;
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
	ImGui::GetStyle().AntiAliasedLines = false;

	load_icon_textures();
	history.load();

	if (!browser.init())
	{
		std::cerr << "Failed to initialise file browser" << std::endl;
		return false;
	}
	// Main loop
	int display_w, display_h;
	glfwGetFramebufferSize(window, &display_w, &display_h);

	LOG(Log_StartInfo, "After inputting the requisite file paths, click \"Pack\" or \"Unpack\" to execute processing.");
	LOG(Log_StartInfo, "If any errors are encountered, please report them, and revert to the command-line version of this program in the meantime.");

	while (!glfwWindowShouldClose(window))
	{
		// Poll and handle events (inputs, window resize, etc.)
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		draw_ui();

		// Rendering
		ImGui::Render();
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}
	// Cleanup
	history.save();
	browser.free();

	thread_pool.stop();

	free_icon_textures();
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return true;
}

void Phyre::GUI::draw_ui()
{
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);

	if (draw_file_dialog())
	{
		ImGui::End();
		return;
	}

	ImGui::Spacing();

	bool input_disabled = processing;
	if (input_disabled) ImGui::BeginDisabled();

	draw_input_path("Original Asset/Folder", history.current[PathID_ORIG], PathID_ORIG);
	if (draw_processing_button("Unpack"))
	{
		run_command(false , true);
		history.push(history.current[PathID_ORIG], PathID_ORIG);
		history.push(history.current[PathID_REP], PathID_REP);
		history.save();
	}
	draw_input_path("Replacement Asset/Folder", history.current[PathID_REP], PathID_REP);
	if (draw_processing_button("Pack"))
	{
		run_command(true, false);
		history.push(history.current[PathID_ORIG], PathID_ORIG);
		history.push(history.current[PathID_REP], PathID_REP);
		history.push(history.current[PathID_MOD], PathID_MOD);
		history.save();
	}
	draw_input_path("Mod Output Asset/Folder", history.current[PathID_MOD], PathID_MOD);

	if (input_disabled) ImGui::EndDisabled();

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

	log_mutex.lock();
	for (auto& log : logs.data)
	{
		if (log.type == Log_Error)		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.75f, 1.0f));
		if (log.type == Log_Info)		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
		if (log.type == Log_StartInfo)	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 1.0f, 1.0f));
		if (log.type == Log_ConvError)	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
		if (log.type == Log_ConvInfo)	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));

		ImGui::TextWrapped(log.c_str());

		ImGui::PopStyleColor();
	}
	log_mutex.unlock();

	if (logs.scroll_log)
	{
		ImGui::SetScrollHereY();
		logs.scroll_log = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::End();
}

bool Phyre::GUI::draw_file_dialog()
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
			std::string path = PhyreIO::Join(browser.cur_parent_folder) + "/" + browser.cur_filename;
			history.push(path, dialog_pathID);
			strcpy_s(history.current[dialog_pathID], history.max_path_size, path.c_str());
		}
		dialog_pathID = PathID_INVALID;
	}

	return true;
}

void  Phyre::GUI::load_icon_textures()
{
	file_icon = GUITexture(file_white, sizeof(file_white));
	folder_icon = GUITexture(folder_white, sizeof(folder_white));
}

void  Phyre::GUI::free_icon_textures()
{
	file_icon.free();
	folder_icon.free();
}

bool Phyre::GUI::draw_processing_button(std::string name)
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

bool Phyre::GUI::draw_input_path(const char* label, char* buf, PathID pathID)
{
	float spacing_size = 20.0f;
	ImGui::PushID(label);
	if (ImGui::ImageButton(file_icon, ImVec2(spacing_size, spacing_size)))
	{
		browser.set_current_path(buf);
		if (pathID == PathID_REP)
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

bool Phyre::GUI::validate_paths(bool pack, bool unpack)
{
	bool processing_folder = false;

	if (std::string(history.current[PathID_ORIG]).contains(".vbf"))
	{
		std::string arc_path, file_name;
		if (!VBFUtils::Separate(history.current[PathID_ORIG], arc_path, file_name))
		{
			LOG(Log_Error, "Vbf asset path is invalid");
			return false;
		}
		
		Phyre::FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
		const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);
		if (!node)
		{
			LOG(Log_Error, "Failed to find " + file_name + " in vbf");
			return false;
		}
		processing_folder = !node->children.empty();
	}
	else
	{
		if (!PhyreIO::VerifyFileAccessible(history.current[PathID_ORIG]))
		{
			LOG(Log_Error, "Original asset path is not accessible");
			return false;
		}

		processing_folder = PhyreIO::IsDirectory(history.current[PathID_ORIG]);

		if (!processing_folder && !PhyreIO::VerifyPhyreHeader(history.current[PathID_ORIG]))
		{
			LOG(Log_Error, "the original asset file has an unknown file structure");
			return false;
		}
	}

	if (std::string(history.current[PathID_REP]).contains(".vbf"))
	{
		LOG(Log_Error, "The replacement asset path cannot be in a vbf");
		return false;
	}

	if (pack && !PhyreIO::VerifyFileAccessible(history.current[PathID_REP]))
	{
		LOG(Log_Error, "The replacement asset path is not accessible");
		return false;
	}

	if (pack && processing_folder && !PhyreIO::IsDirectory(history.current[PathID_REP]))
	{
		LOG(Log_Error, "The original asset path is a folder but the replacement asset path is a file");
		return false;
	}

	if (pack && !processing_folder && PhyreIO::IsDirectory(history.current[PathID_REP]))
	{
		LOG(Log_Error, "The original asset path is a file but the replacement asset path is a folder");
		return false;
	}

	if (unpack && !PhyreIO::VerifyParentFolderAccessible(history.current[PathID_REP]))
	{
		LOG(Log_Error, "The replacemnt asset path's parent folder is not accessible");
		return false;
	}

	if (pack) 
	{
		if (std::string(history.current[PathID_MOD]).contains(".vbf"))
		{
			std::string arc_path, file_name;
			if (!VBFUtils::Separate(history.current[PathID_MOD], arc_path, file_name))
			{
				LOG(Log_Error, "VBF asset path is invalid");
				return false;
			}

			Phyre::FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			if (!arc_rec)
			{
				return false;
			}
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);;
			if (!node)
			{
				LOG(Log_Error, "Failed to find " + file_name + " in vbf");
				return false;
			}
		}
		else
		{
			if (!PhyreIO::VerifyParentFolderAccessible(history.current[PathID_MOD]))
			{
				LOG(Log_Error, "The output-mod asset path's parent folder is not accessible");
				return false;
			}

			if (!PhyreIO::VerifyDifferent(history.current[PathID_ORIG], history.current[PathID_MOD]))
			{
				LOG(Log_Error, "The original and output-mod asset paths must be different");
				return false;
			}
		}
	}

	return true;
}

bool Phyre::GUI::aquire_confirmation(bool pack, bool unpack)
{
	std::error_code ec;
	if (unpack && (
		PhyreIO::IsDirectory(history.current[PathID_REP]) && !std::filesystem::is_empty(history.current[PathID_REP], ec) ||
		!PhyreIO::IsDirectory(history.current[PathID_REP]) && std::filesystem::exists(history.current[PathID_REP], ec))
		) {
		int msgboxID = MessageBoxA(
			NULL, "Overwrite existing replacement asset files?", "Overwrite confirmation",
			MB_ICONWARNING | MB_YESNO
		);

		if (msgboxID != IDYES) return false;
	}

	return true;
}

bool Phyre::GUI::run_command(bool pack, bool unpack)
{
	namespace fs = std::filesystem;

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
				LOG(Log_Error, "failed to deduce archive paths for " + path);
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
					std::streambuf* old_cerr = std::cerr.rdbuf(err_stream.rdbuf());

					LOG(Log_Info, "Loading vbf " + arc_path);

					find->second.valid = find->second.archive.load(arc_path);
					find->second.loaded = true;

					if (!find->second.valid)
					{
						std::cerr.rdbuf(old_cerr);
						LOG(Log_Error, err_stream.str());
						LOG(Log_Error, "Failed to load vbf " + arc_path);
					}
					std::cerr.rdbuf(old_cerr);	
					processing = false;

					run_command(pack, unpack);
				});
				return false;
			}
			while (!find->second.loaded)
			{
				LOG(Log_Info, "Waiting for vbf to load - " + arc_path);
				Sleep(2000);
			}

			if (!find->second.valid)
			{
				LOG(Log_Error, "Failed to load vbf previously " + arc_path);
				return false;
			}
		}
	}

	if (!validate_paths(pack, unpack) || !aquire_confirmation(pack, unpack))
	{
		return false;
	}

	processing = true;

	thread_pool.push([this, pack, unpack](int) 
	{
		std::string tmp_src_dir;
		std::string tmp_dst_dir;
		std::string unmodified_orig_path = PhyreIO::Normalise(history.current[PathID_ORIG]);
		std::string unmodified_rep_path = PhyreIO::Normalise(history.current[PathID_REP]);
		std::string unmodified_out_path = PhyreIO::Normalise(history.current[PathID_MOD]);
		std::string orig_path = unmodified_orig_path;
		std::string rep_path = unmodified_rep_path;
		std::string out_path = unmodified_out_path;

		bool processing_folder = false;

		if (unmodified_orig_path.contains(".vbf"))
		{
			std::string arc_path, file_name;
			VBFUtils::Separate(unmodified_orig_path, arc_path, file_name);
			Phyre::FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);

			tmp_src_dir = PhyreIO::CreateTmpPath("tmp_vbf_src");
			if (tmp_src_dir.empty())
			{
				LOG(Log_Error, "Failed to create temp path tmp_vbf_src");
				processing = false;
				return false;
			}

			LOG(Log_Info, "Extracting vbf files...");
			
			std::stringstream err_stream;
			std::streambuf* old_cerr = std::cerr.rdbuf(err_stream.rdbuf());

			if (!arc_rec->archive.extract_all(tmp_src_dir, *node))
			{
				std::cerr.rdbuf(old_cerr);
				LOG(Log_Error, err_stream.str());
				std::filesystem::remove(tmp_src_dir);
				processing = false;
				return false;
			}
			std::cerr.rdbuf(old_cerr);

			orig_path = tmp_src_dir + "/" + node->name_segment;

			processing_folder = !node->children.empty();
		}

		if (pack && out_path.contains(".vbf"))
		{
			tmp_dst_dir = PhyreIO::CreateTmpPath("tmp_vbf_dst");
			if(tmp_dst_dir.empty()) 
			{
				LOG(Log_Error, "Failed to create temp path tmp_vbf_dst");
				processing = false;
				return false;
			}
			out_path = tmp_dst_dir + "/" + PhyreIO::FileName(out_path);
		}

		processing_folder = PhyreIO::IsDirectory(orig_path);

		if (processing_folder &&
			unpack && !PhyreIO::CopyFolderHierarchy(rep_path, orig_path) ||
			pack && !PhyreIO::CopyFolderHierarchy(out_path, orig_path)
			) {
			LOG(Log_Error, "Failed to copy the original asset path's folder hierarchy");
			processing = false;
			return false;
		}

		char app_path[history.max_path_size] = {};
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
					}
				}
			}
		}

		if (commands.empty())
		{
			LOG(Log_Error, "No files were found");
			processing = false;
			return false;
		}

		LOG(Log_Info, "Processing files...");

		std::string id_suffix = std::to_string(GetCurrentProcessId());

		std::vector<bool> tasks_completed(commands.size(), false);

		for (int i = 0; i < commands.size(); ++i)
		{
			std::string id = std::to_string(i) + std::format("_{:#x}_", (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()) + id_suffix;
			std::string cmd = commands[i];
			thread_pool.push([this, cmd, id, i, &tasks_completed](int)
			{
				Process process;
				std::string cout, cerr;
				process.Execute(cmd, cout, cerr, id);
				
				if (cout.empty() == false)
				{
					LOG(Log_ConvInfo, cout);
				}
				if (cerr.empty() == false)
				{
					LOG(Log_ConvError, cerr);
				}
				
				tasks_completed[i] = true;
			});
		}

		while (!std::all_of(tasks_completed.begin(), tasks_completed.end(), [](bool v) { return v; }))
		{
			Sleep(250);
		}

		if (tmp_dst_dir.size())
		{
			std::string arc_path, file_name;
			VBFUtils::Separate(unmodified_out_path, arc_path, file_name);
			Phyre::FileBrowser::ArchiveRecord* arc_rec = &browser.archives.find(arc_path)->second;
			const VBFArchive::TreeNode* node = arc_rec->archive.find_node(file_name);

			std::stringstream err_stream;
			std::streambuf* old_cerr = std::cerr.rdbuf(err_stream.rdbuf());

			LOG(Log_Info, "Injecting files into vbf - " + arc_path);

			if (!arc_rec->archive.inject_all(tmp_dst_dir, *node))
			{
				std::cerr.rdbuf(old_cerr);
				LOG(Log_Error, err_stream.str());
				LOG(Log_Error, "Failed to inject into " + arc_path);
				std::filesystem::remove_all(tmp_dst_dir);
				processing = false;
				return false;
			}
			if (!arc_rec->archive.update_header())
			{
				std::cerr.rdbuf(old_cerr);
				LOG(Log_Error, err_stream.str());
				LOG(Log_Error, "Failed to update vbf header of " + arc_path);
				std::filesystem::remove_all(tmp_dst_dir);
				processing = false;
				return false;
			}
			std::cerr.rdbuf(old_cerr);
			std::filesystem::remove_all(tmp_dst_dir);
			tmp_dst_dir = "";
		}
		if (tmp_src_dir.size())
		{
			std::filesystem::remove_all(tmp_src_dir);
			tmp_src_dir = "";
		}
		processing = false;
		LOG(Log_Info, "Processing Complete.");

		return true;
	});

	return true;
}