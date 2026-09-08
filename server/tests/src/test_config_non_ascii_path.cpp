#include "tests/includes/test_framework.h"

#include "config/ime_config.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace
{
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

std::filesystem::path MakeProfileRoot()
{
    return std::filesystem::temp_directory_path() / (L"msime-配置测试-" + std::to_wstring(GetCurrentProcessId()));
}

void SeedTemplate(const std::filesystem::path &data_dir)
{
    std::error_code ec;
    std::filesystem::create_directories(data_dir, ec);
    REQUIRE(!ec);
    std::filesystem::copy_file(MSIME_DEFAULT_CONFIG_PATH, data_dir / L"config.default.toml",
                               std::filesystem::copy_options::overwrite_existing, ec);
    REQUIRE(!ec);
}

void WriteText(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(static_cast<bool>(output));
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string ReadText(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}
} // namespace

TEST_CASE(config_round_trips_under_non_ascii_profile_path)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot();
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedEnv local_app_data_env(L"LOCALAPPDATA", local_app_data.wstring());

        InitImeConfig();
        REQUIRE(fs::exists(data_dir / L"config.toml"));

        REQUIRE(SetConfiguredInputMode("japanese"));
        REQUIRE(SetConfiguredInputScheme("wubi"));

        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE_EQ(GetConfiguredInputSchemeName(), std::string("wubi"));
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_recovers_unparseable_file_and_saves)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"损坏";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);
    WriteText(data_dir / L"config.toml", "this is not toml {{{");
    WriteText(data_dir / L"config.base.toml", ReadText(data_dir / L"config.default.toml"));

    {
        ScopedEnv local_app_data_env(L"LOCALAPPDATA", local_app_data.wstring());
        InitImeConfig();
        REQUIRE(SetConfiguredInputMode("japanese"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_overwrites_readonly_file)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"只读";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    {
        ScopedEnv local_app_data_env(L"LOCALAPPDATA", local_app_data.wstring());
        InitImeConfig();
        const fs::path config_path = data_dir / L"config.toml";
        REQUIRE(SetFileAttributesW(config_path.c_str(), FILE_ATTRIBUTE_READONLY));
        REQUIRE(SetConfiguredInputMode("japanese"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
    }

    fs::remove_all(unique_root, ec);
}

TEST_CASE(config_migrates_legacy_acp_mangled_path)
{
    namespace fs = std::filesystem;
    const fs::path unique_root = MakeProfileRoot() / L"遗留";
    const fs::path local_app_data = unique_root / L"本地";
    const fs::path data_dir = local_app_data / L"metasequoiaime";

    std::error_code ec;
    fs::remove_all(unique_root, ec);
    SeedTemplate(data_dir);

    fs::path mangled_dir;
    try
    {
        mangled_dir = fs::path(data_dir.u8string());
    }
    catch (...)
    {
        mangled_dir.clear();
    }
    if (mangled_dir.empty() || mangled_dir == data_dir)
    {
        fs::remove_all(unique_root, ec);
        return;
    }

    fs::create_directories(mangled_dir, ec);
    REQUIRE(!ec);
    const std::string stock = ReadText(data_dir / L"config.default.toml");
    const std::string from = "mode = \"chinese\"";
    const auto pos = stock.find(from);
    REQUIRE(pos != std::string::npos);
    WriteText(data_dir / L"config.toml", stock + "\n# leftover-installer-marker\n");
    std::string leftover = stock;
    leftover.replace(pos, from.size(), "mode = \"japanese\"");
    WriteText(mangled_dir / L"config.toml", leftover);

    {
        ScopedEnv local_app_data_env(L"LOCALAPPDATA", local_app_data.wstring());
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE(SetConfiguredInputScheme("wubi"));
        InitImeConfig();
        REQUIRE_EQ(GetConfiguredInputMode(), std::string("japanese"));
        REQUIRE_EQ(GetConfiguredInputSchemeName(), std::string("wubi"));
    }

    fs::remove_all(unique_root, ec);
}
