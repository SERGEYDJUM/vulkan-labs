#pragma once

uint32_t findGraphicsQueueFamilyIndex(
    std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties) {
    // get the first index into queueFamiliyProperties which supports graphics
    std::vector<vk::QueueFamilyProperties>::const_iterator
        graphicsQueueFamilyProperty = std::find_if(
            queueFamilyProperties.begin(), queueFamilyProperties.end(),
            [](vk::QueueFamilyProperties const& qfp) {
                return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
            });
    assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(),
                                               graphicsQueueFamilyProperty));
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* /*pUserData*/) {
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