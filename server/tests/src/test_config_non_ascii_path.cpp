#include "tests/includes/test_framework.h"

#include "config/ime_config.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

namespace
{
// Sets an environment variable for the duration of a scope and restores it afterwards, so the config
// module can be pointed at a throwaway profile directory without leaking into other tests.
class ScopedEnv
{
  public:
    ScopedEnv(const wchar_t *name, const std::wstring &value) : name_(name)
    {
        wchar_t buffer[32768];
        const DWORD length = GetEnvironmentVariableW(name, buffer, 32768);
        had_previous_ = length != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        previous_.assign(buffer, length);
        SetEnvironmentVariableW(name, value.c_str());
    }
    ~ScopedEnv()
    {
        SetEnvironmentVariableW(name_.c_str(), had_previous_ ? previous_.c_str() : nullptr);
    }

    ScopedEnv(const ScopedEnv &) = delete;
    ScopedEnv &operator=(const ScopedEnv &) = delete;

  private:
    std::wstring name_;
    std::wstring previous_;
    bool had_previous_ = false;
};
} // namespace

// A non-ASCII (e.g. Chinese) user profile path must not break config persistence. Whatever the system
// ANSI code page is, writing a setting and reading it back has to round-trip, because the config module
// builds and opens the path as wide characters instead of routing it through the ANSI encoding (which
// corrupts the path, or throws on code pages that cannot represent the characters).
TEST_CASE(config_round_trips_under_non_ascii_profile_path)
{
    namespace fs = std::filesystem;
    const fs::path unique_root =
        fs::temp_directory_path() / (L"msime-配置测试-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    fs::create_directories(data_dir, ec);
    REQUIRE(!ec);
    // Seed the shipped default so SyncConfigWithInstalledTemplate can create config.toml.
    fs::copy_file(MSIME_DEFAULT_CONFIG_PATH, data_dir / L"config.default.toml", fs::copy_options::overwrite_existing,
                  ec);
    REQUIRE(!ec);

    {
        ScopedEnv local_app_data_env(L"LOCALAPPDATA", local_app_data.wstring());

        InitImeConfig();
        // The config file must land under the Chinese directory, not a mangled sibling.
        REQUIRE(fs::exists(data_dir / L"config.toml"));

        // Writing settings must succeed (this is what surfaced as "设置保存失败" for these users), using
        // values that differ from the shipped defaults so the assertions below actually prove a change.
        REQUIRE(SetConfiguredInputMode("japanese"));
        REQUIRE(SetConfiguredInputScheme("wubi"));

        // Re-initialise so the values are re-parsed from disk, proving they persisted through the file.
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE_EQ(GetConfiguredInputSchemeName(), std::string("wubi"));
    }

    fs::remove_all(unique_root, ec);
}
