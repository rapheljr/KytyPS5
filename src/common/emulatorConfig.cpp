#include "common/emulatorConfig.h"

#include "common/assert.h"

#include <algorithm>
#include <memory>

namespace Config {

static std::unique_ptr<ConfigOptions> g_config;

void Initialize() {
	EXIT_IF(g_config != nullptr);

	g_config = std::make_unique<ConfigOptions>();
}

void Shutdown() {
	g_config.reset();
}

static const ConfigOptions g_default_config{};

void Load(const ConfigOptions& cfg) {
	if (!g_config) {
		g_config = std::make_unique<ConfigOptions>();
	}
	*g_config = cfg;
}

uint32_t GetScreenWidth() {
	return g_config ? g_config->screen_width : g_default_config.screen_width;
}

uint32_t GetScreenHeight() {
	return g_config ? g_config->screen_height : g_default_config.screen_height;
}

bool FullscreenEnabled() {
	return g_config ? g_config->fullscreen_enabled : g_default_config.fullscreen_enabled;
}

uint32_t GetVblankFrequency() {
	return g_config ? std::clamp(g_config->vblank_frequency, 30u, 360u) : g_default_config.vblank_frequency;
}

uint32_t GetConsoleLanguage() {
	return g_config ? g_config->console_language : g_default_config.console_language;
}

GraphicBackendChoice GetGraphicBackend() {
	return g_config ? g_config->graphic_backend : GraphicBackendChoice::Default;
}

bool MetalCaptureEnabled() {
	return g_config ? g_config->metal_capture_enabled : false;
}

std::filesystem::path GetMetalCaptureFile() {
	return g_config ? g_config->metal_capture_file : "_metal_capture.gputrace";
}

bool VulkanValidationEnabled() {
	return g_config ? g_config->vulkan_validation_enabled : false;
}

bool ShaderValidationEnabled() {
	return g_config ? g_config->shader_validation_enabled : false;
}

ShaderOptimizationType GetShaderOptimizationType() {
	return g_config ? g_config->shader_optimization_type : g_default_config.shader_optimization_type;
}

ShaderLogDirection GetShaderLogDirection() {
	return g_config ? g_config->shader_log_direction : g_default_config.shader_log_direction;
}

std::filesystem::path GetShaderLogFolder() {
	return g_config ? g_config->shader_log_folder : g_default_config.shader_log_folder;
}

bool CommandBufferDumpEnabled() {
	return g_config ? g_config->command_buffer_dump_enabled : false;
}

std::filesystem::path GetCommandBufferDumpFolder() {
	return g_config ? g_config->command_buffer_dump_folder : g_default_config.command_buffer_dump_folder;
}

bool GraphicsDebugDumpEnabled() {
	return g_config ? g_config->graphics_debug_dump_enabled : false;
}

OutputDirection GetPrintfDirection() {
	return g_config ? g_config->printf_direction : g_default_config.printf_direction;
}

std::filesystem::path GetPrintfOutputFile() {
	return g_config ? g_config->printf_output_file : g_default_config.printf_output_file;
}

ProfilerDirection GetProfilerDirection() {
	return g_config ? g_config->profiler_direction : g_default_config.profiler_direction;
}

bool SpirvDebugPrintfEnabled() {
	return g_config ? g_config->spirv_debug_printf_enabled : false;
}

bool RenderDocEnabled() {
	return g_config ? g_config->renderdoc_enabled : false;
}

bool ReadbackLinearImagesEnabled() {
	return g_config ? g_config->readback_linear_images : false;
}

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
bool RedZoneProtectionEnabled() {
	return g_config ? g_config->red_zone_protection_enabled : false;
}
#endif

const Keymap& GetKeymap() {
	return g_config ? g_config->keymap : g_default_config.keymap;
}

} // namespace Config
