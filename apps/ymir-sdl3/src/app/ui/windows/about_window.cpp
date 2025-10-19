#include "about_window.hpp"

#include <ymir/version.hpp>

#include <util/std_lib.hpp>
#include <ymir/util/compiler_info.hpp>

#include <app/ui/fonts/IconsMaterialSymbols.h>

#include <SDL3/SDL_clipboard.h>

// Includes for versions only
#include <SDL3/SDL.h>
#include <curl/curlver.h>
#include <cxxopts.hpp>
#include <fmt/format.h>
#include <lz4.h>
#include <nghttp2/nghttp2ver.h>
#include <nghttp3/version.h>
#include <ngtcp2/version.h>
#include <nlohmann/json.hpp>
#include <openssl/opensslv.h>
#include <rtmidi/RtMidi.h>
#include <semver.hpp>
#include <toml++/toml.hpp>
#include <xxhash.h>

#define _STR_IMPL(x) #x
#define _STR(x) _STR_IMPL(x)
#define BROTLI_VERSION "1.1.0" // Not exported
#define CEREAL_VERSION "1.3.2" // Not exported
#define CMRC_VERSION "2.0.0"   // Not exported
#define CURL_VERSION _STR(LIBCURL_VERSION_MAJOR) "." _STR(LIBCURL_VERSION_MINOR) "." _STR(LIBCURL_VERSION_PATCH)
#define CXXOPTS_VERSION _STR(CXXOPTS__VERSION_MAJOR) "." _STR(CXXOPTS__VERSION_MINOR) "." _STR(CXXOPTS__VERSION_PATCH)
#define DATE_VERSION "3.0.4" // Not exported
#define IMGUI_VERSION_FULL IMGUI_VERSION " (" _STR(IMGUI_VERSION_NUM) ")"
#define LZMA_VERSION "24.05" // Private dependency of libchdr
#define MIO_VERSION "1.1.0"  // Not exported
#define NLOHMANN_JSON_VERSION \
    _STR(NLOHMANN_JSON_VERSION_MAJOR) "." _STR(NLOHMANN_JSON_VERSION_MINOR) "." _STR(NLOHMANN_JSON_VERSION_PATCH)
#define SDL_VERSION_STR _STR(SDL_MAJOR_VERSION) "." _STR(SDL_MINOR_VERSION) "." _STR(SDL_MICRO_VERSION)
#define SEMVER_VERSION _STR(SEMVER_VERSION_MAJOR) "." _STR(SEMVER_VERSION_MINOR) "." _STR(SEMVER_VERSION_PATCH)
#define STB_IMAGE_VERSION "2.30"       // Not exported
#define STB_IMAGE_WRITE_VERSION "1.16" // Not exported
#define MC_CONCQUEUE_VERSION "1.0.4"   // Not exported
#define TOMLPP_VERSION _STR(TOML_LIB_MAJOR) "." _STR(TOML_LIB_MINOR) "." _STR(TOML_LIB_PATCH)
#define XXHASH_VERSION _STR(XXH_VERSION_MAJOR) "." _STR(XXH_VERSION_MINOR) "." _STR(XXH_VERSION_RELEASE)
#define ZLIB_VERSION "1.3.1" // Private dependency of libchdr
#define ZSTD_VERSION "1.5.6" // Private dependency of libchdr

static const std::string fmtVersion = std::to_string(FMT_VERSION / 10000) + "." +
                                      std::to_string(FMT_VERSION / 100 % 100) + "." + std::to_string(FMT_VERSION % 100);

namespace app::ui {

struct License {
    const char *name;
    const char *url;
};

struct FontInfo {
    ImFont *font;
    float size;
};

struct FontDesc {
    using FontFn = FontInfo (*)(SharedContext &ctx);

