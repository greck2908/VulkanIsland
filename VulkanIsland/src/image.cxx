#define _SCL_SECURE_NO_WARNINGS

#include "image.h"
#include "resource.h"


[[nodiscard]] std::optional<VkFormat> FindDepthImageFormat(VkPhysicalDevice physicalDevice) noexcept
{
    return FindSupportedImageFormat(
        physicalDevice,
        make_array(VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT),
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

[[nodiscard]] std::optional<VkImage>
CreateImageHandle(VulkanDevice const &vulkanDevice, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels,
                              VkFormat format, VkImageTiling tiling, VkBufferUsageFlags usage) noexcept
{
    VkImageCreateInfo const createInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr, 0,
        VK_IMAGE_TYPE_2D,
        format,
        { width, height, 1 },
        mipLevels,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        tiling,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    std::optional<VkImage> image;

    VkImage handle;

    if (auto result = vkCreateImage(vulkanDevice.handle(), &createInfo, nullptr, &handle); result != VK_SUCCESS)
        std::cerr << "failed to create image: "s << result << '\n';

    else image.emplace(handle);

#if USE_DEBUG_MARKERS
    VkDebugMarkerObjectNameInfoEXT const info{
        VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
        nullptr,
        VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT,
        (std::uint64_t)handle,//static_cast<std::uint64_t>(handle),
        "image"
    };

    auto vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(vulkanDevice.handle(), "vkDebugMarkerSetObjectNameEXT");

    if (auto result = vkDebugMarkerSetObjectNameEXT(vulkanDevice.handle(), &info); result != VK_SUCCESS)
        throw std::runtime_error("failed to set the image debug marker: "s + std::to_string(result));
#endif

    return image;
}

[[nodiscard]] std::optional<VulkanTexture>
CreateTexture(VulkanDevice &device, VkFormat format, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels, VkImageTiling tiling,
              VkImageAspectFlags aspectFlags, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags)
{
    std::optional<VulkanTexture> texture;

    if (auto image = device.resourceManager().CreateImage(format, width, height, mipLevels, tiling, usageFlags, propertyFlags); image)
        if (auto view = device.resourceManager().CreateImageView(*image, aspectFlags); view)
            texture.emplace(image, *view);

    /*auto sampler = CreateImageSampler(app.vulkanDevice->handle(), image->mipLevels);

    if (!sampler)
        return { };*/

    return texture;
}


[[nodiscard]] std::pair<VkBuffer, std::shared_ptr<DeviceMemory>> StageImage(VulkanDevice &device, RawImage const &rawImage) noexcept
{
    VkBuffer buffer;

    auto memory = std::visit([&] (auto &&texels)
    {
        auto constexpr usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        auto constexpr propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        using texel_t = typename std::decay_t<decltype(texels)>::value_type;

        auto const bufferSize = static_cast<VkDeviceSize>(std::size(texels) * sizeof(texel_t));

        auto memory = BufferPool::CreateBuffer(device, buffer, bufferSize, usageFlags, propertyFlags);

        if (memory) {
            void *data;

            if (auto result = vkMapMemory(device.handle(), memory->handle(), memory->offset(), memory->size(), 0, &data); result != VK_SUCCESS)
                std::cerr << "failed to map image buffer memory: "s << result << '\n';

            else {
                std::uninitialized_copy(std::begin(texels), std::end(texels), reinterpret_cast<texel_t *>(data));
                vkUnmapMemory(device.handle(), memory->handle());
            }
        }

        return memory;

    }, rawImage.data);

    return std::make_pair(buffer, memory);
}

