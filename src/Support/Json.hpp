#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace e33
{
using Json = nlohmann::json;

// Lê um arquivo UTF-8 inteiro. nullopt se não existe ou não dá para abrir —
// nunca lança: config ausente é um estado normal, não um erro.
std::optional<std::string> read_text_file(const std::filesystem::path& path);

// Grava via arquivo temporário + rename. Um crash no meio da gravação deixa o
// config antigo intacto em vez de um JSON truncado que não carrega mais.
bool write_text_file_atomic(const std::filesystem::path& path, std::string_view contents);

// Parse tolerante: JSON inválido devolve nullopt em vez de lançar, e o chamador
// decide o fallback (que nos dois mods é "comportamento vanilla").
std::optional<Json> parse_json(std::string_view text);
} // namespace e33
