#include <GLFW/glfw3.h>

#include <thread>
#include <time.h>

void drawOther() {
    /* Draw a triangle */
    glBegin(GL_TRIANGLES);

    glColor3f(1.0, 0.0, 1.0);    // Red
    glVertex3f(0.0, 1.0, 0.0);

    glColor3f(0.0, 1.0, 0.0);    // Green
    glVertex3f(-1.0, -1.0, 0.0);

    glColor3f(1.0, 0.0, 1.0);    // Blue
    glVertex3f(1.0, -1.0, 0.0);

    glEnd();
}

int main()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(480, 320, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    bool isDrawOther = false;
    new std::thread([&]{
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            isDrawOther = !isDrawOther;
        }
    });

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Draw a triangle */
        glBegin(GL_TRIANGLES);

        glColor3f(1.0, 0.0, 0.0);    // Red
        glVertex3f(0.0, 1.0, 0.0);

        glColor3f(0.0, 1.0, 0.0);    // Green
        glVertex3f(-1.0, -1.0, 0.0);

        glColor3f(0.0, 0.0, 1.0);    // Blue
        glVertex3f(1.0, -1.0, 0.0);

        glEnd();

        if (isDrawOther)
            drawOther();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
