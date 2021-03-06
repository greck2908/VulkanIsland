
#include <istream>

#include "main.hxx"
#include "TARGA_loader.hxx"



template<std::size_t N>
std::optional<texel_buffer_t> instantiate_texel_buffer()
{
    return std::make_optional<std::vector<vec<N, std::uint8_t>>>();
}

std::optional<texel_buffer_t> instantiate_texel_buffer(std::uint8_t pixelDepth) noexcept
{
    switch (pixelDepth) {
        case 8:
            return instantiate_texel_buffer<1>();

        case 16:
            return instantiate_texel_buffer<2>();

        case 24:
            return instantiate_texel_buffer<3>();

        case 32:
            return instantiate_texel_buffer<4>();

        default:
            return std::nullopt;
    }
}

constexpr ePIXEL_LAYOUT GetPixelLayout(std::uint8_t pixelDepth) noexcept
{
    switch (pixelDepth) {
        case 8:
            return ePIXEL_LAYOUT::nRED;
            break;

        case 16:
            return ePIXEL_LAYOUT::nRG;
            break;

        case 24:
            return ePIXEL_LAYOUT::nBGR;
            break;

        case 32:
            return ePIXEL_LAYOUT::nBGRA;
            break;

        default:
            return ePIXEL_LAYOUT::nUNDEFINED;
    }
}

constexpr VkFormat GetPixelFormat(ePIXEL_LAYOUT pixelLayout) noexcept
{
    switch (pixelLayout) {
        case ePIXEL_LAYOUT::nRED:
            return VK_FORMAT_R8_UNORM;

        case ePIXEL_LAYOUT::nRG:
            return VK_FORMAT_R8G8_UNORM;

        case ePIXEL_LAYOUT::nRGB:
            return VK_FORMAT_R8G8B8_UNORM;

        case ePIXEL_LAYOUT::nBGR:
            return VK_FORMAT_B8G8R8_UNORM;

        case ePIXEL_LAYOUT::nRGBA:
            return VK_FORMAT_R8G8B8A8_UNORM;

        case ePIXEL_LAYOUT::nBGRA:
            return VK_FORMAT_B8G8R8A8_UNORM;

        case ePIXEL_LAYOUT::nUNDEFINED:
        default:
            return VK_FORMAT_UNDEFINED;
    }
}


void LoadUncompressedTrueColorImage(TARGA &targa, std::ifstream &file)
{
    texel_buffer_t buffer;

    if (auto result = instantiate_texel_buffer(targa.pixelDepth); result)
        buffer = result.value();

    else return;

    targa.pixelLayout = GetPixelLayout(targa.pixelDepth);

    std::visit([&targa, &file] (auto &&buffer)
    {
        buffer.resize(targa.width * targa.height);

        using texel_type = typename std::decay_t<decltype(buffer)>::value_type;

        file.read(reinterpret_cast<char *>(std::data(buffer)), std::size(buffer) * sizeof(texel_type));

        if constexpr (texel_type::size == 3)
        {
            targa.pixelLayout = targa.pixelLayout == ePIXEL_LAYOUT::nRGB ? ePIXEL_LAYOUT::nRGBA : ePIXEL_LAYOUT::nBGRA;

            using vec_type = vec<4, typename texel_type::value_type>;

            std::vector<vec_type> intermidiateBuffer(std::size(buffer));

            std::size_t i = 0;
            for (auto &&texel : buffer)
                intermidiateBuffer.at(++i - 1) = std::move(vec_type{texel.array.at(0), texel.array.at(1), texel.array.at(2), 1});

            targa.data = std::move(intermidiateBuffer);
        }

        else targa.data = std::move(buffer);

    }, std::move(buffer));
}

