//////////////////////////////////////////////////////////////////////////////
/// 
/// cfgedit -- simple JSON config file editor.
/// 
/// Usage:
/// 
///     cfgedit
/// 
///     cfgedit <path>
///
//////////////////////////////////////////////////////////////////////////////

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
#include <sstream>

//////////////////////////////////////////////////////////////////////////////
// types
//////////////////////////////////////////////////////////////////////////////

/// @brief Functor for closing files.
struct file_closer {
    void operator()(FILE* file) { fclose(file); }
};

/// @brief unique_ptr which can close a FILE.
using file_ptr = std::unique_ptr<FILE, file_closer>;

/// @brief Config editor.
///
/// Tracks an open JSON config file and keeps a copy of the contents. Displays a
/// UI using ImGUI to edit the content copy, and can save it back to the
/// original file.
class cfgedit {
public:

    /// @brief Constructor.
    cfgedit();

    // Copying is prohibited.
    cfgedit(const cfgedit& that) = delete;

    // Assignment is prohibited.
    cfgedit& operator=(const cfgedit& that) = delete;

    /// @brief Destructor.
    ~cfgedit();
    
    /// @brief Call ImGUI such that editing GUI for the currently open document
    /// is displayed.
    void gui();

    /// @brief Open the indicated file and parse its contents. If there is an error,
    /// an error will be displayed.
    void open_file(const char* path);

    /// @brief Save the current contents to the last location we read.
    void save();

    /// @brief Discard the contents and retained path.
    void close();

    /// @brief Discard the contents and reload the retained path.
    void reload();

private:

    /// @brief Display ImGUI controls to edit a JSON value.
    ///
    /// This will inspect the value and recurse as needed. For instance, if the
    /// value is an object then we will add GUI for each of its members.
    ///
    /// Heuristics are applied to identify structured data e.g. colours, and
    /// display appropriate controls to edit them.
    ///
    /// @param name Label for the edit control. 
    ///
    /// @param value JSON value to edit.
    ///
    /// @param depth Current recursion depth. Start at 0. 
    ///
    /// @param fieldId Unique-ish id to push for controls. This gets pushed via
    /// ImGui::PushID() for each value, and then incremented. In this way, the
    /// controls for each value have an id distinct from one another --
    /// otherwise, you can get weird behaviour if controls have the same label,
    /// which is how ImGui usually identifies controls.
    void gui_for(const char* name, rapidjson::Value& value, int depth, int* fieldId);

    /// @brief Attempt to interpret a value as a colour and display GUI for it.
    /// 
    /// @param name Label for field.
    ///
    /// @param value Array value.
    ///
    /// @return Whether this value looked like a colour. IF not, then no GUI was
    /// created for it.
    bool try_colour_gui_for(const char* name, rapidjson::Value& value);

    // Path to last-read file.
    std::string m_file_path;

    // Contents of last-read file.
    rapidjson::Document m_open_document;
};

//////////////////////////////////////////////////////////////////////////////
// functions / callbacks
//////////////////////////////////////////////////////////////////////////////

/// @brief Write 32x32 RGBA data representing an icon to the array provided.
static void get_icon_rgba(unsigned char rgba[32*32*4])
{
    // It's just beautiful isn't it.
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

    // The above is 32x16 since text characters typically are taller than they
    // are wide, so 2:1 looks square. So we set two rows at a time on the
    // output.
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

/// @brief Output glfw errors.
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/// @brief Open dropped path. It is assumed that `*window` has a `cfgedit` as
/// its user pointer.
static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths)
{
    if (count > 0) {
        void* ptr = glfwGetWindowUserPointer(window);
        if (ptr) {
            cfgedit* editor = (cfgedit*)ptr;
            editor->open_file(paths[0]);
        }
    }
}

//////////////////////////////////////////////////////////////////////////////
// cfgedit implementation
//////////////////////////////////////////////////////////////////////////////

cfgedit::cfgedit()
= default;

cfgedit::~cfgedit()
= default;

