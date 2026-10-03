// SPDX-License-Identifier: GPL-3.0-or-later

#include "HamDrmJpeg2000Codec.h"

#include <openjpeg.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace decodium::sstv::hamdrm {
namespace {

struct CodecDeleter final
{
    void operator()(opj_codec_t* codec) const noexcept
    {
        if (codec != nullptr) {
            opj_destroy_codec(codec);
        }
    }
};

struct StreamDeleter final
{
    void operator()(opj_stream_t* stream) const noexcept
    {
        if (stream != nullptr) {
            opj_stream_destroy(stream);
        }
    }
};

struct ImageDeleter final
{
    void operator()(opj_image_t* image) const noexcept
    {
        if (image != nullptr) {
            opj_image_destroy(image);
        }
    }
};

using CodecPtr = std::unique_ptr<opj_codec_t, CodecDeleter>;
using StreamPtr = std::unique_ptr<opj_stream_t, StreamDeleter>;
using ImagePtr = std::unique_ptr<opj_image_t, ImageDeleter>;

struct ErrorContext final
{
    std::string message;
};

void errorCallback(const char* message, void* userData)
{
    if (message == nullptr || userData == nullptr) {
        return;
    }
    auto& context = *static_cast<ErrorContext*>(userData);
    constexpr std::size_t maximumMessageBytes = 2U * 1024U;
    if (context.message.size() < maximumMessageBytes) {
        const std::size_t remaining = maximumMessageBytes
            - context.message.size();
        context.message.append(message,
                               std::min(remaining, std::strlen(message)));
    }
}

HamDrmStatus codecFailure(const ErrorContext& context,
                          const char* fallback)
{
    std::string detail = context.message.empty() ? fallback : context.message;
    while (!detail.empty()
           && (detail.back() == '\n' || detail.back() == '\r')) {
        detail.pop_back();
    }
    return HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                 std::move(detail));
}

struct InputMemory final
{
    const std::uint8_t* data {nullptr};
    std::size_t size {0U};
    std::size_t position {0U};
};

OPJ_SIZE_T readInput(void* destination, OPJ_SIZE_T requested, void* userData)
{
    auto* input = static_cast<InputMemory*>(userData);
    if (input == nullptr || destination == nullptr || input->position >= input->size) {
        return static_cast<OPJ_SIZE_T>(-1);
    }
    const std::size_t count = std::min<std::size_t>(
        static_cast<std::size_t>(requested), input->size - input->position);
    std::memcpy(destination, input->data + input->position, count);
    input->position += count;
    return static_cast<OPJ_SIZE_T>(count);
}

OPJ_OFF_T skipInput(OPJ_OFF_T amount, void* userData)
{
    auto* input = static_cast<InputMemory*>(userData);
    if (input == nullptr || amount < 0
        || static_cast<std::uint64_t>(amount) > input->size - input->position) {
        return static_cast<OPJ_OFF_T>(-1);
    }
    input->position += static_cast<std::size_t>(amount);
    return amount;
}

OPJ_BOOL seekInput(OPJ_OFF_T position, void* userData)
{
    auto* input = static_cast<InputMemory*>(userData);
    if (input == nullptr || position < 0
        || static_cast<std::uint64_t>(position) > input->size) {
        return OPJ_FALSE;
    }
    input->position = static_cast<std::size_t>(position);
    return OPJ_TRUE;
}

struct OutputMemory final
{
    std::vector<std::uint8_t> data;
    std::size_t position {0U};
    std::size_t maximumBytes {0U};
};

bool moveOutput(OutputMemory& output, std::size_t position)
{
    if (position > output.maximumBytes) {
        return false;
    }
    if (position > output.data.size()) {
        output.data.resize(position, 0U);
    }
    output.position = position;
    return true;
}

OPJ_SIZE_T writeOutput(void* source, OPJ_SIZE_T requested, void* userData)
{
    auto* output = static_cast<OutputMemory*>(userData);
    if (output == nullptr || source == nullptr
        || static_cast<std::uint64_t>(requested)
            > output->maximumBytes - output->position) {
        return static_cast<OPJ_SIZE_T>(-1);
    }
    const std::size_t count = static_cast<std::size_t>(requested);
    if (!moveOutput(*output, output->position + count)) {
        return static_cast<OPJ_SIZE_T>(-1);
    }
    std::memcpy(output->data.data() + output->position - count, source, count);
    return requested;
}