void LoadUncompressedColorMappedImage(TARGA &targa, std::ifstream &file)
{
    if ((targa.header.colorMapType & 0x01) != 0x01)
        return;

    targa.colorMapDepth = targa.header.colorMapSpec.at(4);

    texel_buffer_t palette;

    if (auto buffer = instantiate_texel_buffer(targa.colorMapDepth); buffer)
        palette = buffer.value();

    else return;

    targa.pixelLayout = GetPixelLayout(targa.colorMapDepth);

    auto colorMapStart = static_cast<std::size_t>((targa.header.colorMapSpec.at(1) << 8) + targa.header.colorMapSpec.at(0));
    auto colorMapLength = static_cast<std::size_t>((targa.header.colorMapSpec.at(3) << 8) + targa.header.colorMapSpec.at(2));

    file.seekg(colorMapStart, std::ios::cur);

    palette = std::visit([&] (auto &&palette) -> texel_buffer_t
    {
        palette.resize(colorMapLength);

        using texel_type = typename std::decay_t<decltype(palette)>::value_type;

        file.read(reinterpret_cast<char *>(std::data(palette)), std::size(palette) * sizeof(texel_type));

        if constexpr (texel_type::size == 3)
        {
            targa.pixelLayout = targa.pixelLayout == ePIXEL_LAYOUT::nRGB ? ePIXEL_LAYOUT::nRGBA : ePIXEL_LAYOUT::nBGRA;

            using vec_type = vec<4, typename texel_type::value_type>;

            std::vector<vec_type> intermidiateBuffer(std::size(palette));

            std::size_t i = 0;
            for (auto &&texel : palette)
                intermidiateBuffer.at(++i - 1) = std::move(vec_type{texel.array.at(0), texel.array.at(1), texel.array.at(2), 1});

            return std::move(intermidiateBuffer);
        }

        else return std::move(palette);

    }, std::move(palette));

    std::visit([&targa, &file] (auto &&palette)
    {
        std::vector<std::size_t> indices(targa.width * targa.height);
        file.read(reinterpret_cast<char *>(std::data(indices)), std::size(indices) * sizeof(std::byte));

        std::decay_t<decltype(palette)> buffer(targa.width * targa.height);

        std::ptrdiff_t begin, end;
        std::size_t colorIndex;

        for (auto it_index = std::cbegin(indices); it_index < std::cend(indices); ) {
            begin = std::distance(std::cbegin(indices), it_index);

            colorIndex = *it_index;

            it_index = std::partition_point(it_index, std::cend(indices), [colorIndex] (auto index) { return index == colorIndex; });

            end = std::distance(std::cbegin(indices), it_index);

            std::fill_n(std::next(std::begin(buffer), begin), end - begin, palette.at(colorIndex));
        }

        targa.data = std::move(buffer);

    }, std::move(palette));
}

[[nodiscard]] std::optional<RawImage> LoadTARGA(std::string_view _name)
{
    auto current_path = fs::current_path();

    fs::path directory{"contents"s};
    fs::path name{std::data(_name)};

    if (!fs::exists(current_path / directory))
        directory = current_path / fs::path{"../"s} / directory;

    std::ifstream file((directory / name).native(), std::ios::binary);

    if (!file.is_open())
        return { };

    TARGA targa;

    file.read(reinterpret_cast<char *>(&targa.header), sizeof(targa.header));

    targa.width = static_cast<std::int16_t>((targa.header.imageSpec.at(5) << 8) + targa.header.imageSpec.at(4));
    targa.height = static_cast<std::int16_t>((targa.header.imageSpec.at(7) << 8) + targa.header.imageSpec.at(6));
    targa.pixelDepth = targa.header.imageSpec.at(8);

    if (targa.width * targa.height * targa.pixelDepth < 0)
        return { };

    // 0x07 - first three bits from last byte of image specification field.
    [[maybe_unused]] auto const alphaDepth = targa.header.imageSpec.at(9) & 0x07;

    std::vector<std::byte> imageID(targa.header.IDLength);

    if (std::size(imageID))
        file.read(reinterpret_cast<char *>(std::data(imageID)), sizeof(std::size(imageID)));

    switch (targa.header.imageType) {
        // No image data is present
        case 0x00:
        // Uncompressed monochrome image
        case 0x03:
        // Run-length encoded color-mapped image
        case 0x09:
        // Run-length encoded monochrome image
        case 0x0B:
        // Run-length encoded true-color image
        case 0x0A:
            return { };

        // Uncompressed color-mapped image
        case 0x01:
            LoadUncompressedColorMappedImage(targa, file);
            break;

        // Uncompressed true-color image
        case 0x02:
            LoadUncompressedTrueColorImage(targa, file);
            break;

        default:
            return { };
    }

    file.close();

    if (targa.pixelLayout == ePIXEL_LAYOUT::nUNDEFINED)
        return { };

    RawImage image;

    image.format = GetPixelFormat(targa.pixelLayout);
    image.type = VK_IMAGE_VIEW_TYPE_2D;

    image.width = targa.width;
    image.height = targa.height;

    image.mipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(image.width, image.height))) + 1);

    image.data = std::move(targa.data);

    return image;
}