void cfgedit::gui()
{
    if (m_open_document.HasParseError()) {
        ImGui::Text("Parse error.");
        ImGui::Text("Drag-drop a .json file onto this window to edit it.");
    } else if (m_open_document.IsNull()) {
        ImGui::Text("Drag-drop a .json file onto this window to edit it.");
    } else {
        if (ImGui::Button("Save")) {
            save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            reload();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            close();
        }
        ImGui::Separator();
        int fieldId = 0;
        gui_for(m_file_path.c_str(), m_open_document, 0, &fieldId);
    }
}

void cfgedit::gui_for(const char* name, rapidjson::Value& value, int depth, int* fieldId)
{
    // Push a unique-ish id for the UI for this value.
    // todo: is this unique enough? I'm assuming we end up with the stack of ids plus the
    // label getting concatenated together to form a composite id, but it might not actually
    // work that way. Might need to revisit this if I get weird behaviour.
    int id = ++* fieldId;
    ImGui::PushID(id);

    if (value.IsNull()) {
        // This is a rare scenario!
        ImGui::Text("<NULL>");

    } else if (value.IsBool()) {
        // Edit bool.
        bool bValue = value.GetBool();
        if (ImGui::Checkbox(name, &bValue)) {
            value.SetBool(bValue);
        }

    } else if (value.IsObject()) {
        // Edit object recursively. At level zero we don't push a tree node as
        // that's a waste of space doesn't look nice, but subobjects get indented.
        if (depth == 0 || ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) {
                gui_for(it->name.GetString(), it->value, depth+1, fieldId);
            }
            if (depth != 0) ImGui::TreePop();
        }

    } else if (value.IsArray()) {
        // Edit array recursively. Attempt to spot colours and provide nice GUI
        // for them.
        if (!try_colour_gui_for(name, value)) {
            if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen)) {
                size_t i = 0;
                for (auto it = value.Begin(); it != value.End(); ++it) {
                    std::stringstream sstrm;
                    sstrm << name << "[" << i++ << "]";
                    gui_for(sstrm.str().c_str(), *it, depth+1, fieldId);
                }
                ImGui::TreePop();
            }
        }

    } else if (value.IsInt()) {
        // Edit integer.
        int iValue = value.GetInt();
        if (ImGui::InputInt(name, &iValue)) {
            value.SetInt(iValue);
        }

    } else if (value.IsLosslessDouble()) {
        // Edit real number.
        double dValue = value.GetDouble();
        if (ImGui::InputDouble(name, &dValue)) {
            value.SetDouble(dValue);
        }
    }

    // Pop the Id we pushed before.
    ImGui::PopID();
}

/// @brief Does a string end with a suffix
/// @param str the suffix-haver
/// @param suffix potential suffix
/// @return true if 'str' has the suffix, false if not, or if either string is null.
static bool str_ends_with(const char* str, const char* suffix)
{
    if (!str || !suffix) return false;
    size_t str_len = strlen(str); // bite me
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

bool cfgedit::try_colour_gui_for(const char* name, rapidjson::Value& value)
{
    // Check the name looks right, it's an array, and has a plausible length.
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

    // Are colour components 0f..1f or 0..255? Get the colour as rgba.
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
    } else if (all_int) {
        for (size_t i = 0; i < value.Size(); ++i) {
            rgba[i] = value[i].GetInt() / 255.0f;
        }
    } else {
        return false;
    }

    // Show edit UI.
    bool edited = false;
    if (value.Size() == 3) {
        edited = ImGui::ColorEdit3(name, rgba);
    } else {
        edited = ImGui::ColorEdit4(name, rgba);
    }

    // Set the value if it was edited.
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

    // Yep, it was a colour.
    return true;
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

void cfgedit::reload()
{
    std::string file_path = m_file_path;
    open_file(file_path.c_str());
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

//////////////////////////////////////////////////////////////////////////////
// main function
//////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
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

    // If a path was provided then open it.
    if (argc > 1) {
        editor.open_file(argv[1]);
    }

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
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("cfgedit_window", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        editor.gui();
        ImGui::End();

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