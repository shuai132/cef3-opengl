#include <GLFW/glfw3.h>

#include <thread>
#include <time.h>
#include <include/wrapper/cef_library_loader.h>
#include <include/internal/cef_mac.h>
#include "simple_app.h"
#include "simple_handler.h"

static void drawCEF() {
    SimpleHandler::GetInstance()->Render();
}

static void drawTriangle() {
    /* Draw a triangle */
    glBegin(GL_TRIANGLES);

    glColor3f(1.0, 0.0, 0.0);    // Red
    glVertex3f(0.0, 1.0, 0.0);

    glColor3f(0.0, 1.0, 0.0);    // Green
    glVertex3f(-1.0, -1.0, 0.0);

    glColor3f(0.0, 0.0, 1.0);    // Blue
    glVertex3f(1.0, -1.0, 0.0);

    glEnd();
}

int main(int argc, char* argv[])
{
    /************* GLFW *************/
    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(480, 320, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(1);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /************* CEF3 *************/
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

    volatile bool isDrawOther = false;
    new std::thread([&]{
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            isDrawOther = !isDrawOther;
        }
    });

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        if (true) {
            drawCEF();
        } else {
            drawTriangle();
        }

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        CefDoMessageLoopWork();
    }

    // Shut down CEF.
    CefShutdown();

    glfwTerminate();
    return 0;
}
