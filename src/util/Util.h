#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif

// Executable directory (Windows/macOS/Linux).
inline std::filesystem::path exeDir()
{
    namespace fs = std::filesystem;

#if defined(_WIN32)
    // Wide-char; supports long paths if OS setting is enabled.
    wchar_t buf[32768];
    HMODULE hMod = GetModuleHandleW(nullptr);
    DWORD len = GetModuleFileNameW(hMod, buf, static_cast<DWORD>(std::size(buf)));

    if (len == 0 || len == std::size(buf))
    {
        // Fallback: current dir if something goes wrong.
        return fs::current_path();
    }

    fs::path p(buf);

    return p.parent_path();

#elif defined(__APPLE__)
    // Resolve executable path then canonicalize.
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string tmp(size, '\0');

    if (_NSGetExecutablePath(tmp.data(), &size) != 0) return fs::current_path();

    fs::path p = fs::weakly_canonical(fs::path(tmp));

    return p.parent_path();

#else
    // Linux/Unix: /proc/self/exe symlink -> absolute path to binary.
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);

    if (n >= 0)
    {
        buf[n] = '\0';
        return fs::path(buf).parent_path();

    }

    return fs::current_path();
#endif
}

// Try several locations to resolve a shader path.
inline std::filesystem::path resolveShaderPath(const std::string& in)
{
    namespace fs = std::filesystem;

    std::vector<fs::path> tries;
    fs::path p(in);

    tries.push_back(p);
    tries.push_back(exeDir() / p);
    tries.push_back(exeDir() / "shaders" / p.filename());
    tries.push_back(exeDir().parent_path() / "shaders" / p.filename());
    tries.push_back(fs::current_path() / p);
    tries.push_back(fs::current_path() / "shaders" / p.filename());

    for (auto& t : tries)
    {
        std::error_code ec;
        if (fs::exists(t, ec) && !ec) return t;
    }

    std::cerr << "Shader Program Could not resolve '" << in << "'.\n"
        << "  CWD: " << fs::current_path().string() << "\n"
        << "  EXE: " << exeDir().string() << "\n";

    return in; // Let caller fail loudly with the unresolved path string.
}

// Ensure an output directory next to the executable.
inline std::filesystem::path ensureOutputDir(const std::string& name = "output")
{
    namespace fs = std::filesystem;

    fs::path out = exeDir() / name;
    std::error_code ec;
    fs::create_directories(out, ec); // Fine if it already exists.

    return out;
}