
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>
#include <GLFW/glfw3.h>
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/encodedstream.h"
#include <vector>
#include <string>
#include <memory>

struct file_deleter {
    void operator()(FILE* file) { fclose(file); }
};
using file_ptr = std::unique_ptr<FILE, file_deleter>;

class cfgedit {
public:
    cfgedit();
    cfgedit(const cfgedit& that) = delete;
    cfgedit& operator=(const cfgedit& that) = delete;
    ~cfgedit();
    void open_file(const char* path);
    void gui();
    void save();
    void close();
private:
    void gui_for(const char* name, rapidjson::Value& value);
    bool try_colour_gui_for(const char* name, rapidjson::Value& value);
    std::string m_file_path;
    rapidjson::Document m_open_document;
};

static void get_icon_rgba(unsigned char rgba[32*32*4])
{
    constexpr char* icon =
        "00000000000000000000000000000000"
        "00111111100111111100111111111100"
        "00111111100111111100111111111100"
        "00111000000111000000111000000000"
        "00111000000111000000111000000000"
        "00111000000111000000111000000000"
        "00111000000111000000111000000000"
        "00111000000111111100111001111100"
        "00111000000111111100111001111100"
        "00111000000111000000111000011100"
        "00111000000111000000111000011100"
        "00111000000111000000111000011100"
        "00111000000111000000111000011100"
        "00111111100111000000111111111100"
        "00111111100111000000111111111100"
        "00000000000000000000000000000000";
    for (size_t i = 0; i < 32; ++i) {
        for (size_t j = 0; j < 16; ++j) {
            char value = icon[i+j*32] == '0' ? 0 : 0xff;
            size_t row0 = j * 2 * 32 * 4;
            size_t row1 = (j * 2 + 1) * 32 * 4;
            rgba[i * 4 + row0 + 0] = 0;
            rgba[i * 4 + row0 + 1] = 0;
            rgba[i * 4 + row0 + 2] = 0;
            rgba[i * 4 + row0 + 3] = value;
            rgba[i * 4 + row1 + 0] = 0;
            rgba[i * 4 + row1 + 1] = 0;
            rgba[i * 4 + row1 + 2] = 0;
            rgba[i * 4 + row1 + 3] = value;
        }
    }
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths)
{
    if (count > 0) {
        cfgedit* editor = (cfgedit*)glfwGetWindowUserPointer(window);
        editor->open_file(paths[0]);
    }
}

cfgedit::cfgedit()
= default;

cfgedit::~cfgedit()
= default;

void cfgedit::gui()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("cfgedit_window", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    if (m_open_document.HasParseError()) {
        ImGui::Text("Parse error.");
    } else if (m_open_document.IsNull()) {
        ImGui::Text("Drag-drop a .json file onto this window to edit it.");
    } else {
        gui_for("Document", m_open_document);
        if (ImGui::Button("Save")) {
            save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            close();
        }
    }
    ImGui::End();
}

void cfgedit::gui_for(const char* name, rapidjson::Value& value)
{
    if (value.IsNull()) {
        ImGui::Text("<NULL>");
    } else if (value.IsBool()) {
        bool bValue = value.GetBool();
        if (ImGui::Checkbox(name, &bValue)) {
            value.SetBool(bValue);
        }
    } else if (value.IsObject()) {
        for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
            gui_for(it->name.GetString(), it->value);
        }
    } else if (value.IsArray()) {
        try_colour_gui_for(name, value);
    } else if (value.IsInt()) {
        int iValue = value.GetInt();
        if (ImGui::InputInt(name, &iValue)) {
            value.SetInt(iValue);
        }
    } else if (value.IsLosslessDouble()) {
        double dValue = value.GetDouble();
        if (ImGui::InputDouble(name, &dValue)) {
            value.SetDouble(dValue);
        }
    }
}

