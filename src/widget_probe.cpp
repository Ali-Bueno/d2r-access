#include "widget_probe.h"

#include "async_log.h"
#include "native_memory.h"
#include "native_ui.h"
#include "native_widgets.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace D2RAccess::WidgetProbe {

namespace {

// The head of the object is where the reader's own known fields live (+0x50
// availability, +0x51 visibility, +0x88 text box string), so a state flag is
// very likely nearby. Sixteen rows keeps one dump readable in a log.
constexpr size_t DumpBytes = 0x100;
constexpr size_t RowBytes  = 16;

// Fields this reader already relies on, named in the dump so a changed byte
// can be told apart from one that is merely known.
struct KnownOffset {
	size_t      offset;
	const char* what;
};
constexpr KnownOffset KnownOffsets[] = {
	{ 0x08, "name string" },
	{ 0x50, "availability" },
	{ 0x51, "visibility" },
	{ 0x58, "children vector" },
	{ 0x88, "text box string" },
};

std::atomic<bool>      g_enabled{ false };
std::atomic<bool>      g_once{ false };
std::atomic<uintptr_t> g_lastWidget{ 0 };
std::atomic<uint64_t>  g_lastHash{ 0 };
std::atomic<unsigned long long> g_lastDumpAt{ 0 };

// A state flag only shows up by comparing the same control before and after
// it changes, and the console cannot be opened outside a loaded game -- so
// during character creation there is no way to ask for the second dump. The
// probe watches for the change itself instead: same widget, different bytes,
// dump again. Rate-limited, because a control that animates would otherwise
// dump every frame.
constexpr unsigned long long RedumpIntervalMs = 250;

uint64_t HashWidget(uintptr_t widget) {
	constexpr uint64_t Prime = 1099511628211ULL;
	unsigned char bytes[DumpBytes]{};
	const NativeMemory::ReadScope scope;
	if (!NativeMemory::Copy(widget, bytes, sizeof(bytes))) return 0;
	uint64_t hash = 14695981039346656037ULL; // FNV-1a, 64-bit
	for (unsigned char byte : bytes) hash = (hash ^ byte) * Prime;
	// 0 is the "unreadable" answer, so never return it for real content.
	return hash != 0 ? hash : 1;
}

const char* KnownAt(size_t offset) noexcept {
	for (const KnownOffset& known : KnownOffsets) {
		if (known.offset == offset) return known.what;
	}
	return nullptr;
}

void DumpRow(const D2RL::PluginContext* context, uintptr_t widget, size_t offset) {
	unsigned char bytes[RowBytes]{};
	const bool    ok = NativeMemory::Copy(widget + offset, bytes, RowBytes);

	char hex[RowBytes * 3 + 1]{};
	char ascii[RowBytes + 1]{};
	for (size_t i = 0; i < RowBytes; ++i) {
		if (ok) {
			std::snprintf(hex + i * 3, 4, "%02X ", bytes[i]);
			ascii[i] = (bytes[i] >= 0x20 && bytes[i] < 0x7F) ? static_cast<char>(bytes[i]) : '.';
		} else {
			std::memcpy(hex + i * 3, "?? ", 3);
			ascii[i] = '?';
		}
	}

	// Name any known field that starts inside this row, so the reader of the
	// log does not have to count columns to recognise one.
	char notes[128]{};
	size_t used = 0;
	for (size_t i = 0; i < RowBytes && used + 1 < sizeof(notes); ++i) {
		if (const char* what = KnownAt(offset + i); what != nullptr) {
			const int written = std::snprintf(notes + used, sizeof(notes) - used, "%s+0x%02zX %s",
				used == 0 ? "  <- " : ", ", offset + i, what);
			if (written <= 0) break;
			used += static_cast<size_t>(written);
		}
	}

	D2RAccess::AsyncLog::InfoF(context, "[probe]   +0x%03zX: %s |%s|%s", offset, hex, ascii, notes);
}

void Dump(const D2RL::PluginContext* context, uintptr_t panel, uintptr_t widget) {
	const NativeUi::Type type = NativeUi::Classify(widget);
	const std::string    name = NativeWidgets::ReadName(widget);
	D2RAccess::AsyncLog::InfoF(context, "[probe] focus panel=%p widget=%p type=%s name=\"%s\"",
		reinterpret_cast<void*>(panel), reinterpret_cast<void*>(widget),
		type.name.empty() ? "?" : type.name.c_str(), name.c_str());

	{
		const NativeMemory::ReadScope scope;
		for (size_t offset = 0; offset < DumpBytes; offset += RowBytes) {
			DumpRow(context, widget, offset);
		}
	}

	// Engine strings inside the object: the typed contents of a text field and
	// a dropdown's displayed value both live in one of these.
	for (const NativeWidgets::FoundString& found : NativeWidgets::ScanStrings(widget, DumpBytes)) {
		D2RAccess::AsyncLog::InfoF(context, "[probe]   string +0x%03zX \"%s\"", found.offset, found.value.c_str());
	}
}

}

void SetEnabled(bool enabled) noexcept {
	g_enabled.store(enabled);
	if (enabled) g_lastWidget.store(0);
}

bool Enabled() noexcept { return g_enabled.load(); }

void RequestOnce() noexcept {
	g_once.store(true);
	g_lastWidget.store(0);
}

void Rearm() noexcept { g_lastWidget.store(0); }

void Tick(const D2RL::PluginContext* context) {
	const bool once = g_once.load();
	if (!g_enabled.load() && !once) return;
	if (!NativeWidgets::IsAvailable() || !NativeWidgets::SupportsTree()) return;

	const NativeUi::Focus focus = NativeUi::CurrentFocus();
	if (focus.widget == 0) return;
	const bool sameWidget = focus.widget == g_lastWidget.load();
	const unsigned long long now = GetTickCount64();
	uint64_t hash = 0;
	if (sameWidget) {
		// Already dumped this one: only dump again once its bytes actually
		// differ, which is what a toggle looks like from out here.
		if (!g_enabled.load()) return;
		if (now - g_lastDumpAt.load() < RedumpIntervalMs) return;
		hash = HashWidget(focus.widget);
		if (hash == 0 || hash == g_lastHash.load()) return;
	}
	g_lastWidget.store(focus.widget);
	g_lastHash.store(hash != 0 ? hash : HashWidget(focus.widget));
	g_lastDumpAt.store(now);
	if (once) g_once.store(false);

	Dump(context, focus.panel, focus.widget);
}

}
