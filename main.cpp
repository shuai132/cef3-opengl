#include <GLFW/glfw3.h>
#include <functional>

#include <thread>
#include <time.h>
#include <include/wrapper/cef_library_loader.h>
#include <include/internal/cef_mac.h>
#include "simple_app.h"
#include "simple_handler.h"

typedef std::function<void(CefBrowserHost*)> Handle;

static int mouse_x_ = 0;
static int mouse_y_ = 0;

static void foreachBrowser(const Handle &handle) {
    auto browsers = SimpleHandler::GetInstance()->getBrowserList();
    for (const auto &browser : browsers) {
        handle(browser->GetHost());
    }
}

static void error_callback(int error, const char* description) {
    fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    bool pressed = (action == GLFW_PRESS);

    CefKeyEvent evt;
    evt.native_key_code = key;
    evt.type = pressed ? KEYEVENT_KEYDOWN : KEYEVENT_KEYUP;
    evt.character = key;
    evt.native_key_code = key;
    evt.type = KEYEVENT_CHAR;

    foreachBrowser([&](CefBrowserHost* browser){
        browser->SendKeyEvent(evt);
    });
}

static void mouse_callback(GLFWwindow* window, int btn, int state, int mods) {
    int mouse_up = (GLFW_RELEASE == state);

    std::map<int, CefBrowserHost::MouseButtonType> btn_type_map;
    btn_type_map[GLFW_MOUSE_BUTTON_LEFT] = MBT_LEFT;
    btn_type_map[GLFW_MOUSE_BUTTON_MIDDLE] = MBT_MIDDLE;
    btn_type_map[GLFW_MOUSE_BUTTON_RIGHT] = MBT_RIGHT;
    CefBrowserHost::MouseButtonType btn_type = btn_type_map[btn];

    CefMouseEvent evt;
    evt.x = mouse_x_;
    evt.y = mouse_y_;

    //TODO
    int click_count = 1;

    foreachBrowser([&](CefBrowserHost* browser){
        browser->SendMouseClickEvent(evt, btn_type, mouse_up, click_count);
    });
}

static void scroll_callback(GLFWwindow* window, double xAxis, double yAxis) {
    CefMouseEvent evt;
    evt.x = mouse_x_;
    evt.y = mouse_y_;

    foreachBrowser([&](CefBrowserHost* browser){
        browser->SendMouseWheelEvent(evt, static_cast<int>(xAxis), static_cast<int>(yAxis));
    });
}

static void motion_callback(GLFWwindow* window, double x, double y) {
    mouse_x_ = x;
    mouse_y_ = y;

    CefMouseEvent evt;
    evt.x = x;
    evt.y = y;

    //TODO
    bool mouse_leave = false;

    foreachBrowser([&](CefBrowserHost* browser){
        browser->SendMouseMoveEvent(evt, mouse_leave);
    });
}

static void reshape_callback(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);

    SimpleHandler::GetInstance()->resize(w, h);
    foreachBrowser([&](CefBrowserHost* browser){
        browser->WasResized();
    });
}

static GLFWwindow* initGLFW(int w, int h) {
    glfwSetErrorCallback(error_callback);

    /* Initialize the library */
    if (!glfwInit())
        return nullptr;

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(w, h, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return nullptr;
    }

    glfwSwapInterval(1);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, motion_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, reshape_callback);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    return window;
}

static int initCEF3(int argc, char* argv[]) {
    // Load the CEF framework library at runtime instead of linking directly
    // as required by the macOS sandbox implementation.
    CefScopedLibraryLoader library_loader;
    if (!library_loader.LoadInMain())
        return 1;

    // Provide CEF with command-line arguments.
    CefMainArgs main_args(argc, argv);

    // Specify CEF global settings here.
    CefSettings settings;
    settings.windowless_rendering_enabled = true;

    // When generating projects with CMake the CEF_USE_SANDBOX value will be defined
    // automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line to disable
    // use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = true;
#endif

    // SimpleApp implements application-level callbacks for the browser process.
    // It will create the first browser instance in OnContextInitialized() after
    // CEF has initialized.
    CefRefPtr<SimpleApp> app(new SimpleApp);

    // Initialize CEF for the browser process.
    CefInitialize(main_args, settings, app.get(), NULL);

    // Run the CEF message loop. This will block until CefQuitMessageLoop() is
    // called.
//    CefRunMessageLoop();
    return 0;
}

int main(int argc, char* argv[])
{
    // init GLFW
    GLFWwindow* window = initGLFW(1024, 768);
    if (!window) return -1;

    // init CEF3
    auto ret = initCEF3(argc, argv);
    if (ret != 0) return -2;

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window)) {
        SimpleHandler::GetInstance()->Render();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        CefDoMessageLoopWork();
    }

    // Shut down CEF.
    CefShutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
