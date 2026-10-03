#pragma once
// Opt-in performance timeline for startup, pause/resume and shutdown.
//
// Disabled unless the process starts with --halljoy-perf-log=<file>. When
// disabled every call is a single relaxed atomic load. Records hold static
// names, an index and QPC timestamps only (no input values, keys or paths)
// and are written once, at process exit, as plain text.
#include <cstdint>

namespace halljoy::perf
{

// Parses the command line; call once at process start.
void ConfigureFromCommandLine(const wchar_t* commandLine) noexcept;
bool Enabled() noexcept;
std::int64_t Now() noexcept;

// name must be a string literal (stored by pointer).
void Mark(const char* name, std::uint64_t index = 0) noexcept;
void Span(const char* name, std::uint64_t index, std::int64_t startQpc, const char* detail = nullptr) noexcept;
// Returns a stable copy of text for use as a record name/detail (perf mode only;
// a bounded table, never freed). Returns "?" when full.
const char* Intern(const char* text) noexcept;
// Writes the timeline and per-thread CPU times; safe to call more than once.
void Flush() noexcept;

class Scope
{
public:
    explicit Scope(const char* name, std::uint64_t index = 0, const char* detail = nullptr) noexcept
        : name_(name), detail_(detail), index_(index), start_(Enabled() ? Now() : 0) {}
    ~Scope() { if (start_) Span(name_, index_, start_, detail_); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    const char* name_;
    const char* detail_;
    std::uint64_t index_;
    std::int64_t start_;
};

} // namespace halljoy::perf
