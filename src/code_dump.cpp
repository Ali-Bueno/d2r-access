#include "code_dump.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace D2RAccess::CodeDump {

namespace {

constexpr const wchar_t* DumpRelativeDir = L"d2r-access\\";
constexpr const wchar_t* DumpBinaryName  = L"D2R.text.bin";
constexpr const wchar_t* DumpInfoName    = L"D2R.text.json";
constexpr const wchar_t* ImageBinaryName = L"D2R.image.bin";
constexpr const wchar_t* ImageInfoName   = L"D2R.image.json";
constexpr const char*    CodeSectionName = ".text";
// The mapped image runs past what the on-disk headers describe. The highest
// data reference measured in .text lands at image+0x3E2C27F, about 62 MB,
// against the 39.5 MB the file's own SizeOfImage declares (docs/decompile.md).
// This bounds a corrupt walk without cutting the real mapping short.
constexpr size_t MaxImageBytes = 128u * 1024 * 1024;
constexpr size_t MaxRegions    = 4096;

struct Region {
	uintptr_t base = 0;
	size_t    size = 0;
	uint32_t  state = 0, protect = 0, type = 0;
	bool      ours = false;     // inside the exe's own allocation
	bool      captured = false; // committed and readable, so its bytes are in the dump
};

bool Readable(const MEMORY_BASIC_INFORMATION& info) {
	return info.State == MEM_COMMIT && (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
}

std::wstring DumpDirectory(const D2RL::PluginContext* context) {
	std::wstring dir = context->pluginDirectory != nullptr ? context->pluginDirectory : L"";
	if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') {
		dir += L'\\';
	}
	return dir + DumpRelativeDir;
}

bool Exists(const std::wstring& path) {
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Copies [address, address+size) page by page, zero-filling unreadable pages.
void CopyReadable(const uint8_t* address, size_t size, std::vector<uint8_t>& out, size_t& unreadablePages) {
	out.assign(size, 0);
	size_t offset = 0;
	while (offset < size) {
		MEMORY_BASIC_INFORMATION info {};
		if (VirtualQuery(address + offset, &info, sizeof(info)) == 0) {
			break;
		}
		const size_t regionEnd = static_cast<size_t>(reinterpret_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize - address);
		const size_t chunk     = (regionEnd < size ? regionEnd : size) - offset;
		const bool   readable  = info.State == MEM_COMMIT && (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0;
		if (readable) {
			std::memcpy(out.data() + offset, address + offset, chunk);
		} else {
			unreadablePages += (chunk + 0xFFF) / 0x1000;
		}
		offset += chunk;
	}
}

// Every region from the image base upwards, in address order, until the
// mapping ends. Regions outside the exe's own allocation are recorded rather
// than assumed away: the code demonstrably points past the end of the
// mapping, and deciding which of those bytes matter is an offline question
// this manifest answers, not one to guess here.
std::vector<Region> WalkImage(const uint8_t* base, size_t sizeOfImage, size_t& capturedEnd) {
	std::vector<Region> regions;
	capturedEnd = 0;
	size_t offset = 0;
	while (offset < MaxImageBytes && regions.size() < MaxRegions) {
		MEMORY_BASIC_INFORMATION info {};
		if (VirtualQuery(base + offset, &info, sizeof(info)) == 0) break;
		// Free space at or past the declared end of the image means the
		// mapping is behind us; anything further belongs to someone else.
		if (info.State == MEM_FREE && offset >= sizeOfImage) break;
		Region region;
		region.base     = reinterpret_cast<uintptr_t>(info.BaseAddress);
		region.size     = info.RegionSize;
		region.state    = info.State;
		region.protect  = info.Protect;
		region.type     = info.Type;
		region.ours     = info.AllocationBase == static_cast<const void*>(base);
		region.captured = Readable(info);
		const uintptr_t end = region.base + region.size - reinterpret_cast<uintptr_t>(base);
		if (region.size == 0 || end <= offset) break; // a walk that stopped advancing
		if (region.captured) capturedEnd = end < MaxImageBytes ? end : MaxImageBytes;
		regions.push_back(region);
		offset = end;
	}
	return regions;
}

}

bool EnsureDumped(const D2RL::PluginContext* context) {
	if (context == nullptr || context->exeBase == 0) {
		return false;
	}
	const std::wstring dir    = DumpDirectory(context);
	const std::wstring binary = dir + DumpBinaryName;
	if (Exists(binary)) {
		return true;
	}

	const auto* base = reinterpret_cast<const uint8_t*>(context->exeBase);
	const auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		context->LogError("code dump: exeBase has no DOS header");
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		context->LogError("code dump: exeBase has no NT header");
		return false;
	}
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	const IMAGE_SECTION_HEADER* code    = nullptr;
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
		if (std::strncmp(reinterpret_cast<const char*>(section[i].Name), CodeSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0) {
			code = &section[i];
			break;
		}
	}
	if (code == nullptr) {
		context->LogError("code dump: no .text section");
		return false;
	}

	std::vector<uint8_t> bytes;
	size_t               unreadable = 0;
	CopyReadable(base + code->VirtualAddress, code->Misc.VirtualSize, bytes, unreadable);

	CreateDirectoryW(dir.c_str(), nullptr);
	{
		std::ofstream out(binary, std::ios::binary);
		if (!out) {
			context->LogError("code dump: cannot write the dump file");
			return false;
		}
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	{
		std::ofstream info(dir + DumpInfoName);
		info << "{ \"imageBase\": " << context->exeBase << ", \"textRva\": " << code->VirtualAddress << ", \"textSize\": " << code->Misc.VirtualSize
			 << ", \"unreadablePages\": " << unreadable << ", \"build\": \"" << (context->buildVersion != nullptr ? context->buildVersion : "") << "\" }\n";
	}
	D2RL::LogInfoF(context, "code dump: wrote %zu bytes of .text (RVA 0x%lX, %zu unreadable pages)", bytes.size(), static_cast<unsigned long>(code->VirtualAddress), unreadable);
	return true;
}

bool EnsureImageDumped(const D2RL::PluginContext* context) {
	if (context == nullptr || context->exeBase == 0) return false;
	const std::wstring dir    = DumpDirectory(context);
	const std::wstring binary = dir + ImageBinaryName;
	if (Exists(binary)) return true;

	const auto* base = reinterpret_cast<const uint8_t*>(context->exeBase);
	const auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		context->LogError("image dump: exeBase has no DOS header");
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		context->LogError("image dump: exeBase has no NT header");
		return false;
	}
	// The LIVE header, not the file's: if the protector stretched the mapping
	// it is this value that says so, and comparing the two is the first thing
	// worth knowing offline.
	const size_t sizeOfImage = nt->OptionalHeader.SizeOfImage;

	size_t capturedEnd = 0;
	const std::vector<Region> regions = WalkImage(base, sizeOfImage, capturedEnd);
	if (capturedEnd == 0) {
		context->LogError("image dump: nothing readable at the image base");
		return false;
	}

	// One flat block based at the image base, so a byte at file offset N is
	// the byte the code addresses at base+N. Gaps stay zero rather than
	// closing up, which is what keeps that true.
	std::vector<uint8_t> bytes(capturedEnd, 0);
	size_t unreadable = 0, captured = 0;
	for (const Region& region : regions) {
		if (!region.captured) {
			unreadable += (region.size + 0xFFF) / 0x1000;
			continue;
		}
		const size_t offset = region.base - reinterpret_cast<uintptr_t>(base);
		if (offset >= capturedEnd) continue;
		const size_t chunk = region.size < capturedEnd - offset ? region.size : capturedEnd - offset;
		std::memcpy(bytes.data() + offset, base + offset, chunk);
		captured += chunk;
	}

	CreateDirectoryW(dir.c_str(), nullptr);
	{
		std::ofstream out(binary, std::ios::binary);
		if (!out) {
			context->LogError("image dump: cannot write the dump file");
			return false;
		}
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	{
		std::ofstream info(dir + ImageInfoName);
		info << "{ \"imageBase\": " << context->exeBase
			 << ", \"sizeOfImage\": " << sizeOfImage
			 << ", \"dumpedBytes\": " << bytes.size()
			 << ", \"capturedBytes\": " << captured
			 << ", \"unreadablePages\": " << unreadable
			 << ", \"build\": \"" << (context->buildVersion != nullptr ? context->buildVersion : "") << "\""
			 << ", \"regions\": [";
		for (size_t i = 0; i < regions.size(); ++i) {
			const Region& region = regions[i];
			info << (i ? ", " : "") << "{ \"rva\": " << (region.base - context->exeBase)
				 << ", \"size\": " << region.size
				 << ", \"state\": " << region.state
				 << ", \"protect\": " << region.protect
				 << ", \"type\": " << region.type
				 << ", \"ours\": " << (region.ours ? 1 : 0)
				 << ", \"captured\": " << (region.captured ? 1 : 0) << " }";
		}
		info << "] }\n";
	}
	D2RL::LogInfoF(context,
		"image dump: wrote %zu bytes from base 0x%llX (SizeOfImage 0x%zX, %zu captured, %zu regions, %zu unreadable pages)",
		bytes.size(), static_cast<unsigned long long>(context->exeBase), sizeOfImage, captured, regions.size(), unreadable);
	return true;
}

}
