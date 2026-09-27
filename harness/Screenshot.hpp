#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// Captura do framebuffer para BMP.
//
// Existe por dois motivos práticos: screenshot de README é requisito de
// publicação (mod sem imagem ninguém instala), e depender do gerenciador de
// janelas para fotografar o overlay é frágil — qualquer clique em outro
// programa rouba o foco e a captura sai errada.
//
// BMP sem compressão para não arrastar dependência de PNG para dentro do
// harness. Converta depois com `sips -s format png`.
namespace e33::harness
{
inline bool write_bmp(const std::string& path, int width, int height,
                      const std::vector<std::uint8_t>& rgba)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    const int row_bytes = width * 3;
    const int padding = (4 - (row_bytes % 4)) % 4;
    const int padded_row = row_bytes + padding;
    const std::uint32_t pixel_bytes = static_cast<std::uint32_t>(padded_row * height);
    const std::uint32_t file_size = 54u + pixel_bytes;

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
    {
        return false;
    }

    std::uint8_t header[54]{};
    header[0] = 'B';
    header[1] = 'M';
    const auto put32 = [&header](int offset, std::uint32_t value) {
        header[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
        header[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        header[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
        header[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    };
    put32(2, file_size);
    put32(10, 54);                                    // offset dos pixels
    put32(14, 40);                                    // tamanho do DIB
    put32(18, static_cast<std::uint32_t>(width));
    put32(22, static_cast<std::uint32_t>(height));
    header[26] = 1;                                   // planos
    header[28] = 24;                                  // bits por pixel
    put32(34, pixel_bytes);
    std::fwrite(header, 1, sizeof(header), file);

    // glReadPixels devolve de baixo para cima, que é a mesma ordem do BMP.
    std::vector<std::uint8_t> row(static_cast<std::size_t>(padded_row), 0);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto src = static_cast<std::size_t>((y * width + x) * 4);
            const auto dst = static_cast<std::size_t>(x * 3);
            row[dst + 0] = rgba[src + 2]; // BMP guarda BGR
            row[dst + 1] = rgba[src + 1];
            row[dst + 2] = rgba[src + 0];
        }
        std::fwrite(row.data(), 1, static_cast<std::size_t>(padded_row), file);
    }

    std::fclose(file);
    return true;
}
} // namespace e33::harness
