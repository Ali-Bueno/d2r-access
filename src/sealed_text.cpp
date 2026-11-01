#include "sealed_text.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>

namespace D2RAccess::SealedText {
namespace {
constexpr size_t NonceSize = 12;
constexpr size_t TagSize = 16;
// Starts each packed text, followed by its name and a line break; never in a text file.
constexpr char PackMark = '\x1D';

// AES in GCM mode with `key`; both handles are released by the destructor.
class Cipher {
public:
	explicit Cipher(const Key& key) {
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_AES_ALGORITHM, nullptr, 0))) return;
		const auto mode = const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM);
		if (!BCRYPT_SUCCESS(BCryptSetProperty(algorithm_, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(mode),
			static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0))) return;
		BCryptGenerateSymmetricKey(algorithm_, &key_, nullptr, 0, const_cast<PUCHAR>(key.data()),
			static_cast<ULONG>(key.size()), 0);
	}
	~Cipher() {
		if (key_) BCryptDestroyKey(key_);
		if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0);
	}
	Cipher(const Cipher&) = delete;
	Cipher& operator=(const Cipher&) = delete;
	BCRYPT_KEY_HANDLE Handle() const { return key_; }
private:
	BCRYPT_ALG_HANDLE algorithm_ = nullptr;
	BCRYPT_KEY_HANDLE key_ = nullptr;
};

BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ModeInfo(uint8_t* nonce, uint8_t* tag) {
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = nonce;
	info.cbNonce = static_cast<ULONG>(NonceSize);
	info.pbTag = tag;
	info.cbTag = static_cast<ULONG>(TagSize);
	return info;
}

bool Random(uint8_t* bytes, size_t size) {
	return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}
}

std::optional<Key> NewKey() {
	Key key{};
	return Random(key.data(), key.size()) ? std::optional(key) : std::nullopt;
}

std::optional<std::vector<uint8_t>> Seal(std::string_view plain, const Key& key) {
	const Cipher cipher(key);
	if (!cipher.Handle()) return std::nullopt;
	std::vector<uint8_t> sealed(NonceSize + plain.size() + TagSize);
	uint8_t* nonce = sealed.data();
	uint8_t* body = nonce + NonceSize;
	uint8_t* tag = body + plain.size();
	if (!Random(nonce, NonceSize)) return std::nullopt;
	auto info = ModeInfo(nonce, tag);
	ULONG written = 0;
	const auto status = BCryptEncrypt(cipher.Handle(), reinterpret_cast<PUCHAR>(const_cast<char*>(plain.data())),
		static_cast<ULONG>(plain.size()), &info, nullptr, 0, body, static_cast<ULONG>(plain.size()), &written, 0);
	if (!BCRYPT_SUCCESS(status) || written != plain.size()) return std::nullopt;
	return sealed;
}

std::optional<std::string> Open(std::span<const uint8_t> sealed, const Key& key) {
	if (sealed.size() < NonceSize + TagSize) return std::nullopt;
	const Cipher cipher(key);
	if (!cipher.Handle()) return std::nullopt;
	const size_t size = sealed.size() - NonceSize - TagSize;
	std::array<uint8_t, NonceSize> nonce{};
	std::array<uint8_t, TagSize> tag{};
	std::copy_n(sealed.begin(), NonceSize, nonce.begin());
	std::copy_n(sealed.end() - TagSize, TagSize, tag.begin());
	std::vector<uint8_t> body(sealed.begin() + NonceSize, sealed.end() - TagSize);
	std::string plain(size, '\0');
	auto info = ModeInfo(nonce.data(), tag.data());
	ULONG written = 0;
	const auto status = BCryptDecrypt(cipher.Handle(), body.data(), static_cast<ULONG>(size), &info, nullptr, 0,
		reinterpret_cast<PUCHAR>(plain.data()), static_cast<ULONG>(size), &written, 0);
	if (!BCRYPT_SUCCESS(status) || written != size) return std::nullopt;
	return plain;
}

std::string Pack(const Texts& texts) {
	std::string packed;
	for (const auto& [name, text] : texts) packed.append(1, PackMark).append(name).append(1, '\n').append(text);
	return packed;
}

Texts Unpack(std::string_view packed) {
	Texts texts;
	while (!packed.empty() && packed.front() == PackMark) {
		const auto nameEnd = packed.find('\n');
		if (nameEnd == std::string_view::npos) break;
		const auto next = packed.find(PackMark, nameEnd);
		texts[std::string(packed.substr(1, nameEnd - 1))] = std::string(packed.substr(nameEnd + 1, next - nameEnd - 1));
		packed = next == std::string_view::npos ? std::string_view{} : packed.substr(next);
	}
	return texts;
}

}
