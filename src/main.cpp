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

const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<Vertex> VERTICES = {
    {{0.5, 0.5}, {1, 0, 0}},
    {{0, 0}, {0, 0, 1}},
    {{0.5, -0.5}, {0, 1, 0}},

    {{0, 0}, {0, 0, 1}},
    {{-0.5, 0.5}, {0, 1, 0}},
    {{-0.5, -0.5}, {1, 0, 0}},

    {{-0.08, 0}, {0, 0, 0}},
    {{-0.44, 0.34}, {0, 0, 0}},
    {{-0.44, -0.34}, {0, 0, 0}},

    // {{0.08, 0}, {0, 0, 0}},
    // {{0.44, 0.34}, {0, 0, 0}},
    // {{0.44, -0.34}, {0, 0, 0}},
};

class App {
   public:
    /// @brief Initializes GLFW and Vulkan components in the correct order
    void init() {
        initGlfw();
        gatherVkLayers();
        gatherVkExtensions();
        initInstance();
#ifndef NDEBUG
        initValidation();
#endif
        initWindow();
        initSurface();
        initPhysicalDevice();
        initDevice();
        initVertexBuffer();
        createSyncObjects();
        createRenderPass();
        createPipeline();
    }

    /// @brief Runs window's event loop
    void runLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (swapchain_rebuild_needed) {
                rebuildSwapchain();
            }

