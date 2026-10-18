#include <sunshine/sunshine.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

static void FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto* swapchain = reinterpret_cast<Sunshine::SwapchainContext*>(glfwGetWindowUserPointer(window));
    if (swapchain) {
        swapchain->RequestResize();
    }
}

int main() {
    std::cout << "==========================================\n"
              << " Sunshine Renderer — Faz 1 Sunshine API\n"
              << "==========================================\n";

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Sunshine Renderer — 00 Clear Screen", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }

    try {
        // 1. Initialize Sunshine Device & Swapchain
        Sunshine::Device device({ .enableValidation = true });
        Sunshine::SwapchainContext swapchain(device, window);

        glfwSetWindowUserPointer(window, &swapchain);
        glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);

        std::cout << "[Sunshine] GPU Device: " << device.GetDeviceName() << std::endl;
        std::cout << "[Sunshine] Entering main render loop..." << std::endl;

        double lastTime = glfwGetTime();
        int frameCount = 0;

        // 2. Main Render Loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // Begin frame (returns nullopt when minimized or recreating)
            if (auto frame = device.BeginFrame(swapchain)) {
                // Clear the screen with Sunshine Amber color
                frame->Clear(Sunshine::Color::SunshineAmber());
                
                // Submit and present frame
                device.SubmitFrame(*frame);
            }

            frameCount++;
            double currentTime = glfwGetTime();
            if (currentTime - lastTime >= 1.0) {
                double fps = frameCount / (currentTime - lastTime);
                double msPerFrame = 1000.0 / fps;

                char title[128];
                snprintf(title, sizeof(title), "Sunshine Renderer — 00 Clear Screen [%.1f FPS | %.2f ms]", fps, msPerFrame);
                glfwSetWindowTitle(window, title);
                std::cout << "[Sunshine] Running @ " << static_cast<int>(fps) << " FPS (" << msPerFrame << " ms/frame)" << std::endl;

                frameCount = 0;
                lastTime = currentTime;
            }
        }

        device.WaitIdle();
        std::cout << "[Sunshine] Main loop exited cleanly." << std::endl;

    } catch (const Sunshine::GraphicsException& ex) {
        std::cerr << "\n" << ex.what() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "\n[Standard Exception] " << ex.what() << "\n";
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "[Sunshine] Exited with success." << std::endl;
    return EXIT_SUCCESS;
}
