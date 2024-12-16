#pragma once
#include <fstream>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {0, sizeof(Vertex)};
    }

    static std::array<vk::VertexInputAttributeDescription, 2>
    getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2>
            attributeDescriptions{};

        attributeDescriptions[0] = vk::VertexInputAttributeDescription(
            0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos));

        attributeDescriptions[1] = vk::VertexInputAttributeDescription(
            1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color));

        return attributeDescriptions;
    }
};

struct SurfaceInfo {
    // vk::SurfaceCapabilitiesKHR capabilities;
    vk::Format color_format;
    vk::ColorSpaceKHR color_space;
    vk::PresentModeKHR present_mode;
    vk::Extent2D extent;
    uint32_t min_image_cnt;
    uint32_t max_image_cnt;

    static SurfaceInfo from(vk::raii::PhysicalDevice &device,
                            vk::raii::SurfaceKHR &surface) {
        auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
        auto surface_formats = device.getSurfaceFormatsKHR(surface);
        auto surface_present_modes = device.getSurfacePresentModesKHR(surface);

        auto selected_format = surface_formats[0];
        for (auto format : surface_formats) {
            if ((format.format == vk::Format::eB8G8R8A8Srgb) &&
                (format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)) {
                selected_format = format;
                break;
            }
        }

        auto selected_mode = surface_present_modes[0];
        for (auto mode : surface_present_modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                selected_mode = mode;
            }
        }

        return {selected_format.format,
                selected_format.colorSpace,
                selected_mode,
                capabilities.currentExtent,
                capabilities.minImageCount,
                capabilities.maxImageCount};
    }
};

struct QueueFamiliesInfo {
    uint32_t graphics_family_idx;
    uint32_t present_family_idx;

    static std::optional<QueueFamiliesInfo> from(
        vk::raii::PhysicalDevice &device, vk::raii::SurfaceKHR &surface) {
        auto families = device.getQueueFamilyProperties();
        std::optional<uint32_t> graphics;
        std::optional<uint32_t> present;

        for (uint32_t i = 0; i < families.size(); i++) {
            if (families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                graphics = i;
            }

            if (device.getSurfaceSupportKHR(i, surface)) {
                present = i;
            }
        }

        if (!graphics.has_value() || !present.has_value()) {
            return std::nullopt;
        }

        return QueueFamiliesInfo{graphics.value(), present.value()};
    }
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const *pCallbackData,
    void * /*pUserData*/) {
    switch (static_cast<uint32_t>(pCallbackData->messageIdNumber)) {
        case 0x822806fa:
            return vk::False;
        case 0xe8d1a9fe:
            return vk::False;
    }

    std::cerr << vk::to_string(
                     static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(
                         messageSeverity))
              << ": "
              << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(
                     messageTypes))
              << ":\n";
    std::cerr << std::string("\t") << "messageIDName   = <"
              << pCallbackData->pMessageIdName << ">\n";
    std::cerr << std::string("\t")
              << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    std::cerr << std::string("\t") << "message         = <"
              << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount) {
        std::cerr << std::string("\t") << "Queue Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++) {
            std::cerr << std::string("\t\t") << "labelName = <"
                      << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount) {
        std::cerr << std::string("\t") << "CommandBuffer Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
            std::cerr << std::string("\t\t") << "labelName = <"
                      << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount) {
        std::cerr << std::string("\t") << "Objects:\n";
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
            std::cerr << std::string("\t\t") << "Object " << i << "\n";
            std::cerr << std::string("\t\t\t") << "objectType   = "
                      << vk::to_string(static_cast<vk::ObjectType>(
                             pCallbackData->pObjects[i].objectType))
                      << "\n";
            std::cerr << std::string("\t\t\t") << "objectHandle = "
                      << pCallbackData->pObjects[i].objectHandle << "\n";
            if (pCallbackData->pObjects[i].pObjectName) {
                std::cerr << std::string("\t\t\t") << "objectName   = <"
                          << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }
    return vk::False;
}

std::vector<char> loadShaderBytes(const std::string &path) {
    std::ifstream shader_file(path, std::ios::binary);

    shader_file.seekg(0, std::ios_base::end);
    std::size_t const shader_file_size = shader_file.tellg();
    assert(shader_file_size % sizeof(std::uint32_t) == 0);

    std::vector<char> binary(shader_file_size);
    shader_file.seekg(0, std::ios_base::beg);
    shader_file.read(binary.data(), shader_file_size);

    return binary;
}

uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                        vk::PhysicalDeviceMemoryProperties memProperties) {
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}