OPJ_OFF_T skipOutput(OPJ_OFF_T amount, void* userData)
{
    auto* output = static_cast<OutputMemory*>(userData);
    if (output == nullptr || amount < 0
        || static_cast<std::uint64_t>(amount)
            > output->maximumBytes - output->position
        || !moveOutput(*output,
                       output->position + static_cast<std::size_t>(amount))) {
        return static_cast<OPJ_OFF_T>(-1);
    }
    return amount;
}

OPJ_BOOL seekOutput(OPJ_OFF_T position, void* userData)
{
    auto* output = static_cast<OutputMemory*>(userData);
    if (output == nullptr || position < 0
        || static_cast<std::uint64_t>(position) > output->maximumBytes
        || !moveOutput(*output, static_cast<std::size_t>(position))) {
        return OPJ_FALSE;
    }
    return OPJ_TRUE;
}

StreamPtr inputStream(InputMemory& input)
{
    StreamPtr stream(opj_stream_create(64U * 1024U, OPJ_STREAM_READ));
    if (!stream) {
        return {};
    }
    opj_stream_set_user_data(stream.get(), &input, nullptr);
    opj_stream_set_user_data_length(stream.get(), input.size);
    opj_stream_set_read_function(stream.get(), readInput);
    opj_stream_set_skip_function(stream.get(), skipInput);
    opj_stream_set_seek_function(stream.get(), seekInput);
    return stream;
}

StreamPtr outputStream(OutputMemory& output)
{
    StreamPtr stream(opj_stream_create(64U * 1024U, OPJ_STREAM_WRITE));
    if (!stream) {
        return {};
    }
    opj_stream_set_user_data(stream.get(), &output, nullptr);
    opj_stream_set_user_data_length(stream.get(), output.maximumBytes);
    opj_stream_set_write_function(stream.get(), writeOutput);
    opj_stream_set_skip_function(stream.get(), skipOutput);
    opj_stream_set_seek_function(stream.get(), seekOutput);
    return stream;
}

HamDrmStatus validateDimensions(std::uint32_t width,
                                std::uint32_t height,
                                const HamDrmLimits& limits)
{
    if (width == 0U || height == 0U || width > limits.maximumImageDimension
        || height > limits.maximumImageDimension
        || static_cast<std::uint64_t>(width) * height
            > limits.maximumImagePixels) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "JPEG2000 dimensions exceed policy");
    }
    return HamDrmStatus::success();
}

HamDrmStatus validateComponent(const opj_image_comp_t& component,
                               const HamDrmLimits& limits,
                               bool requireDecodedData)
{
    if ((requireDecodedData && component.data == nullptr)
        || component.w == 0U || component.h == 0U
        || component.dx == 0U || component.dy == 0U
        || component.prec == 0U || component.prec > 31U
        || component.w > limits.maximumImageDimension
        || component.h > limits.maximumImageDimension
        || static_cast<std::uint64_t>(component.w) * component.h
            > limits.maximumImagePixels) {
        return HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                     "JPEG2000 component exceeds policy");
    }
    return HamDrmStatus::success();
}

std::uint8_t componentToByte(const opj_image_comp_t& component,
                             std::int32_t value) noexcept
{
    const std::int64_t minimum = component.sgnd != 0U
        ? -(std::int64_t {1} << (component.prec - 1U)) : 0;
    const std::int64_t maximum = component.sgnd != 0U
        ? (std::int64_t {1} << (component.prec - 1U)) - 1
        : (std::int64_t {1} << component.prec) - 1;
    const std::int64_t clamped = std::clamp<std::int64_t>(value, minimum,
                                                          maximum);
    return static_cast<std::uint8_t>(
        ((clamped - minimum) * 255 + (maximum - minimum) / 2)
        / (maximum - minimum));
}

std::uint8_t sampleComponent(const opj_image_comp_t& component,
                             std::uint32_t imageX,
                             std::uint32_t imageY,
                             std::uint32_t imageOriginX,
                             std::uint32_t imageOriginY) noexcept
{
    const std::uint64_t referenceX = static_cast<std::uint64_t>(imageOriginX)
        + imageX;
    const std::uint64_t referenceY = static_cast<std::uint64_t>(imageOriginY)
        + imageY;
    std::uint64_t componentX = referenceX > component.x0
        ? (referenceX - component.x0) / component.dx : 0U;
    std::uint64_t componentY = referenceY > component.y0
        ? (referenceY - component.y0) / component.dy : 0U;
    componentX = std::min<std::uint64_t>(componentX, component.w - 1U);
    componentY = std::min<std::uint64_t>(componentY, component.h - 1U);
    return componentToByte(component,
                           component.data[componentY * component.w + componentX]);
}

