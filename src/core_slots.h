#pragma once

#include "native_memory.h"

#include <windows.h>

#include <cstdint>
#include <cwchar>
#include <iterator>
#include <span>
#include <string>
#include <vector>

// D2RLoader resolves the game pointers it needs into a table of 0x18-byte
// descriptors in D2RCore.dll's writable data: +0x00 holds the address, written
// at startup, and +0x08 the game RVA it was resolved from. Reading a descriptor
// gives the plugin the very function the loader itself calls.
//
// Which entry a pointer lands in moves with every D2RCore build — entries are
// inserted, so even the distance between two of them changes — which is why
// nothing here stores an offset into the DLL. A descriptor is found by the game
// RVA it carries instead, a fact of the game build rather than the loader's.
// Checked against both builds we have: on 1.2.1-beta this reproduces every
// offset that used to be hardcoded (0x5A8CF8, 0x5A8D10, 0x5AA340, 0x5AB610,
// 0x5AB880, 0x5ADA48), and on 1.3.0-beta it yields 0x67DDC0, 0x67DDF0,
// 0x67F4D0, 0x680AA0, 0x680D28 and 0x683810. Each of those RVAs occurs exactly
// once in the whole DLL in both builds.
namespace D2RAccess::CoreSlots {

constexpr const wchar_t* ModuleName          = L"D2RCore.dll";
constexpr size_t         DescriptorRvaOffset = 0x8;

struct Anchor {
	uint64_t  gameRva = 0;
	uintptr_t slot    = 0; // offset from D2RCore's base, 0 when unresolved
	unsigned  hits    = 0;
};

// Fills every anchor in a single pass over D2RCore's writable data. The loader
// writes the resolved address into the descriptor, so only a writable section
// can hold one; skipping the rest also keeps a constant that happens to equal
// one of the RVAs, in code or in a resource, from being read as a descriptor.
// An RVA that is absent, or that turns up more than once, leaves its slot at
// zero rather than guessing — callers report `hits` when that happens.
inline void Resolve(uintptr_t coreBase, std::span<Anchor> anchors) {
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(coreBase);
	const auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<const uint8_t*>(coreBase) + dos->e_lfanew);
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
		constexpr DWORD writableData = IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ;
		if ((section->Characteristics & writableData) != writableData || (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) continue;
		const uintptr_t begin = coreBase + section->VirtualAddress;
		const size_t    size  = section->Misc.VirtualSize;
		if (size < sizeof(uint64_t) || !NativeMemory::Readable(reinterpret_cast<const void*>(begin), size)) continue;
		for (uintptr_t at = begin + DescriptorRvaOffset; at + sizeof(uint64_t) <= begin + size; at += sizeof(uint64_t)) {
			const uint64_t value = *reinterpret_cast<const uint64_t*>(at);
			if (value == 0) continue;
			for (Anchor& anchor : anchors) {
				if (value != anchor.gameRva) continue;
				++anchor.hits;
				anchor.slot = at - DescriptorRvaOffset - coreBase; // an offset: callers add the base back
			}
		}
	}
	for (Anchor& anchor : anchors) {
		if (anchor.hits != 1) anchor.slot = 0;
	}
}

// True once every anchor resolved to exactly one descriptor.
inline bool Resolved(std::span<const Anchor> anchors) {
	for (const Anchor& anchor : anchors) {
		if (anchor.slot == 0) return false;
	}
	return true;
}

// The plugin context's buildVersion is the *game's* build ("3.3.93787"); the
// API reports nothing about the loader, yet the loader build is what decides
// whether these descriptors and the layout fingerprints still hold. Its own DLL
// says so, in the version resource that carries the whole "1.3.0-beta" string
// the numeric fields cannot express. Read once, on first use.
inline const char* LoaderVersion() {
	static const std::string cached = [] {
		std::string   result = "unknown";
		const HMODULE core   = GetModuleHandleW(ModuleName);
		wchar_t       path[MAX_PATH] {};
		if (core == nullptr || GetModuleFileNameW(core, path, MAX_PATH) == 0) return result;
		DWORD       ignored = 0;
		const DWORD size    = GetFileVersionInfoSizeW(path, &ignored);
		if (size == 0) return result;
		std::vector<uint8_t> block(size);
		if (GetFileVersionInfoW(path, 0, size, block.data()) == 0) return result;
		struct LanguageAndCodePage {
			WORD language, codePage;
		};
		LanguageAndCodePage* translation = nullptr;
		UINT                 bytes       = 0;
		if (VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translation), &bytes) == 0 || bytes < sizeof(LanguageAndCodePage)) return result;
		wchar_t query[64] {};
		std::swprintf(query, std::size(query), L"\\StringFileInfo\\%04x%04x\\FileVersion", translation->language, translation->codePage);
		wchar_t* value = nullptr;
		if (VerQueryValueW(block.data(), query, reinterpret_cast<void**>(&value), &bytes) == 0 || value == nullptr) return result;
		const int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
		if (length <= 1) return result;
		result.resize(static_cast<size_t>(length) - 1);
		WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), length, nullptr, nullptr);
		return result;
	}();
	return cached.c_str();
}

}