    const char *name;
    const License &license;
    const char *url;
    FontFn fontFn;
    const char *demoText;
};

// clang-format off
inline constexpr License licenseApache2_0   {.name = "Apache-2.0",    .url = "https://opensource.org/licenses/Apache-2.0"};
inline constexpr License licenseBSD2        {.name = "BSD-2-Clause",  .url = "https://opensource.org/licenses/BSD-2-Clause"};
inline constexpr License licenseBSD3        {.name = "BSD-3-Clause",  .url = "https://opensource.org/licenses/BSD-3-Clause"};
//inline constexpr License licenseBSL         {.name = "BSL-1.0", .      url = "https://opensource.org/license/bsl-1-0"};
//inline constexpr License licenseISC         {.name = "ISC",           .url = "https://opensource.org/licenses/ISC"};
inline constexpr License licenseMIT         {.name = "MIT",           .url = "https://opensource.org/licenses/MIT"};
inline constexpr License licenseMITCurl     {.name = "MIT-cURL",      .url = "https://github.com/curl/curl/blob/master/COPYING"};
inline constexpr License licenseMITRtMidi   {.name = "MIT-RtMidi",    .url = "https://github.com/thestk/rtmidi/blob/master/LICENSE"};
inline constexpr License licensePublicDomain{.name = "Public domain", .url = nullptr};
//inline constexpr License licenseUnlicense   {.name = "Unlicense",     .url = "https://opensource.org/licenses/unlicense"};
inline constexpr License licenseZlib        {.name = "Zlib",          .url = "https://opensource.org/licenses/Zlib"};
inline constexpr License licenseOFL         {.name = "OFL-1.1",       .url = "https://opensource.org/licenses/OFL-1.1"};

static const struct {
    const char *name;
    const char *version = nullptr;
    const License &license;
    const char *repoURL;
    const char *licenseURL = nullptr;
    const bool repoPrivate = false;
    const char *homeURL = nullptr;
} depsCode[] = {
    {.name = "Brotli",                        .version = BROTLI_VERSION,             .license = licenseMIT,           .repoURL = "https://github.com/google/brotli",               .licenseURL = "https://github.com/google/brotli/blob/master/LICENSE"},
    {.name = "cereal",                        .version = CEREAL_VERSION,             .license = licenseBSD3,          .repoURL = "https://github.com/USCiLab/cereal",              .licenseURL = "https://github.com/USCiLab/cereal/blob/master/LICENSE",                  .homeURL = "https://uscilab.github.io/cereal/index.html"},
    {.name = "CMakeRC",                       .version = CMRC_VERSION,               .license = licenseMIT,           .repoURL = "https://github.com/vector-of-bool/cmrc",         .licenseURL = "https://github.com/vector-of-bool/cmrc/blob/master/LICENSE.txt"},
    {.name = "curl",                          .version = CURL_VERSION,               .license = licenseMITCurl,       .repoURL = "https://github.com/curl/curl",                   .licenseURL = "https://github.com/curl/curl/blob/master/COPYING",                       .homeURL = "https://curl.se/"},
    {.name = "cxxopts",                       .version = CXXOPTS_VERSION,            .license = licenseMIT,           .repoURL = "https://github.com/jarro2783/cxxopts",           .licenseURL = "https://github.com/jarro2783/cxxopts/blob/master/LICENSE"},
    {.name = "date",                          .version = DATE_VERSION,               .license = licenseMIT,           .repoURL = "https://github.com/HowardHinnant/date",          .licenseURL = "https://github.com/HowardHinnant/date/blob/master/LICENSE.txt"},
    {.name = "Dear ImGui",                    .version = IMGUI_VERSION_FULL,         .license = licenseMIT,           .repoURL = "https://github.com/ocornut/imgui",               .licenseURL = "https://github.com/ocornut/imgui/blob/master/LICENSE.txt"},
    {.name = "{fmt}",                         .version = fmtVersion.c_str(),         .license = licenseMIT,           .repoURL = "https://github.com/fmtlib/fmt",                  .licenseURL = "https://github.com/fmtlib/fmt/blob/master/LICENSE",                      .homeURL = "https://fmt.dev/latest/index.html"},
    {.name = "ImGui Club",                                                           .license = licenseMIT,           .repoURL = "https://github.com/ocornut/imgui_club",          .licenseURL = "https://github.com/ocornut/imgui_club/blob/main/LICENSE.txt"},
    {.name = "libchdr",                                                              .license = licenseBSD3,          .repoURL = "https://github.com/rtissera/libchdr",            .licenseURL = "https://github.com/rtissera/libchdr/blob/master/LICENSE.txt"},
    {.name = "lz4",                           .version = LZ4_VERSION_STRING,         .license = licenseBSD2,          .repoURL = "https://github.com/lz4/lz4",                     .licenseURL = "https://github.com/lz4/lz4/blob/dev/lib/LICENSE",                        .homeURL = "https://lz4.org/",},
    {.name = "lzma",                          .version = LZMA_VERSION,               .license = licensePublicDomain,                                                                                                                                                       .homeURL = "https://www.7-zip.org/sdk.html",},
    {.name = "mio",                           .version = MIO_VERSION,                .license = licenseMIT,           .repoURL = "https://github.com/StrikerX3/mio",               .licenseURL = "https://github.com/StrikerX3/mio/blob/master/LICENSE"},
    {.name = "moodycamel::\nConcurrentQueue", .version = "\n" MC_CONCQUEUE_VERSION,  .license = licenseBSD2,          .repoURL = "https://github.com/cameron314/concurrentqueue",  .licenseURL = "https://github.com/cameron314/concurrentqueue/blob/master/LICENSE.md"},
    {.name = "Neargye/semver",                .version = SEMVER_VERSION,             .license = licenseMIT,           .repoURL = "https://github.com/Neargye/semver",              .licenseURL = "https://github.com/Neargye/semver/blob/master/LICENSE"},
    {.name = "nghttp2",                       .version = NGHTTP2_VERSION,            .license = licenseMIT,           .repoURL = "https://github.com/nghttp2/nghttp2",             .licenseURL = "https://github.com/nghttp2/nghttp2/blob/master/COPYING",                 .homeURL = "https://nghttp2.org/"},
    {.name = "nghttp3",                       .version = NGHTTP3_VERSION,            .license = licenseMIT,           .repoURL = "https://github.com/ngtcp2/nghttp3",              .licenseURL = "https://github.com/ngtcp2/nghttp3/blob/main/COPYING",                    .homeURL = "https://nghttp2.org/nghttp3/"},
    {.name = "ngtcp2",                        .version = NGTCP2_VERSION,             .license = licenseMIT,           .repoURL = "https://github.com/ngtcp2/ngtcp2",               .licenseURL = "https://github.com/ngtcp2/ngtcp2/blob/main/COPYING",                     .homeURL = "https://nghttp2.org/ngtcp2/"},
    {.name = "nlohmann/json",                 .version = NLOHMANN_JSON_VERSION,      .license = licenseMIT,           .repoURL = "https://github.com/nlohmann/json",               .licenseURL = "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT",              .homeURL = "https://json.nlohmann.me/"},
    {.name = "OpenSSL",                       .version = OPENSSL_FULL_VERSION_STR,   .license = licenseApache2_0,     .repoURL = "https://github.com/openssl/openssl",             .licenseURL = "https://github.com/openssl/openssl/blob/master/LICENSE.txt",             .homeURL = "https://www.openssl.org/"},
    {.name = "RtMidi",                        .version = RTMIDI_VERSION,             .license = licenseMITRtMidi,     .repoURL = "https://github.com/thestk/rtmidi",               .licenseURL = "https://github.com/thestk/rtmidi/blob/master/LICENSE"},
    {.name = "SDL3",                          .version = SDL_VERSION_STR,            .license = licenseZlib,          .repoURL = "https://github.com/libsdl-org/SDL",              .licenseURL = "https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt"},
    {.name = "SDL_GameControllerDB",                                                 .license = licenseZlib,          .repoURL = "https://github.com/mdqinc/SDL_GameControllerDB", .licenseURL = "https://github.com/mdqinc/SDL_GameControllerDB/blob/master/LICENSE"},
    {.name = "stb_image",                     .version = STB_IMAGE_VERSION,          .license = licenseMIT,           .repoURL = "https://github.com/nothings/stb",                .licenseURL = "https://github.com/nothings/stb/blob/master/LICENSE"},
    {.name = "stb_image_write",               .version = STB_IMAGE_WRITE_VERSION,    .license = licenseMIT,           .repoURL = "https://github.com/nothings/stb",                .licenseURL = "https://github.com/nothings/stb/blob/master/LICENSE"},
    {.name = "toml++",                        .version = TOMLPP_VERSION,             .license = licenseMIT,           .repoURL = "https://github.com/marzer/tomlplusplus" ,        .licenseURL = "https://github.com/marzer/tomlplusplus/blob/master/LICENSE",             .homeURL = "https://marzer.github.io/tomlplusplus/"},
    {.name = "xxHash",                        .version = XXHASH_VERSION,             .license = licenseBSD2,          .repoURL = "https://github.com/Cyan4973/xxHash",             .licenseURL = "https://github.com/Cyan4973/xxHash/blob/dev/LICENSE",                    .homeURL = "https://xxhash.com/"},
    {.name = "zlib",                          .version = ZLIB_VERSION,               .license = licenseZlib,          .repoURL = "https://github.com/madler/zlib",                 .licenseURL = "https://github.com/madler/zlib/blob/develop/LICENSE",                    .homeURL = "https://zlib.net/"},
    {.name = "zstd",                          .version = ZSTD_VERSION,               .license = licenseBSD3,          .repoURL = "https://github.com/facebook/zstd",               .licenseURL = "https://github.com/facebook/zstd/blob/dev/LICENSE",                      .homeURL = "http://www.zstd.net/"},
};


static const char *demoTextStandard =
    "The quick brown fox jumps over the lazy dog\n"
    "0123456789 `~!@#$%^&*()_+-=[]{}<>,./?;:'\"\\|\n"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ  \u00C0\u00C9\u00CE\u00D5\u00DA\u00D1\u00C7\u00DD\n"
    "abcdefghijklmnopqrstuvwxyz  \u00E0\u00E9\u00EE\u00F5\u00FA\u00F1\u00E7\u00FD";

static const char *demoTextMaterialSymbols =
    ICON_MS_HOME          ICON_MS_HELP         ICON_MS_FOLDER         ICON_MS_DOCS             ICON_MS_SETTINGS      ICON_MS_MENU         ICON_MS_HISTORY      ICON_MS_HISTORY_OFF    "\n"
    ICON_MS_PLAY_ARROW    ICON_MS_PAUSE        ICON_MS_PLAY_PAUSE     ICON_MS_ARROW_BACK_2     ICON_MS_FAST_FORWARD  ICON_MS_FAST_REWIND  ICON_MS_SKIP_NEXT    ICON_MS_SKIP_PREVIOUS  "\n"
    ICON_MS_VOLUME_MUTE   ICON_MS_VOLUME_UP    ICON_MS_VOLUME_DOWN    ICON_MS_VOLUME_OFF       ICON_MS_NO_SOUND      ICON_MS_TUNE         ICON_MS_EJECT        ICON_MS_ALBUM          "\n"
    ICON_MS_STEP          ICON_MS_STEP_INTO    ICON_MS_STEP_OVER      ICON_MS_STEP_OUT         ICON_MS_BUG_REPORT    ICON_MS_CODE         ICON_MS_MEMORY       ICON_MS_TV             "\n"
    ICON_MS_CONTENT_COPY  ICON_MS_CONTENT_CUT  ICON_MS_CONTENT_PASTE  ICON_MS_VIDEOGAME_ASSET  ICON_MS_JOYSTICK      ICON_MS_GAMEPAD      ICON_MS_MOUSE        ICON_MS_KEYBOARD;

static const FontDesc fontDescs[] = {
    { .name = "Material Symbols", .license = licenseApache2_0, .url = "https://fonts.google.com/icons",               .fontFn = [](SharedContext &ctx) -> FontInfo { return {ctx.fonts.sansSerif.regular, 24.0f}; }, .demoText = demoTextMaterialSymbols },
    { .name = "Spline Sans",      .license = licenseOFL,       .url = "https://github.com/SorkinType/SplineSans",     .fontFn = [](SharedContext &ctx) -> FontInfo { return {ctx.fonts.sansSerif.regular, 16.0f}; }, .demoText = demoTextStandard },
    { .name = "Spline Sans Mono", .license = licenseOFL,       .url = "https://github.com/SorkinType/SplineSansMono", .fontFn = [](SharedContext &ctx) -> FontInfo { return {ctx.fonts.monospace.regular, 16.0f}; }, .demoText = demoTextStandard },
    { .name = "Zen Dots",         .license = licenseOFL,       .url = "https://github.com/googlefonts/zen-dots",      .fontFn = [](SharedContext &ctx) -> FontInfo { return {ctx.fonts.display,           24.0f}; }, .demoText = demoTextStandard },
};
// clang-format on

static const std::unordered_map<std::string_view, const char *> kRenderers = {
    {"vulkan", "Vulkan"}, {"direct3d", "Direct3D 9"}, {"direct3d11", "Direct3D 11"}, {"direct3d12", "Direct3D 12"},
    {"metal", "Metal"},   {"opengl", "OpenGL"},       {"opengles2", "OpenGL ES 2"},
};

const char *RendererToHumanReadableString(std::string_view driver) {
    if (kRenderers.contains(driver)) {
        return kRenderers.at(driver);
    }
    return driver.data();
}

// If only SDL3 exposed the nice desc field they already have in the SDL_AudioDriver struct...
// Also note that just because certain systems are listed here, it doesn't mean Ymir actually supports them.
static const std::unordered_map<std::string_view, const char *> kAudioDrivers = {
    {"AAudio", "AAudio audio driver"},
    {"alsa", "ALSA PCM audio"},
    {"coreaudio", "CoreAudio"},
    {"directsound", "DirectSound"},
    {"disk", "direct-to-disk audio"},
    {"dsp", "Open Sound System (/dev/dsp)"},
    {"dummy", "SDL dummy audio driver"},
    {"emscripten", "SDL emscripten audio driver"},
    {"haiku", "Haiku BSoundPlayer"},
    {"jack", "JACK Audio Connection Kit"},
    {"netbsd", "NetBSD audio"},
    {"N-Gage", "N-Gage audio driver"},
    {"n3ds", "SDL N3DS audio driver"},
    {"openslES", "OpenSL ES audio driver"},
    {"pipewire", "Pipewire"},
    {"psp", "PSP audio driver"},
    {"ps2", "PS2 audio driver"},
    {"pulseaudio", "PulseAudio"},
    {"qsa", "QNX QSA Audio"},
    {"sndio", "OpenBSD sndio"},
    {"vita", "VITA audio driver"},
    {"wasapi", "WASAPI"},
};

const char *AudioDriverToHumanReadableString(std::string_view driver) {
    if (kAudioDrivers.contains(driver)) {
        return kAudioDrivers.at(driver);
    }
    return "unknown";
}

AboutWindow::AboutWindow(SharedContext &context)
    : WindowBase(context) {

    m_windowConfig.name = "About";
}

void AboutWindow::PrepareWindow() {
    auto *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(660 * m_context.displayScale, 800 * m_context.displayScale),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(400 * m_context.displayScale, 240 * m_context.displayScale),
                                        ImVec2(1000 * m_context.displayScale, 900 * m_context.displayScale));
}

