#pragma once

#include <vector>
#include <array>
#include <string>
#include <string_view>

#include "helpers.h"

#define GLFW_EXPOSE_NATIVE_WIN32

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include "debug.h"



class VulkanInstance final {
public:

    template<class E, class L>
    VulkanInstance(E &&extensions, L &&layers);
    ~VulkanInstance();

    VkInstance handle() noexcept;
         
private:

    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugReportCallbackEXT debugReportCallback_{VK_NULL_HANDLE};

    VulkanInstance() = delete;
    VulkanInstance(VulkanInstance const &) = delete;
    VulkanInstance(VulkanInstance &&) = delete;

    void CreateInstance(std::vector<char const *> &&extensions, std::vector<char const *> &&layers);
};

template<class E, class L>
inline VulkanInstance::VulkanInstance(E &&extensions, L &&layers)
{
    auto constexpr use_extensions = !std::is_same_v<std::false_type, E>;
    auto constexpr use_layers = !std::is_same_v<std::false_type, L>;

    std::vector<char const *> extensions_;
    std::vector<char const *> layers_;

    if constexpr (use_extensions)
    {
        using T = std::decay_t<E>;
        static_assert(is_container_v<T>, "'extensions' must be a container");
        static_assert(std::is_same_v<typename std::decay_t<T>::value_type, char const *>, "'extensions' must contain null-terminated strings");

        if constexpr (use_layers)
        {
            auto present = std::any_of(std::cbegin(extensions), std::cend(extensions), [] (auto &&name)
            {
                return std::strcmp(name, VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0;
            });

            if (!present)
                throw std::runtime_error("enabled validation layers require enabled 'VK_EXT_debug_report' extension"s);
        }

        if constexpr (std::is_rvalue_reference_v<T>)
            std::move(extensions.begin(), extensions.end(), std::back_inserter(extensions_));

        else std::copy(extensions.begin(), extensions.end(), std::back_inserter(extensions_));

    }

    if constexpr (use_layers)
    {
        using T = std::decay_t<L>;
        static_assert(is_container_v<T>, "'layers' must be a container");
        static_assert(std::is_same_v<typename std::decay_t<T>::value_type, char const *>, "'layers' must contain null-terminated strings");

        if constexpr (std::is_rvalue_reference_v<T>)
            std::move(layers.begin(), layers.end(), std::back_inserter(layers_));

        else std::copy(layers.begin(), layers.end(), std::back_inserter(layers_));
    }

    CreateInstance(std::move(extensions_), std::move(layers_));
}

inline VkInstance VulkanInstance::handle() noexcept
{
    return instance_;
}