std::uint8_t byteClamp(double value) noexcept
{
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value), 0L, 255L));
}

int maximumResolutionLevels(std::uint32_t width, std::uint32_t height)
{
    std::uint32_t minimum = std::min(width, height);
    int levels = 1;
    while (minimum >= 2U && levels < 32) {
        minimum >>= 1U;
        ++levels;
    }
    return levels;
}

} // namespace

HamDrmValueResult<HamDrmRgbaImage> decodeHamDrmJpeg2000(
    const std::uint8_t* data,
    std::size_t size,
    const HamDrmLimits& limits)
{
    if (data == nullptr || size == 0U || size > limits.maximumObjectBytes) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::LimitExceeded,
                                      "JPEG2000 input exceeds policy")};
    }
    InputMemory input {data, size, 0U};
    auto stream = inputStream(input);
    CodecPtr codec(opj_create_decompress(OPJ_CODEC_JP2));
    if (!stream || !codec) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "cannot allocate OpenJPEG decoder")};
    }
    ErrorContext errors;
    opj_set_error_handler(codec.get(), errorCallback, &errors);
    opj_set_warning_handler(codec.get(), errorCallback, &errors);
    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    if (!opj_setup_decoder(codec.get(), &parameters)) {
        return {std::nullopt, codecFailure(errors,
                                           "OpenJPEG decoder setup failed")};
    }

    opj_image_t* rawImage = nullptr;
    if (!opj_read_header(stream.get(), codec.get(), &rawImage)
        || rawImage == nullptr) {
        return {std::nullopt, codecFailure(errors,
                                           "OpenJPEG header decode failed")};
    }
    ImagePtr image(rawImage);
    if (image->x1 <= image->x0 || image->y1 <= image->y0) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::Malformed,
                                      "JPEG2000 image grid is invalid")};
    }
    const std::uint32_t width = image->x1 - image->x0;
    const std::uint32_t height = image->y1 - image->y0;
    if (const auto status = validateDimensions(width, height, limits);
        !status.ok()) {
        return {std::nullopt, status};
    }
    if (image->numcomps != 1U && image->numcomps != 3U
        && image->numcomps != 4U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedContent,
                                      "JPEG2000 component count is unsupported")};
    }
    if (image->color_space == OPJ_CLRSPC_CMYK
        || image->color_space == OPJ_CLRSPC_UNKNOWN) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::UnsupportedContent,
                                      "JPEG2000 colour space is unsupported")};
    }
    // Header parsing does not allocate the full decoded raster. Validate its
    // advertised grid before allowing OpenJPEG to allocate component planes.
    for (OPJ_UINT32 component = 0U; component < image->numcomps; ++component) {
        if (const auto status = validateComponent(image->comps[component],
                                                   limits, false);
            !status.ok()) {
            return {std::nullopt, status};
        }
    }
    if (!opj_decode(codec.get(), stream.get(), image.get())
        || !opj_end_decompress(codec.get(), stream.get())) {
        return {std::nullopt, codecFailure(errors,
                                           "OpenJPEG image decode failed")};
    }
    for (OPJ_UINT32 component = 0U; component < image->numcomps; ++component) {
        if (const auto status = validateComponent(image->comps[component],
                                                   limits, true);
            !status.ok()) {
            return {std::nullopt, status};
        }
    }

    HamDrmRgbaImage output;
    output.width = width;
    output.height = height;
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    output.rgba.resize(pixels * 4U);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::size_t destination =
                (static_cast<std::size_t>(y) * width + x) * 4U;
            if (image->numcomps == 1U) {
                const std::uint8_t gray = sampleComponent(
                    image->comps[0], x, y, image->x0, image->y0);
                output.rgba[destination] = gray;
                output.rgba[destination + 1U] = gray;
                output.rgba[destination + 2U] = gray;
                output.rgba[destination + 3U] = 255U;
                continue;
            }
            const std::uint8_t first = sampleComponent(
                image->comps[0], x, y, image->x0, image->y0);
            const std::uint8_t second = sampleComponent(
                image->comps[1], x, y, image->x0, image->y0);
            const std::uint8_t third = sampleComponent(
                image->comps[2], x, y, image->x0, image->y0);
            if (image->color_space == OPJ_CLRSPC_SYCC
                || image->color_space == OPJ_CLRSPC_EYCC) {
                const double cb = static_cast<double>(second) - 128.0;
                const double cr = static_cast<double>(third) - 128.0;
                output.rgba[destination] = byteClamp(first + 1.402 * cr);
                output.rgba[destination + 1U] = byteClamp(
                    first - 0.344136 * cb - 0.714136 * cr);
                output.rgba[destination + 2U] = byteClamp(first + 1.772 * cb);
            } else {
                output.rgba[destination] = first;
                output.rgba[destination + 1U] = second;
                output.rgba[destination + 2U] = third;
            }
            output.rgba[destination + 3U] = image->numcomps == 4U
                ? sampleComponent(image->comps[3], x, y,
                                  image->x0, image->y0)
                : 255U;
        }
    }
    return {std::move(output), HamDrmStatus::success()};
}