void AboutWindow::DrawContents() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("About")) {
            if (ImGui::BeginChild("##about")) {
                DrawAboutTab();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Dependencies")) {
            if (ImGui::BeginChild("##dependencies")) {
                DrawDependenciesTab();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Acknowledgements")) {
            if (ImGui::BeginChild("##acknowledgements", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
                DrawAcknowledgementsTab();
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void AboutWindow::DrawAboutTab() {
    ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);

    ImGui::Image((ImTextureID)m_context.images.ymirLogo.texture,
                 ImVec2(m_context.images.ymirLogo.size.x * m_context.displayScale,
                        m_context.images.ymirLogo.size.y * m_context.displayScale));

    ImGui::PushFont(m_context.fonts.display, m_context.fontSizes.display);
    ImGui::TextUnformatted("Ymir");
    ImGui::PopFont();
    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.xlarge);
    ImGui::TextUnformatted("Version " Ymir_VERSION);
    ImGui::PopFont();
#if Ymir_DEV_BUILD
    ImGui::SameLine();
    ImGui::PushFont(m_context.fonts.sansSerif.regular, m_context.fontSizes.xlarge);
    ImGui::TextUnformatted("(development build)");
    ImGui::PopFont();
#endif

    ImGui::PushFont(m_context.fonts.sansSerif.regular, m_context.fontSizes.large);
    ImGui::TextUnformatted("A Sega Saturn emulator");
    ImGui::PopFont();

    if (ImGui::Button("Copy version")) {
        SDL_SetClipboardText(Ymir_VERSION);
    }

    ImGui::NewLine();
    ImGui::Text("Compiled with %s %s.", compiler::name, compiler::version::string.c_str());
#ifdef Ymir_BUILD_TIMESTAMP
    if (auto buildTimestamp = util::parse8601(Ymir_BUILD_TIMESTAMP)) {
        auto localBuildTime = util::to_local_time(std::chrono::system_clock::time_point(*buildTimestamp));
        ImGui::TextUnformatted(fmt::format("Built at {}", localBuildTime).c_str());
    }
#endif
    if constexpr (ymir::version::is_nightly_build) {
        ImGui::TextUnformatted("Nightly release channel.");
    } else if constexpr (ymir::version::is_stable_build) {
        ImGui::TextUnformatted("Stable release channel.");
    } else if constexpr (ymir::version::is_local_build) {
        ImGui::TextUnformatted("Local development build.");
    }

#if defined(__x86_64__) || defined(_M_X64)
    #ifdef Ymir_AVX2
    ImGui::Text("Using AVX2 instruction set.");
    #else
    ImGui::Text("Using SSE2 instruction set.");
    #endif
#elif defined(__aarch64__) || defined(__arm64__)
    ImGui::Text("Using NEON instruction set.");
#endif

    SDL_PropertiesID rendererProps = SDL_GetRendererProperties(m_context.screen.renderer);
    std::string_view rendererName = SDL_GetStringProperty(rendererProps, SDL_PROP_RENDERER_NAME_STRING, "unknown");
    const char *graphicsBackendName = "unknown";
    if (rendererName == "gpu") {
        auto *gpuDevice = static_cast<SDL_GPUDevice *>(
            SDL_GetPointerProperty(rendererProps, SDL_PROP_RENDERER_GPU_DEVICE_POINTER, nullptr));
        if (gpuDevice) {
            const char *gpuDriver = SDL_GetGPUDeviceDriver(gpuDevice);
            graphicsBackendName = RendererToHumanReadableString(gpuDriver);
        } else {
            graphicsBackendName = "SDL GPU";
        }
    } else {
        graphicsBackendName = RendererToHumanReadableString(rendererName);
    }
    ImGui::Text("Using %s graphics backend for GUI rendering.", graphicsBackendName);
    ImGui::TextUnformatted("Using software VDP1/VDP2 renderer.");

    const char *audioDriver = SDL_GetCurrentAudioDriver();
    ImGui::Text("Using %s audio driver.", AudioDriverToHumanReadableString(audioDriver));

    ImGui::NewLine();
    ImGui::TextUnformatted("Licensed under ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("GPLv3", "https://www.gnu.org/licenses/gpl-3.0.en.html");

    ImGui::TextUnformatted("The source code can be found at ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("https://github.com/StrikerX3/Ymir");

    ImGui::NewLine();
    ImGui::TextUnformatted("Join the official ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("Discord server", "https://discord.gg/NN3A7n5dzn");

    ImGui::TextUnformatted("Consider supporting my work on ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("Patreon", "https://www.patreon.com/StrikerX3");

    ImGui::PopTextWrapPos();
}

void AboutWindow::DrawDependenciesTab() {
    static constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_SizingFixedFit;

    // -----------------------------------------------------------------------------

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Libraries");
    ImGui::PopFont();

    if (ImGui::BeginTable("libraries", 3, kTableFlags)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("License");
        ImGui::TableSetupColumn("Links");
        ImGui::TableHeadersRow();

        for (auto &dep : depsCode) {
            ImGui::PushID(dep.name);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.medium);
            ImGui::TextUnformatted(dep.name);
            ImGui::PopFont();
            if (dep.version != nullptr) {
                ImGui::SameLine();
                // TODO: don't hardcode colors!
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.77f, 0.8f, 1.0f));
                ImGui::TextUnformatted(dep.version);
                ImGui::PopStyleColor();
            }

            ImGui::TableSetColumnIndex(1);
            if (dep.licenseURL != nullptr) {
                ImGui::TextLinkOpenURL(dep.license.name, dep.licenseURL);
            } else if (dep.license.url != nullptr) {
                ImGui::TextLinkOpenURL(dep.license.name, dep.license.url);
            } else {
                ImGui::TextUnformatted(dep.license.name);
            }

            ImGui::TableSetColumnIndex(2);
            if (dep.repoURL != nullptr) {
                ImGui::TextLinkOpenURL(dep.repoURL);
            }
            if (dep.repoPrivate) {
                ImGui::SameLine();
                // TODO: don't hardcode colors!
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.77f, 0.8f, 1.0f));
                ImGui::TextUnformatted("(private)");
                ImGui::PopStyleColor();
            }
            if (dep.homeURL != nullptr) {
                ImGui::TextLinkOpenURL(dep.homeURL);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // -----------------------------------------------------------------------------

    ImGui::Separator();

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Fonts");
    ImGui::PopFont();

    if (ImGui::BeginTable("fonts", 3, kTableFlags)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("License");
        ImGui::TableSetupColumn("Link");
        ImGui::TableHeadersRow();

        for (auto &font : fontDescs) {
            ImGui::PushID(font.name);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            auto cursor = ImGui::GetCursorPos();
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
            ImGui::Selectable("", false, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered() && (ImGui::TableGetColumnFlags(0) & ImGuiTableColumnFlags_IsHovered)) {
                ImGui::BeginTooltip();
                auto [fontPtr, fontSize] = font.fontFn(m_context);
                ImGui::PushFont(fontPtr, fontSize);
                ImGui::TextUnformatted(font.demoText);
                ImGui::PopFont();
                ImGui::EndTooltip();
            }
            ImGui::SetCursorPos(cursor);

            ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.medium);
            ImGui::TextUnformatted(font.name);
            ImGui::PopFont();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextLinkOpenURL(font.license.name, font.license.url);

            ImGui::TableSetColumnIndex(2);
            ImGui::TextLinkOpenURL(font.url);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void AboutWindow::DrawAcknowledgementsTab() {
    ImGui::PushTextWrapPos(ImGui::GetWindowContentRegionMax().x);

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Ymir was made possible by");
    ImGui::PopFont();

    auto ack = [&](const char *name, const char *url) {
        ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.medium);
        ImGui::TextLinkOpenURL(name, url);
        ImGui::PopFont();
    };

    auto ackWithAuthor = [&](const char *name, const char *author, const char *url) {
        ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.medium);
        ImGui::TextLinkOpenURL(name, url);
        ImGui::PopFont();

        ImGui::SameLine();

        ImGui::PushFont(m_context.fonts.sansSerif.regular, m_context.fontSizes.medium);
        ImGui::Text("by %s", author);
        ImGui::PopFont();
    };

    ackWithAuthor("antime's feeble Sega Saturn page", "antime", "https://antime.kapsi.fi/sega/");
    ackWithAuthor("Hardware research and signal traces", "Sergiy Dvodnenko (srg320)",
                  "https://github.com/srg320/Saturn_hw");
    ackWithAuthor("Original research", "Charles MacDonald",
                  "https://web.archive.org/web/20150119062930/http://cgfm2.emuviews.com/saturn.php");
    {
        ImGui::Indent();
        ack("Sega Saturn hardware notes (sattech.txt)",
            "https://web.archive.org/web/20140318183509/http://cgfm2.emuviews.com/txt/sattech.txt");
        ack("VDP1 hardware notes (vdp1tech.txt)",
            "https://web.archive.org/web/20150106171745/http://cgfm2.emuviews.com/sat/vdp1tech.txt");
        ack("Sega Saturn Cartridge Information (satcart.txt)",
            "https://web.archive.org/web/20140724061526/http://cgfm2.emuviews.com/sat/satcart.txt");
        ack("EMS Action Replay Plus notes (satar.txt)",
            "https://web.archive.org/web/20140724045721/http://cgfm2.emuviews.com/sat/satar.txt");
        ack("Comms Link hardware notes (comminfo.txt)",
            "https://web.archive.org/web/20140724035829/http://cgfm2.emuviews.com/sat/comminfo.txt");
        ImGui::Unindent();
    }
    ackWithAuthor("Collection of Dreamcast docs", "Senryoku",
                  "https://github.com/Senryoku/dreamcast-docs/tree/master/AICA/DOCS");
    {
        ImGui::Indent();
        ackWithAuthor("Original AICA research", "Neill Corlett",
                      "https://raw.githubusercontent.com/Senryoku/dreamcast-docs/refs/heads/master/"
                      "AICA/DOCS/myaica.txt");
        ImGui::Unindent();
    }
    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.medium);
    ImGui::TextUnformatted("CD block research");
    ImGui::PopFont();
    {
        ImGui::Indent();
        ackWithAuthor("CD block modchip research", "Pinchy",
                      "https://segaxtreme.net/threads/finally-made-a-working-modchip.14781/");
        ackWithAuthor("CD interface and signal traces", "Pinchy",
                      "https://web.archive.org/web/20111203080908/http://www.crazynation.org/SEGA/Saturn/cd_tech.htm");
        ackWithAuthor(
            "CD drive command log", "Pinchy",
            "https://web.archive.org/web/20111011104440/http://www.crazynation.org/SEGA/Saturn/files/command_log.txt");
        ackWithAuthor(
            "Raw optical disc format", "Joachim Metz",
            "https://github.com/libyal/libodraw/blob/main/documentation/Optical%20disc%20RAW%20format.asciidoc");
        ackWithAuthor("YGR registers, ROM disassembly and signal traces", "Sergiy Dvodnenko (srg320)",
                      "https://github.com/srg320/Saturn_hw/tree/main/CDB");
        ImGui::Unindent();
    }
    ack("Yabause wiki", "http://wiki.yabause.org/");
    ack("SegaRetro on Sega Saturn", "https://segaretro.org/Sega_Saturn");

    // -----------------------------------------------------------------------------

    ImGui::NewLine();

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Helpful tools and test suites");
    ImGui::PopFont();

    ackWithAuthor("libyaul", "mrkotfw and contributors", "https://github.com/yaul-org/libyaul");
    ackWithAuthor("libyaul-examples", "mrkotfw and contributors", "https://github.com/yaul-org/libyaul-examples");
    ackWithAuthor("saturn-tests", "StrikerX3", "https://github.com/StrikerX3/saturn-tests");
    ackWithAuthor("SH-4 single step tests", "raddad772", "https://github.com/SingleStepTests/sh4");
    ackWithAuthor("M68000 single step tests", "raddad772", "https://github.com/SingleStepTests/m68000");
    ackWithAuthor("Various tests", "celeriyacon", "https://github.com/celeriyacon");
    ImGui::Indent();
    ImGui::TextUnformatted("cdbtest, memtimes, misctest, scspadpcm, scsptest, scutest, sh2test, smpctest and vdp2test");
    ImGui::Unindent();

    // -----------------------------------------------------------------------------

    ImGui::NewLine();

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Other emulators that inspired Ymir");
    ImGui::PopFont();

    ackWithAuthor("Saturn MiSTer", "Sergiy Dvodnenko (srg320)", "https://github.com/MiSTer-devel/Saturn_MiSTer");

    ackWithAuthor("Mednafen", "various contributors", "https://mednafen.github.io/");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("(libretro git mirror)##mednafen", "https://github.com/libretro-mirrors/mednafen-git");

    ackWithAuthor("Yaba Sanshiro 2", "devmiyax", "https://github.com/devmiyax/yabause");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("(site)##yaba_sanshiro_2", "https://www.uoyabause.org/");

    ackWithAuthor("Yabause", "Guillaume Duhamel and contributors", "https://github.com/Yabause/yabause");

    ackWithAuthor("Mesen2", "Sour and contributors", "https://github.com/SourMesen/Mesen2");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("(site)##mesen", "https://www.mesen.ca/");

    ackWithAuthor("openMSX", "openMSX developers", "https://github.com/openMSX/openMSX");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("(site)##openmsx", "https://openmsx.org/");

    ackWithAuthor("DuckStation", "Stenzek and contributors", "https://github.com/stenzek/duckstation");
    ImGui::SameLine();
    ImGui::TextLinkOpenURL("(site)##duckstation", "https://www.duckstation.org/");

    // -----------------------------------------------------------------------------

    ImGui::NewLine();

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("Special thanks");
    ImGui::PopFont();

    ImGui::TextUnformatted("To the ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("/r/EmuDev community", "https://www.reddit.com/r/EmuDev/");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(" and their ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("Discord server", "https://discord.gg/dkmJAes");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(".");

    ImGui::TextUnformatted("To the ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("project contributors", "https://github.com/StrikerX3/Ymir/graphs/contributors");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(" and users ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("reporting issues and feature requests", "https://github.com/StrikerX3/Ymir/issues");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(", including:");
    ImGui::Indent();
    ImGui::TextUnformatted("BlueInterlude, bsdcode, Citrodata, floreal, GlaireDaggers, lvsweat, mmkzer0, ronan22, "
                           "tegaidogun, tordona, Wunkolo.");
    ImGui::Unindent();

    ImGui::TextUnformatted("To the friends in the ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("official Ymir Discord server", "https://discord.gg/NN3A7n5dzn");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(", especially:");
    ImGui::Indent();
    ImGui::TextUnformatted(
        "Aydan Watkins, celeriyacon, Charles / thelastangryman1907, Damian Gracz, fathamburger, GoodWall_533, Jano, "
        "Katanchiro, Lordus, sasori95 / Immersion95, secreto7, Silanda, Sorer, TheCoolPup, waspennator, Zet-sensei.");
    ImGui::Unindent();

    ImGui::TextUnformatted("To the current and former ");
    ImGui::SameLine(0, 0);
    ImGui::TextLinkOpenURL("Patreon supporters", "https://www.patreon.com/StrikerX3");
    ImGui::SameLine(0, 0);
    ImGui::TextUnformatted(":");
    ImGui::Indent();
    ImGui::TextUnformatted("Aitor Guevara, Aydan Watkins, Diego Bartolom\u00E9, Elcorsico 28, Israel Jacquez, Jeff "
                           "Greulich, Julien P, khalifax10, Mored4u, Munch, Oliver Stadler.");
    ImGui::Unindent();

    ImGui::PushFont(m_context.fonts.sansSerif.bold, m_context.fontSizes.large);
    ImGui::TextUnformatted("And YOU!");
    ImGui::PopFont();

    ImGui::PopTextWrapPos();
}

} // namespace app::ui
