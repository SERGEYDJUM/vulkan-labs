#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.hpp"

class App {
   public:
    /// @brief Initializes GLFW and Vulkan components in the correct order
    void init() {
        initGlfw();
        gatherVkLayers();
        gatherVkExtensions();
        initWindow();

        initInstance();
#ifndef NDEBUG
        initValidation();
#endif
        initDevice();
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

    std::optional<vk::raii::Instance> vk_instance;
    std::optional<vk::raii::SurfaceKHR> vk_surface;
    std::optional<vk::raii::PhysicalDevice> vk_physical_device;
    std::optional<vk::raii::Device> vk_device;
    std::vector<const char*> instance_extensions;
    std::vector<const char*> instance_layers;

    GLFWwindow* window = nullptr;

#ifndef NDEBUG
    const bool enable_validation_layers = true;
    std::optional<vk::raii::DebugUtilsMessengerEXT> debug_messenger;

    /// @brief Initializes debug messenger
    void initValidation() {
        vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_info{
            {},
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            debugUtilsMessengerCallback};

        debug_messenger = vk_instance.value().createDebugUtilsMessengerEXT(
            debug_messenger_info);
    }
#else
    const bool enable_validation_layers = false;
#endif

    /// @brief Initializes GLFW and sets error callback
    void initGlfw() {
        glfwInit();

        if (glfwVulkanSupported() == GLFW_FALSE) {
            throw std::runtime_error("glfw: failed to find vulkan loader");
        }

        glfwSetErrorCallback([](int error, const char* msg) {
            std::cerr << "glfw: "
                      << "(" << error << ") " << msg << std::endl;
        });
    }

    /// @brief Checks availability of necessary layers and records them
    void gatherVkLayers() {
        std::unordered_set<std::string> layer_set{};

#ifndef NDEBUG
        std::vector<const char*> required_layers = {
            "VK_LAYER_KHRONOS_validation"};
#else
        std::vector<const char*> required_layers = {};
#endif

        for (auto layer : vk_context.enumerateInstanceLayerProperties()) {
            std::string layer_name{static_cast<const char*>(layer.layerName)};
            // #ifndef NDEBUG
            //             std::cout << layer_name << std::endl;
            // #endif
            layer_set.insert(layer_name);
        }

        for (auto layer : required_layers) {
            // #ifndef NDEBUG
            //             std::cout << "required: " << layer <<
            //             std::endl;
            // #endif
            if (layer_set.count(layer) == 0) {
                throw std::runtime_error(
                    "vulkan layer(s) required by GLFW missing");
            }

            instance_layers.emplace_back(layer);
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
            // #ifndef NDEBUG
            //             std::cout << extname << std::endl;
            // #endif
            extension_set.insert(extname);
        }

        for (uint32_t i = 0; i < glfw_extensions_cnt; i++) {
            std::string required_ext{glfw_extensions[i]};
            // #ifndef NDEBUG
            //             std::cout << "required: " << required_ext <<
            //             std::endl;
            // #endif
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
        auto& instance = vk_instance.value();

        VkSurfaceKHR surface;

        if (glfwCreateWindowSurface(*instance, window, nullptr, &surface) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }

        vk_surface = {instance, surface};
    }

    void initDevice() {
        auto& instance = vk_instance.value();

        uint32_t graphics_queue_family_idx;

        for (auto phys_dev : instance.enumeratePhysicalDevices()) {
            graphics_queue_family_idx = findGraphicsQueueFamilyIndex(
                phys_dev.getQueueFamilyProperties());

            if (glfwGetPhysicalDevicePresentationSupport(
                    static_cast<VkInstance>(*instance),
                    static_cast<VkPhysicalDevice>(*phys_dev),
                    graphics_queue_family_idx) == GLFW_TRUE) {
                vk_physical_device = phys_dev;
            }
        }

        if (!vk_physical_device.has_value()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        auto& physical_device = vk_physical_device.value();

        uint32_t queue_count = 1;
        float queue_priority = 0.0f;

        std::vector<const char*>& device_layers = instance_layers;
        std::vector<const char*> device_extensions{};

        vk::DeviceQueueCreateInfo device_queue_create_info(
            vk::DeviceQueueCreateFlags(), graphics_queue_family_idx,
            queue_count, &queue_priority);

        vk::DeviceCreateInfo device_create_info(
            vk::DeviceCreateFlags(), device_queue_create_info, instance_layers,
            device_extensions);

        vk_device = physical_device.createDevice(device_create_info);
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