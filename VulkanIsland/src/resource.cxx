#include "resource.h"

std::shared_ptr<VulkanImage>
ResourceManager::CreateImage(VkFormat format, std::uint16_t width, std::uint16_t height, std::uint32_t mipLevels,
                             VkImageTiling tiling, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags)
{
    std::shared_ptr<VulkanImage> image;

    auto handle = CreateImageHandle(device_, width, height, mipLevels, format, tiling, usageFlags);

    if (handle) {
        auto memory = device_.memoryManager().AllocateMemory(*handle, propertyFlags, tiling == VK_IMAGE_TILING_LINEAR);

        if (memory) {
            if (auto result = vkBindImageMemory(device_.handle(), *handle, memory->handle(), memory->offset()); result != VK_SUCCESS)
                std::cerr << "failed to bind image buffer memory: "s << result << '\n';

            else image.reset(
                new VulkanImage{memory, *handle, format, mipLevels, width, height},
                [this] (VulkanImage *ptr_image)
                {
                    FreeImage(*ptr_image);

                    delete ptr_image;
                }
            );
        }
    }

    return image;
}

std::optional<VulkanImageView>
ResourceManager::CreateImageView(VulkanImage const &image, VkImageViewType type, VkImageAspectFlags aspectFlags) noexcept
{
    std::optional<VulkanImageView> view;

    VkImageViewCreateInfo const createInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr, 0,
        image.handle(),
        type,
        image.format(),
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { aspectFlags, 0, image.mipLevels(), 0, 1 }
    };

    VkImageView handle;

    if (auto result = vkCreateImageView(device_.handle(), &createInfo, nullptr, &handle); result != VK_SUCCESS)
        std::cerr << "failed to create image view: "s << result << '\n';

    else view.emplace(handle, type);

    return view;
}

std::shared_ptr<VulkanSampler>
ResourceManager::CreateImageSampler(std::uint32_t mipLevels) noexcept
{
    std::shared_ptr<VulkanSampler> sampler;

    VkSamplerCreateInfo const createInfo{
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr, 0,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.f,
        VK_TRUE, 16.f,
        VK_FALSE, VK_COMPARE_OP_ALWAYS,
        0.f, static_cast<float>(mipLevels),
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        VK_FALSE
    };

    VkSampler handle;

    if (auto result = vkCreateSampler(device_.handle(), &createInfo, nullptr, &handle); result != VK_SUCCESS)
        std::cerr << "failed to create sampler: "s << result << '\n';

    else sampler.reset(
        new VulkanSampler{handle},
        [this] (VulkanSampler *ptr_sampler)
        {
            FreeSampler(*ptr_sampler);

            delete ptr_sampler;
        }
    );

    return sampler;
}


std::shared_ptr<VulkanBuffer>
ResourceManager::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) noexcept
{
    std::shared_ptr<VulkanBuffer> buffer;

    auto handle = CreateBufferHandle(device_, size, usage, properties);

    if (handle) {
        auto memory = device_.memoryManager().AllocateMemory(*handle, properties, false);

        if (memory) {
            if (auto result = vkBindBufferMemory(device_.handle(), *handle, memory->handle(), memory->offset()); result != VK_SUCCESS)
                std::cerr << "failed to bind buffer memory: "s << result << '\n';

            else buffer.reset(
                new VulkanBuffer{memory, *handle},
                [this] (VulkanBuffer *ptr_buffer)
                {
                    FreeBuffer(*ptr_buffer);

                    delete ptr_buffer;
                }
            );
        }
    }

    return buffer;
}


void ResourceManager::FreeImage(VulkanImage const &image) noexcept
{
    vkDestroyImage(device_.handle(), image.handle(), nullptr);
    image.memory().reset();
}

void ResourceManager::FreeSampler(VulkanSampler const &sampler) noexcept
{
    vkDestroySampler(device_.handle(), sampler.handle(), nullptr);
}

void ResourceManager::FreeImageView(VulkanImageView const &view) noexcept
{
    vkDestroyImageView(device_.handle(), view.handle(), nullptr);
}


void ResourceManager::FreeBuffer(VulkanBuffer const &buffer) noexcept
{
    vkDestroyBuffer(device_.handle(), buffer.handle(), nullptr);
    buffer.memory().reset();
}