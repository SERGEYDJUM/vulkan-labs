#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

// Ensure inclusion after Vulkan
#include <GLFW/glfw3.h>

#include "utils.hpp"

const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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
        initSurface();
        initPhysicalDevice();
        initDevice();

        rebuildGraphics();
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
    const vk::Extent2D window_extent{800, 600};
    const vk::raii::Context vk_context{};

    std::optional<vk::raii::Instance> vk_instance;
    std::optional<vk::raii::SurfaceKHR> vk_surface;
    std::optional<vk::raii::PhysicalDevice> vk_physical_device;
    std::optional<vk::raii::Device> vk_device;
    std::optional<vk::raii::SwapchainKHR> vk_swapchain;
    std::optional<QueueFamiliesInfo> vk_q_families_info;
    std::optional<vk::raii::Queue> vk_graphics_queue;
    std::optional<vk::raii::Queue> vk_present_queue;
    std::optional<vk::raii::CommandPool> vk_cmd_pool;
    std::optional<vk::raii::CommandBuffer> vk_cmd_buffer;
    std::optional<SurfaceInfo> vk_surface_info;
    std::optional<vk::raii::RenderPass> vk_render_pass;
    std::optional<vk::raii::Pipeline> vk_pipeline;

    vk::Optional<const vk::raii::PipelineCache> vk_pipeline_cache{nullptr};

    std::vector<vk::Image> vk_sc_images;
    std::vector<vk::raii::ImageView> vk_sc_imageviews;
    std::vector<vk::raii::Framebuffer> vk_sc_framebuffers;

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
        vk::ApplicationInfo app_info{"App", 1, "Engine", 1, vk::ApiVersion13};
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
        window = glfwCreateWindow(window_extent.width, window_extent.height,
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

    bool checkDeviceExtensions(vk::raii::PhysicalDevice& phys_dev) {
        std::unordered_set<std::string> extension_set{};
        for (auto ext : phys_dev.enumerateDeviceExtensionProperties()) {
            std::string extname{static_cast<const char*>(ext.extensionName)};
            extension_set.insert(extname);
        }

        for (auto req_ext : REQUIRED_DEVICE_EXTENSIONS) {
            if (extension_set.count(req_ext) == 0) {
                return false;
            }
        }

        return true;
    }

    void initPhysicalDevice() {
        auto& instance = vk_instance.value();
        auto& surface = vk_surface.value();

        for (auto phys_dev : instance.enumeratePhysicalDevices()) {
            auto queues_info = QueueFamiliesInfo::from(phys_dev, surface);

            if (!queues_info.has_value() || !checkDeviceExtensions(phys_dev)) {
                continue;
            }

            if (glfwGetPhysicalDevicePresentationSupport(
                    static_cast<VkInstance>(*instance),
                    static_cast<VkPhysicalDevice>(*phys_dev),
                    queues_info->present_family_idx) != GLFW_TRUE) {
                continue;
            }

            vk_physical_device = phys_dev;
            vk_q_families_info = queues_info;
            break;
        }

        if (!vk_physical_device.has_value()) {
            throw std::runtime_error(
                "failed to find GPU with graphics and presentation support");
        }
    }

    void initDevice() {
        auto& instance = vk_instance.value();
        auto& physical_device = vk_physical_device.value();
        auto& queues_info = vk_q_families_info.value();

        uint32_t queue_count = 1;
        float queue_priority = 1.0f;

        std::vector<vk::DeviceQueueCreateInfo> qc_infos{};
        for (auto idx : {queues_info.graphics_family_idx,
                         queues_info.present_family_idx}) {
            vk::DeviceQueueCreateInfo dqci{
                {}, idx, queue_count, &queue_priority};
            qc_infos.emplace_back(dqci);
        }

        vk::DeviceCreateInfo device_create_info({}, qc_infos, instance_layers,
                                                REQUIRED_DEVICE_EXTENSIONS);

        vk_device = physical_device.createDevice(device_create_info);

        auto& device = vk_device.value();

        vk_graphics_queue = device.getQueue(queues_info.graphics_family_idx, 0);
        vk_present_queue = device.getQueue(queues_info.present_family_idx, 0);

        vk::CommandPoolCreateInfo cmd_pool_info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queues_info.graphics_family_idx);
        vk_cmd_pool = device.createCommandPool(cmd_pool_info);

        vk::CommandBufferAllocateInfo cmd_buffer_info{};
        cmd_buffer_info.setCommandPool(vk_cmd_pool.value());
        cmd_buffer_info.setCommandBufferCount(1);
        vk_cmd_buffer = std::move(device.allocateCommandBuffers(cmd_buffer_info)[0]);
    }

    void rebuildGraphics() {
        createSwapchain();
        createImageViews();
        createRenderPass();
        createPipeline();
        createFrameBuffers();
    }

    void createSwapchain() {
        auto& phys_device = vk_physical_device.value();
        auto& device = vk_device.value();
        auto& surface = vk_surface.value();
        auto& queues_info = vk_q_families_info.value();

        auto surface_info = SurfaceInfo::from(phys_device, surface);

        uint32_t family_idxs[2] = {queues_info.graphics_family_idx,
                                   queues_info.present_family_idx};

        auto img_sharing_mode = vk::SharingMode::eExclusive;
        if (family_idxs[0] != family_idxs[1]) {
            img_sharing_mode = vk::SharingMode::eConcurrent;
        }

        auto img_cnt = surface_info.capabilities.minImageCount + 1;
        if (surface_info.capabilities.maxImageCount != 0) {
            img_cnt =
                std::min(surface_info.capabilities.maxImageCount, img_cnt);
        }

        vk::SwapchainCreateInfoKHR swapchain_info{
            {},
            surface,
            img_cnt,
            surface_info.color_format,
            surface_info.color_space,
            surface_info.capabilities.currentExtent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            img_sharing_mode,
            family_idxs};

        swapchain_info.setPresentMode(surface_info.present_mode);

        if (vk_swapchain.has_value()) {
            swapchain_info.setOldSwapchain(vk_swapchain.value());
        }

        vk_swapchain = device.createSwapchainKHR(swapchain_info);
        vk_sc_images = vk_swapchain->getImages();
        vk_surface_info = surface_info;
    }

    void createImageViews() {
        auto& srfc_info = vk_surface_info.value();
        auto& device = vk_device.value();

        vk::ImageSubresourceRange is_range{};
        is_range.setAspectMask(vk::ImageAspectFlagBits::eColor);
        is_range.setLayerCount(1);
        is_range.setLevelCount(1);

        vk_sc_imageviews.clear();

        std::transform(
            vk_sc_images.begin(), vk_sc_images.end(),
            std::back_inserter(vk_sc_imageviews), [&](vk::Image& img) {
                vk::ImageViewCreateInfo imv_info(
                    {}, img, vk::ImageViewType::e2D, srfc_info.color_format);
                imv_info.setSubresourceRange(is_range);
                return device.createImageView(imv_info);
            });
    }

    void createRenderPass() {
        auto& device = vk_device.value();
        auto& surface_info = vk_surface_info.value();

        vk::AttachmentDescription attach_desc{};
        attach_desc.setFormat(surface_info.color_format);
        attach_desc.setLoadOp(vk::AttachmentLoadOp::eClear);
        attach_desc.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        attach_desc.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        attach_desc.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference attach_ref(
            0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription sp_desc{};
        sp_desc.setColorAttachments(attach_ref);

        vk::RenderPassCreateInfo rp_info{};
        rp_info.setAttachments(attach_desc);
        rp_info.setSubpasses(sp_desc);

        vk_render_pass = device.createRenderPass(rp_info);
    }

    void createPipeline() {
        auto& device = vk_device.value();
        auto& render_pass = vk_render_pass.value();

        auto shader_data = loadShaderBytes("shaders/lab.spv");

        vk::ShaderModuleCreateInfo shader(
            {}, shader_data.size(),
            reinterpret_cast<std::uint32_t const*>(shader_data.data()));

        vk::raii::ShaderModule shader_module =
            device.createShaderModule(shader);

        vk::PipelineShaderStageCreateInfo vert_shader_stage(
            {}, vk::ShaderStageFlagBits::eVertex, shader_module, "vertex_main");

        vk::PipelineShaderStageCreateInfo frag_shader_stage(
            {}, vk::ShaderStageFlagBits::eFragment, shader_module,
            "fragment_main");

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
            vert_shader_stage, frag_shader_stage};

        vk::PipelineVertexInputStateCreateInfo vis_info{};
        vk::PipelineInputAssemblyStateCreateInfo iss_info(
            {}, vk::PrimitiveTopology::eTriangleList, false);
        vk::PipelineViewportStateCreateInfo vps_info{};
        vps_info.setViewportCount(1);
        vps_info.setScissorCount(1);

        vk::PipelineRasterizationStateCreateInfo raster_info{};
        raster_info.setLineWidth(1.0f);
        raster_info.setCullMode(vk::CullModeFlagBits::eBack);
        raster_info.setFrontFace(vk::FrontFace::eClockwise);

        vk::PipelineMultisampleStateCreateInfo ms_info{};
        ms_info.setSampleShadingEnable(false);
        ms_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

        vk::PipelineColorBlendAttachmentState cbas{};
        cbas.setColorWriteMask(
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        cbas.setBlendEnable(false);

        vk::PipelineColorBlendStateCreateInfo cbs_info{};
        cbs_info.setLogicOpEnable(false);
        cbs_info.setAttachments(cbas);
        cbs_info.setBlendConstants({0.0f, 0.0f, 0.0f, 0.0f});

        std::vector<vk::DynamicState> dyn_states = {vk::DynamicState::eViewport,
                                                    vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dyns_info({}, dyn_states);

        auto layout =
            device.createPipelineLayout(vk::PipelineLayoutCreateInfo());

        vk::GraphicsPipelineCreateInfo gp_info{};
        gp_info.setStages(shader_stages);
        gp_info.setPVertexInputState(&vis_info);
        gp_info.setPInputAssemblyState(&iss_info);
        gp_info.setPViewportState(&vps_info);
        gp_info.setPRasterizationState(&raster_info);
        gp_info.setPMultisampleState(&ms_info);
        gp_info.setPColorBlendState(&cbs_info);
        gp_info.setPDynamicState(&dyns_info);
        gp_info.setLayout(layout);
        gp_info.setRenderPass(render_pass);
        gp_info.setSubpass(0);

        vk_pipeline = device.createGraphicsPipeline(vk_pipeline_cache, gp_info);
    }

    void createFrameBuffers() {
        auto& device = vk_device.value();
        auto& render_pass = vk_render_pass.value();
        auto& extent = vk_surface_info.value().capabilities.currentExtent;

        vk_sc_framebuffers.clear();

        std::transform(vk_sc_imageviews.begin(), vk_sc_imageviews.end(),
                       std::back_inserter(vk_sc_framebuffers),
                       [&](vk::raii::ImageView& img) {
                           vk::FramebufferCreateInfo fb_info{};
                           fb_info.setRenderPass(render_pass);
                           fb_info.setAttachments(*img);
                           fb_info.setHeight(extent.height);
                           fb_info.setWidth(extent.width);
                           fb_info.setLayers(1);

                           return device.createFramebuffer(fb_info);
                       });
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