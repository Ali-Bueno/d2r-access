#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Text sealed with AES-256-GCM (Windows CNG): nonce, ciphertext, tag. An altered byte fails
// to open. docs/descriptions.md
namespace D2RAccess::SealedText {

constexpr size_t KeySize = 32;
using Key = std::array<uint8_t, KeySize>;

// A fresh random key.
std::optional<Key> NewKey();
// `plain` sealed under `key` with a random nonce; nullopt when the system refuses.
std::optional<std::vector<uint8_t>> Seal(std::string_view plain, const Key& key);
// The text sealed under `key`; nullopt when it was altered or is not sealed under that key.
std::optional<std::string> Open(std::span<const uint8_t> sealed, const Key& key);

// Texts by name ("enUS"), packed into one text and back.
using Texts = std::unordered_map<std::string, std::string>;
std::string Pack(const Texts& texts);
Texts Unpack(std::string_view packed);

}
