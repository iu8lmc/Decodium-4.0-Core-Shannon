/*
 * Developer-only fixture generator for the pinned rimio/libsstv encoder.
 *
 * Build this file outside the Decodium runtime against libsstv commit
 * 193157a993ac34bfa074074004c9ddadcfe6fd15.  The generated PCM is not
 * committed; its compact hashes and timing landmarks are recorded by the
 * native tests.  This helper is intentionally C11 so it exercises the public
 * libsstv API instead of sharing implementation code with Decodium.
 */

#include <libsstv.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int generate(sstv_mode_t mode, const char *id, const char *path)
{
    uint32_t width = 0U;
    uint32_t height = 0U;
    sstv_image_format_t format = SSTV_FORMAT_RGB;
    if (sstv_get_mode_image_props(mode, &width, &height, &format) != SSTV_OK
        || format != SSTV_FORMAT_YCBCR) {
        return 1;
    }

    const size_t pixels = (size_t)width * (size_t)height;
    if (width == 0U || height == 0U || pixels > SIZE_MAX / 3U) {
        return 2;
    }
    uint8_t *image_bytes = malloc(pixels * 3U);
    if (image_bytes == NULL) {
        return 3;
    }
    for (size_t index = 0U; index < pixels; ++index) {
        /* Public libsstv YCbCr order is Y, Cb/B-Y, Cr/R-Y. */
        image_bytes[index * 3U] = 76U;
        image_bytes[index * 3U + 1U] = 85U;
        image_bytes[index * 3U + 2U] = 255U;
    }

    sstv_image_t image;
    if (sstv_pack_image(&image, width, height, format, image_bytes)
        != SSTV_OK) {
        free(image_bytes);
        return 4;
    }

    void *encoder = NULL;
    if (sstv_create_encoder(&encoder, image, mode, 12000U) != SSTV_OK) {
        free(image_bytes);
        return 5;
    }
    FILE *output = fopen(path, "wb");
    if (output == NULL) {
        sstv_delete_encoder(encoder);
        free(image_bytes);
        return 6;
    }

    int16_t samples[8192];
    sstv_signal_t signal;
    if (sstv_pack_signal(&signal, SSTV_SAMPLE_INT16, 8192U, samples)
        != SSTV_OK) {
        fclose(output);
        sstv_delete_encoder(encoder);
        free(image_bytes);
        return 7;
    }

    uint64_t total = 0U;
    for (;;) {
        const sstv_error_t result = sstv_encode(encoder, &signal);
        if (result != SSTV_ENCODE_SUCCESSFUL && result != SSTV_ENCODE_END) {
            fclose(output);
            sstv_delete_encoder(encoder);
            free(image_bytes);
            return 8;
        }
        if (fwrite(samples, sizeof(samples[0]), signal.count, output)
            != signal.count) {
            fclose(output);
            sstv_delete_encoder(encoder);
            free(image_bytes);
            return 9;
        }
        total += signal.count;
        if (result == SSTV_ENCODE_END) {
            break;
        }
    }

    if (fclose(output) != 0) {
        sstv_delete_encoder(encoder);
        free(image_bytes);
        return 10;
    }
    printf("id=%s mode=%u width=%u height=%u format=%u total=%llu path=%s\n",
           id,
           (unsigned)mode,
           (unsigned)width,
           (unsigned)height,
           (unsigned)format,
           (unsigned long long)total,
           path);
    sstv_delete_encoder(encoder);
    free(image_bytes);
    return 0;
}

int main(void)
{
    if (sstv_init(malloc, free) != SSTV_OK) {
        return 20;
    }
    const struct {
        sstv_mode_t mode;
        const char *id;
        const char *path;
    } modes[] = {
        {SSTV_MODE_PD50, "pd-50", "/tmp/libsstv-pd-50.pcm"},
        {SSTV_MODE_PD90, "pd-90", "/tmp/libsstv-pd-90.pcm"},
        {SSTV_MODE_PD120, "pd-120", "/tmp/libsstv-pd-120.pcm"},
        {SSTV_MODE_PD160, "pd-160", "/tmp/libsstv-pd-160.pcm"},
        {SSTV_MODE_PD180, "pd-180", "/tmp/libsstv-pd-180.pcm"},
        {SSTV_MODE_PD240, "pd-240", "/tmp/libsstv-pd-240.pcm"},
        {SSTV_MODE_PD290, "pd-290", "/tmp/libsstv-pd-290.pcm"}
    };
    for (size_t index = 0U;
         index < sizeof(modes) / sizeof(modes[0]);
         ++index) {
        const int result = generate(
            modes[index].mode, modes[index].id, modes[index].path);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}