            drawFrame();
        }

        vk_device.value().waitIdle();
    }

    ~App() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

   private:
    const vk::raii::Context vk_context{};
    bool window_changed_size = false;
    bool swapchain_rebuild_needed = true;
    uint32_t current_frame = 0;

    std::optional<vk::raii::Instance> vk_instance;
    std::optional<vk::raii::SurfaceKHR> vk_surface;
    std::optional<vk::raii::PhysicalDevice> vk_physical_device;
    std::optional<vk::raii::Device> vk_device;
    std::optional<vk::raii::SwapchainKHR> vk_swapchain;
    std::optional<QueueFamiliesInfo> vk_q_families_info;
    std::optional<vk::raii::Queue> vk_graphics_queue;
    std::optional<vk::raii::Queue> vk_present_queue;
    std::optional<vk::raii::CommandPool> vk_cmd_pool;
    std::optional<SurfaceInfo> vk_surface_info;
    std::optional<vk::raii::RenderPass> vk_render_pass;
    std::optional<vk::raii::Pipeline> vk_pipeline;

    vk::Optional<const vk::raii::PipelineCache> vk_pipeline_cache{nullptr};

    std::optional<vk::raii::CommandBuffers> vk_cmd_buffers;
    std::vector<vk::Image> vk_sc_images;
    std::vector<vk::raii::ImageView> vk_sc_imageviews;
    std::vector<vk::raii::Framebuffer> vk_sc_framebuffers;
    std::optional<vk::raii::Buffer> vk_vertex_buffer;
    std::optional<vk::raii::DeviceMemory> vk_vb_memory;

    std::vector<vk::raii::Semaphore> vk_image_available_sema;
    std::vector<vk::raii::Semaphore> vk_render_finished_sema;
    std::vector<vk::raii::Fence> vk_fences;

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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window = glfwCreateWindow(800, 600, "App", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width,
                                          int height) {
        auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
        app->swapchain_rebuild_needed = true;
    }

    /// @brief Creates a Vulkan surface on the window
    void initSurface() {
        auto& instance = vk_instance.value();

        VkSurfaceKHR* surface = new VkSurfaceKHR();

        if (glfwCreateWindowSurface(*instance, window, nullptr, surface) !=
            VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }

        vk::raii::SurfaceKHR raii_surface(instance, *surface);

        vk_surface = std::move(raii_surface);
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

        vk_surface_info =
            SurfaceInfo::from(vk_physical_device.value(), surface);
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

        vk::CommandPoolCreateInfo cmd_pool_info(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queues_info.graphics_family_idx);
        vk_cmd_pool = device.createCommandPool(cmd_pool_info);

        vk::CommandBufferAllocateInfo cmd_buffer_info{};
        cmd_buffer_info.setCommandPool(vk_cmd_pool.value());
        cmd_buffer_info.setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);

        vk_cmd_buffers = vk::raii::CommandBuffers(device, cmd_buffer_info);
    }

    void initVertexBuffer() {
        auto& phys_dev = vk_physical_device.value();
        auto& device = vk_device.value();

        vk::BufferCreateInfo vb_info({}, sizeof(VERTICES[0]) * VERTICES.size(),
                                     vk::BufferUsageFlagBits::eVertexBuffer,
                                     vk::SharingMode::eExclusive);
        vk_vertex_buffer = device.createBuffer(vb_info);

        auto mem_req = vk_vertex_buffer->getMemoryRequirements();
        auto phys_mem_props = phys_dev.getMemoryProperties();

        vk::MemoryAllocateInfo alloc_info(
            mem_req.size,
            findMemoryType(mem_req.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent,
                           phys_mem_props));

        vk_vb_memory = device.allocateMemory(alloc_info);
        vk_vertex_buffer->bindMemory(*vk_vb_memory, 0);
        std::memcpy(vk_vb_memory->mapMemory(0, vb_info.size), VERTICES.data(),
                    (size_t)vb_info.size);
        vk_vb_memory->unmapMemory();
    }

    void rebuildSwapchain() {
        auto& device = vk_device.value();

        device.waitIdle();
        destroySwapchain();

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        device.waitIdle();
        initSurface();

        vk_surface_info.value().extent.width = width;
        vk_surface_info.value().extent.height = height;

        device.waitIdle();

        createSwapchain();
        createImageViews();
        createFrameBuffers();

        swapchain_rebuild_needed = false;
    }

    void createSwapchain() {
        auto& phys_device = vk_physical_device.value();
        auto& device = vk_device.value();
        auto& surface = vk_surface.value();
        auto& queues_info = vk_q_families_info.value();
        auto& surface_info = vk_surface_info.value();

        uint32_t family_idxs[2] = {queues_info.graphics_family_idx,
                                   queues_info.present_family_idx};

        auto img_sharing_mode = vk::SharingMode::eExclusive;
        if (family_idxs[0] != family_idxs[1]) {
            img_sharing_mode = vk::SharingMode::eConcurrent;
        }

        auto img_cnt = surface_info.min_image_cnt + 1;
        if (surface_info.max_image_cnt != 0) {
            img_cnt = std::min(surface_info.max_image_cnt, img_cnt);
        }

        vk::SwapchainCreateInfoKHR swapchain_info{
            {},
            surface,
            img_cnt,
            surface_info.color_format,
            surface_info.color_space,
            surface_info.extent,
            1,
            vk::ImageUsageFlagBits::eColorAttachment,
            img_sharing_mode,
            family_idxs};

        swapchain_info.setPresentMode(surface_info.present_mode);

        if (vk_swapchain.has_value()) {
            swapchain_info.setOldSwapchain(vk_swapchain.value());
        }

        vk_sc_framebuffers.clear();
        vk_sc_imageviews.clear();
        vk_swapchain = device.createSwapchainKHR(swapchain_info);
        vk_sc_images = vk_swapchain->getImages();
    }

    void destroySwapchain() {
        vk_sc_framebuffers.clear();
        vk_sc_imageviews.clear();
        vk_sc_images.clear();
        vk_swapchain.reset();
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

        auto vertex_shader_data = loadShaderBytes("shaders/vert.spv");
        auto frag_shader_data = loadShaderBytes("shaders/lab.spv");

        vk::ShaderModuleCreateInfo vert_shader(
            {}, vertex_shader_data.size(),
            reinterpret_cast<std::uint32_t const*>(vertex_shader_data.data()));

        vk::ShaderModuleCreateInfo frag_shader(
            {}, frag_shader_data.size(),
            reinterpret_cast<std::uint32_t const*>(frag_shader_data.data()));

        vk::raii::ShaderModule vert_shader_module =
            device.createShaderModule(vert_shader);

        vk::raii::ShaderModule frag_shader_module =
            device.createShaderModule(frag_shader);

        vk::PipelineShaderStageCreateInfo vert_shader_stage(
            {}, vk::ShaderStageFlagBits::eVertex, vert_shader_module, "main");

        vk::PipelineShaderStageCreateInfo frag_shader_stage(
            {}, vk::ShaderStageFlagBits::eFragment, frag_shader_module,
            "fragment_main");

        std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
            vert_shader_stage, frag_shader_stage};

        vk::PipelineVertexInputStateCreateInfo vis_info{};
        auto v_bind_desc = Vertex::getBindingDescription();
        auto v_attr_descs = Vertex::getAttributeDescriptions();
        vis_info.setVertexBindingDescriptions(v_bind_desc);
        vis_info.setVertexAttributeDescriptions(v_attr_descs);

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
        auto& extent = vk_surface_info.value().extent;

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

    void createSyncObjects() {
        auto& device = vk_device.value();

        vk_image_available_sema.clear();
        vk_render_finished_sema.clear();
        vk_fences.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk_fences.push_back(
                device.createFence({vk::FenceCreateFlagBits::eSignaled}));
            vk_image_available_sema.push_back(device.createSemaphore({}));
            vk_render_finished_sema.push_back(device.createSemaphore({}));
        }
    }

    void overwriteCommandBuffer(uint32_t buffer_idx, uint32_t frame_idx) {
        auto& extent = vk_surface_info.value().extent;
        auto& cmd_buf = vk_cmd_buffers.value()[buffer_idx];
        auto& rpass = vk_render_pass.value();
        auto& fbuf = vk_sc_framebuffers[frame_idx];
        auto& pipeline = vk_pipeline.value();
        auto& vertex_buffer = vk_vertex_buffer.value();

        vk::Rect2D rect({0, 0}, extent);
        vk::ClearColorValue clear_color({0.0f, 0.0f, 0.0f, 1.0f});
        vk::ClearValue clear_value(clear_color);
        vk::RenderPassBeginInfo rpb_info(rpass, fbuf, rect, clear_value);
        vk::Viewport viewport(rect.offset.x, rect.offset.y, rect.extent.width,
                              rect.extent.height, 0, 1);

        cmd_buf.reset();
        cmd_buf.begin({});

        {
            cmd_buf.beginRenderPass(rpb_info, vk::SubpassContents::eInline);
            cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
            cmd_buf.setViewport(0, viewport);
            cmd_buf.setScissor(0, rect);
            cmd_buf.bindVertexBuffers(0, *vertex_buffer, {0});
            cmd_buf.draw(static_cast<uint32_t>(VERTICES.size()), 1, 0, 0);
            cmd_buf.endRenderPass();
        }

        cmd_buf.end();
    }

    void drawFrame() {
        auto& phys_device = vk_physical_device.value();
        auto& device = vk_device.value();
        auto& swapch = vk_swapchain.value();
        auto& ima_semaphor = vk_image_available_sema[current_frame];
        auto& rf_semaphor = vk_render_finished_sema[current_frame];
        auto& cmd_buf = vk_cmd_buffers.value()[current_frame];
        auto& queue = vk_graphics_queue.value();
        auto& fence_previous =
            vk_fences[(current_frame + 1) % MAX_FRAMES_IN_FLIGHT];
        auto& fence_current = vk_fences[current_frame];

        vk::AcquireNextImageInfoKHR ani_info(swapch, UINT32_MAX, ima_semaphor,
                                             nullptr, 1);
        uint32_t image_index;

        try {
            auto res_imgidx = device.acquireNextImage2KHR(ani_info);
            image_index = res_imgidx.second;
        } catch (vk::OutOfDateKHRError err) {
            swapchain_rebuild_needed = true;
            return;
        }

        overwriteCommandBuffer(current_frame, image_index);

        vk::PipelineStageFlags stage_flags(
            vk::PipelineStageFlagBits::eColorAttachmentOutput);
        vk::SubmitInfo submit_info(*ima_semaphor, stage_flags, *cmd_buf,
                                   *rf_semaphor);

        queue.submit(submit_info);

        vk::PresentInfoKHR present_info(*rf_semaphor, *swapch, image_index);

        try {
            if (queue.presentKHR(present_info) == vk::Result::eSuboptimalKHR) {
                swapchain_rebuild_needed = true;
            }
        } catch (vk::OutOfDateKHRError err) {
            swapchain_rebuild_needed = true;
        }

        queue.waitIdle();

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
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