static bool str_ends_with(const char* str, const char* suffix)
{
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

bool cfgedit::try_colour_gui_for(const char* name, rapidjson::Value& value)
{
    bool ok = true;
    ok = ok && (
        str_ends_with(name, "Color") ||
        str_ends_with(name, "Colour") ||
        str_ends_with(name, "color") ||
        str_ends_with(name, "colour")
    );
    ok = ok && value.IsArray();
    ok = ok && (value.Size() == 3 || value.Size() == 4);
    if (!ok) return false;
    bool all_int = true;
    bool all_double = true;
    float rgba[4];
    rgba[3] = 1;
    for (size_t i = 0; i < value.Size(); ++i) {
        all_int = all_int && value[i].IsInt();
        all_int = all_int && 0 <= value[i].GetInt() && value[i].GetInt() <= 255;
        all_double = all_double && value[i].IsDouble();
        all_double = all_double && 0 <= value[i].GetDouble() && value[i].GetDouble() <= 1;
    }
    if (all_double) {
        for (size_t i = 0; i < value.Size(); ++i) {
            rgba[i] = (float) value[i].GetDouble();
        }
    }
    else if (all_int) {
        for (size_t i = 0; i < value.Size(); ++i) {
            rgba[i] = value[i].GetInt() / 255.0f;
        }
    } else {
        return false;
    }
    bool edited = false;
    if (value.Size() == 3) {
        edited = ImGui::ColorEdit3(name, rgba);
    } else {
        edited = ImGui::ColorEdit4(name, rgba);
    }
    if (edited) {
        if (all_double) {
            for (size_t i = 0; i < value.Size(); ++i) {
                value[i].SetDouble(rgba[i]);
            }
        } else if (all_int) {
            for (size_t i = 0; i < value.Size(); ++i) {
                value[i].SetInt((int)(rgba[i] * 255));
            }
        }
    }
}

void cfgedit::open_file(const char* path)
{
    m_file_path = path;
    file_ptr fp(fopen(path, "rb"));
    char buffer[256];
    rapidjson::FileReadStream bis(fp.get(), buffer, sizeof(buffer));
    rapidjson::AutoUTFInputStream<unsigned, rapidjson::FileReadStream> eis(bis);
    m_open_document = {};
    m_open_document.ParseStream<0, rapidjson::AutoUTF<unsigned>>(eis);
}

void cfgedit::save()
{
    if (m_file_path.empty()) return;
    file_ptr fp(fopen(m_file_path.c_str(), "wb"));
    char buffer[256];
    rapidjson::FileWriteStream bos(fp.get(), buffer, sizeof(buffer));
    using OutputStream = rapidjson::AutoUTFOutputStream<unsigned, rapidjson::FileWriteStream>;
    OutputStream eos(bos, rapidjson::UTFType::kUTF8, true);
    rapidjson::PrettyWriter<OutputStream, rapidjson::UTF8<>, rapidjson::AutoUTF<unsigned> > writer(eos);
    writer.SetFormatOptions(rapidjson::PrettyFormatOptions::kFormatSingleLineArray);
    m_open_document.Accept(writer);
}

void cfgedit::close()
{
    m_file_path = "";
    m_open_document = {};
}

int main(int, char**)
{
    // Init glfw.
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "cfgedit", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Set icon.
    unsigned char icon_rgba[32 * 32 * 4];
    get_icon_rgba(icon_rgba);
    GLFWimage icon_image = {};
    icon_image.width = 32;
    icon_image.height = 32;
    icon_image.pixels = icon_rgba;
    glfwSetWindowIcon(window, 1, &icon_image);

    // Init imgui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsLight();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Init cfgedit.
    cfgedit editor;
    glfwSetWindowUserPointer(window, &editor);
    glfwSetDropCallback(window, glfw_drop_callback);

    // Main loop.
    while (!glfwWindowShouldClose(window))
    {
        // Input.
        glfwPollEvents();

        // Begin frame.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Define gui.
        editor.gui();

        // Draw.
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup imgui.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Cleanup glfw.
    glfwDestroyWindow(window);
    glfwTerminate();

    // Done.
    return 0;
}