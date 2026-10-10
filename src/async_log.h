#pragma once

#include <D2RLPlugin/api.h>

namespace D2RAccess::AsyncLog {
// Keep the SDK logger, but never perform its synchronous disk flush on a UI callback.
bool Initialize();
void Shutdown();
// Best effort even during allocation failure or exception unwinding.
void Info(const D2RL::PluginContext* context, const char* message) noexcept;
void Warn(const D2RL::PluginContext* context, const char* message) noexcept;
void Error(const D2RL::PluginContext* context, const char* message) noexcept;
void InfoF(const D2RL::PluginContext* context, const char* format, ...) noexcept;
void WarnF(const D2RL::PluginContext* context, const char* format, ...) noexcept;
void ErrorF(const D2RL::PluginContext* context, const char* format, ...) noexcept;
}
