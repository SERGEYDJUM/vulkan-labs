#include <iostream>
#include <stdexcept>
#include <unordered_set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class App {
   public:
    App() : vk_instance{initInstance()} {
        glfwInit();
        checkVkLayers();
        checkVkExtensions();
        initWindow();
    }

    ~App() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void run() { eventLoop(); }

   private:
    const std::pair<uint32_t, uint32_t> window_dims{800, 600};

    vk::raii::Context vk_context;
    vk::raii::Instance vk_instance;

    GLFWwindow* window = nullptr;

#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    void checkVkLayers() {
        bool validation_layer_present = false;

        for (auto layer : vk_context.enumerateInstanceLayerProperties()) {
            std::string layername{static_cast<const char*>(layer.layerName)};
#ifndef NDEBUG
            std::cout << layername << std::endl;
#endif
            if (layername == "VK_LAYER_KHRONOS_validation") {
                validation_layer_present = true;
            }
        }

        if (enable_validation_layers && !validation_layer_present) {
            throw std::runtime_error("vulkan validation layer missing");
        }
    }

    void checkVkExtensions() {
        uint32_t glfw_extensions_cnt = 0;
        const char** glfw_extensions =
            glfwGetRequiredInstanceExtensions(&glfw_extensions_cnt);

        std::unordered_set<std::string> extension_set{};

        for (auto ext : vk_context.enumerateInstanceExtensionProperties()) {
            std::string extname{static_cast<const char*>(ext.extensionName)};
#ifndef NDEBUG
            std::cout << extname << std::endl;
#endif
            extension_set.insert(extname);
        }

        for (uint32_t i = 0; i < glfw_extensions_cnt; i++) {
            std::string required_ext{glfw_extensions[i]};
#ifndef NDEBUG
            std::cout << "required: " << required_ext << std::endl;
#endif
            if (!extension_set.contains(required_ext)) {
                throw std::runtime_error(
                    "vulkan extension(s) required by GLFW missing");
            }
        }

        if (enable_validation_layers &&
            !extension_set.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            throw std::runtime_error("vulkan debug utils extension missing");
        }
    }

    vk::raii::Instance initInstance() {
        vk::ApplicationInfo app_info{"App", 1, "Engine", 1, VK_API_VERSION_1_3};

        vk::InstanceCreateInfo instance_info(vk::InstanceCreateFlags(),
                                             &app_info);

        return vk_context.createInstance(instance_info);
    }

    void initWindow() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(window_dims.first, window_dims.second,
                                  "Vulkan", nullptr, nullptr);
    }

    void eventLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
};

int main() {
    try {
        App app{};
        app.run();
    } catch (vk::SystemError& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}