HamDrmValueResult<std::vector<std::uint8_t>> encodeHamDrmJpeg2000Lossless(
    const HamDrmRgbaImage& input,
    const HamDrmLimits& limits)
{
    if (const auto status = validateDimensions(input.width, input.height, limits);
        !status.ok()) {
        return {std::nullopt, status};
    }
    const std::size_t pixels = static_cast<std::size_t>(input.width)
        * input.height;
    if (pixels > std::numeric_limits<std::size_t>::max() / 4U
        || input.rgba.size() != pixels * 4U) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::InvalidArgument,
                                      "RGBA image storage is inconsistent")};
    }

    std::array<opj_image_cmptparm_t, 3> components {};
    for (auto& component : components) {
        component.dx = 1U;
        component.dy = 1U;
        component.w = input.width;
        component.h = input.height;
        component.prec = 8U;
        component.sgnd = 0U;
    }
    ImagePtr image(opj_image_create(static_cast<OPJ_UINT32>(components.size()),
                                    components.data(), OPJ_CLRSPC_SRGB));
    if (!image) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "cannot allocate OpenJPEG image")};
    }
    image->x0 = 0U;
    image->y0 = 0U;
    image->x1 = input.width;
    image->y1 = input.height;
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        image->comps[0].data[pixel] = input.rgba[pixel * 4U];
        image->comps[1].data[pixel] = input.rgba[pixel * 4U + 1U];
        image->comps[2].data[pixel] = input.rgba[pixel * 4U + 2U];
    }

    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);
    parameters.tcp_numlayers = 1;
    parameters.cp_disto_alloc = 1;
    parameters.tcp_rates[0] = 0.0F;
    parameters.irreversible = 0;
    parameters.tcp_mct = 1;
    parameters.numresolution = std::min(
        parameters.numresolution,
        maximumResolutionLevels(input.width, input.height));
    if (limits.maximumObjectBytes <= static_cast<std::size_t>(
            std::numeric_limits<int>::max())) {
        parameters.max_cs_size = static_cast<int>(limits.maximumObjectBytes);
    }

    CodecPtr codec(opj_create_compress(OPJ_CODEC_JP2));
    if (!codec) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "cannot allocate OpenJPEG encoder")};
    }
    ErrorContext errors;
    opj_set_error_handler(codec.get(), errorCallback, &errors);
    opj_set_warning_handler(codec.get(), errorCallback, &errors);
    if (!opj_setup_encoder(codec.get(), &parameters, image.get())) {
        return {std::nullopt, codecFailure(errors,
                                           "OpenJPEG encoder setup failed")};
    }

    OutputMemory output;
    output.maximumBytes = limits.maximumObjectBytes;
    auto stream = outputStream(output);
    if (!stream) {
        return {std::nullopt,
                HamDrmStatus::failure(HamDrmErrorCode::IoFailure,
                                      "cannot allocate OpenJPEG output stream")};
    }
    if (!opj_start_compress(codec.get(), image.get(), stream.get())
        || !opj_encode(codec.get(), stream.get())
        || !opj_end_compress(codec.get(), stream.get())
        || output.data.empty()) {
        return {std::nullopt, codecFailure(errors,
                                           "OpenJPEG image encode failed")};
    }
    return {std::move(output.data), HamDrmStatus::success()};
}

} // namespace decodium::sstv::hamdrm
