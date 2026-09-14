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
constexpr const char*    CodeSectionName = ".text";

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

}
