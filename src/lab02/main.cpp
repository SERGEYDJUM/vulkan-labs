#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class App {
   public:
    /// @brief Initializes GLFW and Vulkan components in the correct order
    void init() {
        initGlfw();
        gatherVkLayers();
        gatherVkExtensions();
        initInstance();
        initWindow();
        initSurface();
    }

    /// @brief Runs window's event loop
    void runLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    ~App() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

   private:
    const vk::Extent2D surface_extent{800, 600};
    const vk::raii::Context vk_context{};

#ifndef NDEBUG
    const bool enable_validation_layers = true;
#else
    const bool enable_validation_layers = false;
#endif

    std::optional<vk::raii::Instance> vk_instance;
    std::optional<vk::raii::SurfaceKHR> vk_surface;
    std::vector<const char*> instance_extensions;
    std::vector<const char*> instance_layers;

    GLFWwindow* window = nullptr;

    /// @brief Initializes GLFW and sets error callback
    void initGlfw() {
        glfwInit();
        glfwSetErrorCallback([](int error, const char* msg) {
            std::cerr << "glfw: "
                      << "(" << error << ") " << msg << std::endl;
        });
    }

    /// @brief Checks availability of necessary layers and records them
    void gatherVkLayers() {
        const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";
        bool validation_layer_present = false;

        for (auto layer : vk_context.enumerateInstanceLayerProperties()) {
            // Couldn't avoid double cast
            std::string layername{static_cast<const char*>(layer.layerName)};
#ifndef NDEBUG
            std::cout << layername << std::endl;
#endif
            if (layername == validation_layer_name) {
                validation_layer_present = true;
            }
        }

        if (enable_validation_layers) {
            if (!validation_layer_present) {
                throw std::runtime_error("vulkan validation layer missing");
            }

            instance_layers.emplace_back(validation_layer_name);
        }
    }

    /// @brief Checks availability of necessary extensions and records them
    void gatherVkExtensions() {
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
            if (extension_set.count(required_ext) == 0) {
                throw std::runtime_error(
                    "vulkan extension(s) required by GLFW missing");
            }

            instance_extensions.emplace_back(glfw_extensions[i]);
        }

        if (enable_validation_layers) {
            if (extension_set.count(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                throw std::runtime_error(
                    "vulkan debug utils extension missing");
            }
            instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    /// @brief Creates a Vulkan instance with recorded layers and extensions
    void initInstance() {
        vk::ApplicationInfo app_info{"App", 1, "Engine", 1, VK_API_VERSION_1_3};
        vk::InstanceCreateInfo inst_info(vk::InstanceCreateFlags{}, &app_info);

        inst_info.enabledExtensionCount = instance_extensions.size();
        inst_info.ppEnabledExtensionNames = instance_extensions.data();

        inst_info.enabledLayerCount = instance_layers.size();
        inst_info.ppEnabledLayerNames = instance_layers.data();

        vk_instance = vk_context.createInstance(inst_info);
    }

    /// @brief Creates an empty window
    void initWindow() {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(surface_extent.width, surface_extent.height,
                                  "App", nullptr, nullptr);
    }

    /// @brief Creates a Vulkan surface on the window
    void initSurface() {
        VkSurfaceKHR surface;

        if (glfwCreateWindowSurface(*vk_instance.value(), window, nullptr,
                                    &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }

        vk_surface = {vk_instance.value(), surface};
    }
};

int main() {
    App app;

    try {
        app.init();
        app.runLoop();
    } catch (vk::SystemError& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}