#include "Unreal/Memory.hpp"

#if defined(_WIN32)

#include <cstring>

#include <Windows.h>

namespace e33::unreal
{
std::optional<Image> main_image()
{
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0)
    {
        return std::nullopt;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return std::nullopt;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
    {
        return std::nullopt;
    }

    Image image;
    image.base = base;
    image.size = nt->OptionalHeader.SizeOfImage;

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
    {
        char name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
        std::memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
        const std::string_view view{name};

        const Section range{base + section->VirtualAddress, section->Misc.VirtualSize};
        if (view == ".text")
        {
            image.text = range;
        }
        else if (view == ".rdata")
        {
            image.rdata = range;
        }
        else if (view == ".data")
        {
            image.data = range;
        }
    }

    if (image.text.size == 0 || image.data.size == 0)
    {
        return std::nullopt;
    }
    return image;
}

bool is_readable(const void* address, std::size_t size)
{
    if (address == nullptr || size == 0)
    {
        return false;
    }

    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    const auto end = cursor + size;
    if (end < cursor)
    {
        return false;
    }

    // Um intervalo pode atravessar duas regiões com proteções diferentes.
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0)
        {
            return false;
        }
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
                                    | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                                    | PAGE_EXECUTE_WRITECOPY;
        if (info.State != MEM_COMMIT || (info.Protect & kReadable) == 0
            || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
        {
            return false;
        }
        cursor = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

#if defined(_MSC_VER)
// Sem objetos C++ no corpo: é a condição para __try conviver com /EHsc.
bool copy_memory(void* out, std::uintptr_t address, std::size_t size)
{
    __try
    {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool write_memory(std::uintptr_t address, const void* data, std::size_t size)
{
    __try
    {
        std::memcpy(reinterpret_cast<void*>(address), data, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
#else
bool copy_memory(void* out, std::uintptr_t address, std::size_t size)
{
    if (!is_readable(reinterpret_cast<const void*>(address), size))
    {
        return false;
    }
    std::memcpy(out, reinterpret_cast<const void*>(address), size);
    return true;
}

// Só a checagem de sintaxe do mingw passa por aqui; a DLL distribuída é MSVC.
bool write_memory(std::uintptr_t address, const void* data, std::size_t size)
{
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == 0
        || info.State != MEM_COMMIT
        || (info.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) == 0)
    {
        return false;
    }
    std::memcpy(reinterpret_cast<void*>(address), data, size);
    return true;
}
#endif
} // namespace e33::unreal

#else // !_WIN32

#include <cstring>

namespace e33::unreal
{
std::optional<Image> main_image() { return std::nullopt; }

// Fora do Windows só existe o processo de teste, e os testes só apontam para
// buffers que eles mesmos alocaram.
bool is_readable(const void* address, std::size_t size)
{
    return address != nullptr && size != 0;
}

bool copy_memory(void* out, std::uintptr_t address, std::size_t size)
{
    if (!is_readable(reinterpret_cast<const void*>(address), size))
    {
        return false;
    }
    std::memcpy(out, reinterpret_cast<const void*>(address), size);
    return true;
}

bool write_memory(std::uintptr_t address, const void* data, std::size_t size)
{
    if (address == 0 || size == 0)
    {
        return false;
    }
    std::memcpy(reinterpret_cast<void*>(address), data, size);
    return true;
}
} // namespace e33::unreal

#endif
