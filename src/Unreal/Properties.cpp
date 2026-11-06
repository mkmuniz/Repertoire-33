#include "Unreal/Properties.hpp"

#include <format>

namespace e33::unreal
{
namespace
{
constexpr std::size_t kGuidLength = 32;
constexpr int kChainLimit = 64;     // superclasses
constexpr int kPropertyLimit = 4096; // campos por struct

template <typename Predicate>
bool all_of(std::string_view text, Predicate predicate)
{
    for (const char c : text)
    {
        if (!predicate(static_cast<unsigned char>(c)))
        {
            return false;
        }
    }
    return !text.empty();
}

bool is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

bool is_hex(unsigned char c)
{
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
} // namespace

bool matches_member_name(std::string_view actual, std::string_view wanted)
{
    if (actual == wanted)
    {
        return true;
    }
    if (!actual.starts_with(wanted) || actual.size() <= wanted.size() + 1
        || actual[wanted.size()] != '_')
    {
        return false;
    }
    // Resto: "<numero>_<guid de 32 hex>".
    const auto rest = actual.substr(wanted.size() + 1);
    const auto underscore = rest.find('_');
    if (underscore == std::string_view::npos)
    {
        return false;
    }
    const auto number = rest.substr(0, underscore);
    const auto guid = rest.substr(underscore + 1);
    return all_of(number, is_digit) && guid.size() == kGuidLength && all_of(guid, is_hex);
}

std::optional<Property> Properties::find(std::uintptr_t ustruct, std::string_view name) const
{
    for (int depth = 0; ustruct != 0 && depth < kChainLimit; ++depth, ustruct = super_of(ustruct))
    {
        auto field = read<std::uintptr_t>(ustruct + struct_layout::kChildProperties).value_or(0);
        for (int n = 0; field != 0 && n < kPropertyLimit; ++n)
        {
            const auto field_name = read<FName>(field + field_layout::kName);
            if (field_name)
            {
                auto text = m_objects.names().to_string(*field_name);
                if (matches_member_name(text, name))
                {
                    Property property;
                    property.address = field;
                    property.name = std::move(text);
                    const auto field_class = read<std::uintptr_t>(field + field_layout::kClass);
                    if (field_class && *field_class != 0)
                    {
                        if (const auto type = read<FName>(*field_class))
                        {
                            property.type = m_objects.names().to_string(*type);
                        }
                    }
                    property.offset = read<std::int32_t>(field + field_layout::kOffset).value_or(-1);
                    property.size =
                        read<std::int32_t>(field + field_layout::kElementSize).value_or(0);
                    return property;
                }
            }
            field = read<std::uintptr_t>(field + field_layout::kNext).value_or(0);
        }
    }
    return std::nullopt;
}

std::optional<Property> Properties::find_path(std::uintptr_t ustruct,
                                              std::initializer_list<std::string_view> path) const
{
    std::optional<Property> result{};
    std::int32_t offset = 0;
    for (const auto name : path)
    {
        if (result)
        {
            // Descer exige que o campo anterior seja um struct.
            if (result->type != "StructProperty")
            {
                return std::nullopt;
            }
            ustruct = struct_of(*result);
        }
        result = find(ustruct, name);
        if (!result || result->offset < 0)
        {
            return std::nullopt;
        }
        offset += result->offset;
    }
    if (result)
    {
        result->offset = offset;
    }
    return result;
}

std::uintptr_t Properties::struct_of(const Property& property)
{
    return read<std::uintptr_t>(property.address + field_layout::kStruct).value_or(0);
}

std::uintptr_t Properties::super_of(std::uintptr_t ustruct)
{
    return ustruct == 0 ? 0 : read<std::uintptr_t>(ustruct + struct_layout::kSuperStruct).value_or(0);
}

bool Properties::is_a(std::uintptr_t klass, std::uintptr_t base)
{
    for (int depth = 0; klass != 0 && depth < kChainLimit; ++depth, klass = super_of(klass))
    {
        if (klass == base)
        {
            return true;
        }
    }
    return false;
}

std::string Properties::validate_layout() const
{
    // SuperStruct: a classe de UClass herda de UStruct.
    const auto class_class = m_objects.find("Class /Script/CoreUObject.Class");
    const auto struct_class = m_objects.find("Class /Script/CoreUObject.Struct");
    if (class_class == 0 || struct_class == 0)
    {
        return "classes Class/Struct do CoreUObject nao encontradas";
    }
    if (super_of(class_class) != struct_class)
    {
        return std::format("UStruct::SuperStruct nao esta em +0x{:X}", struct_layout::kSuperStruct);
    }

    // FProperty: FVector do UE5 é X, Y, Z em double, nos offsets 0, 8, 16.
    const auto vector = m_objects.find("ScriptStruct /Script/CoreUObject.Vector");
    if (vector == 0)
    {
        return "ScriptStruct Vector nao encontrado";
    }
    const auto y = find(vector, "Y");
    if (!y)
    {
        return std::format("FField::Next/Name ou ChildProperties (+0x{:X}) nao conferem",
                           struct_layout::kChildProperties);
    }
    if (y->offset != 8 || y->size != 8)
    {
        return std::format("FProperty::Offset/ElementSize nao conferem (Y em {} com {} bytes)",
                           y->offset, y->size);
    }
    if (y->type != "DoubleProperty")
    {
        return std::format("FField::ClassPrivate nao confere (tipo de Y: \"{}\")", y->type);
    }
    return {};
}
} // namespace e33::unreal
