#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cwchar>

#include "analog_key_codes.h"

namespace halljoy::alumix104 {

// Exact Alumix 104 Yotei map from the pinned official configurator. Sparse
// physical indices are intentional. The calculator's consumer key (120) has
// no keyboard HID usage and is not exposed as an analog game binding.
inline constexpr std::array<std::uint16_t, 121> kFactoryHid{
    41,58,59,60,61,62,63,64,65,66,67,
    68,69,0,0,0,53,30,31,32,33,34,
    35,36,37,38,39,45,46,83,84,85,43,
    20,26,8,21,23,28,24,12,18,19,47,
    48,95,96,97,57,4,22,7,9,10,11,
    13,14,15,51,0,49,92,93,94,225,
    29,27,6,25,5,17,16,54,55,56,
    229,40,89,90,91,224,227,226,44,230,
    halljoy::keycode::kFn,101,228,80,81,82,79,42,98,99,88,
    0,0,0,70,71,0,72,73,74,75,76,77,78,86,
    87,0,0,0,0,0,0,0,0,0,0
};

inline bool ExactProduct(const wchar_t* name) noexcept {
    return name && std::wcscmp(name, L"Alumix 104 Yotei Magnetic") == 0;
}
inline bool ExactCollection(unsigned page, unsigned usage,
                            unsigned inputBytes, unsigned outputBytes) noexcept {
    return page == 0xff68 && usage == 0x61 &&
           inputBytes == 65 && outputBytes == 65;
}

struct TravelSample {
    std::uint8_t index = 0;
    std::uint16_t stroke = 0;
    std::uint16_t maximum = 0;
    std::uint16_t milli = 0;
    bool zeroMaximumPositive = false;
    bool aboveFull = false;
    bool mapped = false;
};

constexpr std::size_t kBatchKeys = 7;
using BatchFrame = std::array<std::uint8_t, 65>;
enum class BatchReplyKind : std::uint8_t { Unrelated, Echo, Candidate, Malformed };
struct BatchReading {
    std::uint8_t index = 0;
    std::uint8_t status = 0;
    std::uint16_t adc = 0;
    std::uint16_t stroke = 0;
};

// Official configurator 0x68 packet: seven requested physical indices occupy
// seven eight-byte records. The offset is only a reply correlation token here;
// calibration and simulation flags are never sent by this query.
inline bool MakeBatchRequest(const std::uint8_t* indices, std::size_t count,
                             std::uint16_t offset, BatchFrame& out) noexcept {
    if (!indices || !count || count > kBatchKeys) return false;
    BatchFrame next{};
    next[1] = 0xaa; next[2] = 0x68;
    next[3] = static_cast<std::uint8_t>(count * 8);
    next[4] = static_cast<std::uint8_t>(offset);
    next[5] = static_cast<std::uint8_t>(offset >> 8);
    next[7] = 1; // The official one-packet query marks its final packet.
    for (std::size_t i = 0; i < count; ++i) {
        if (indices[i] >= kFactoryHid.size()) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (indices[j] == indices[i]) return false;
        next[9 + i * 8] = indices[i];
    }
    out = next;
    return true;
}

inline BatchReplyKind ParseBatchReply(const std::uint8_t* report,
                                      std::size_t bytes, const BatchFrame& request,
                                      const std::uint8_t* indices, std::size_t count,
                                      std::array<BatchReading, kBatchKeys>& out) noexcept {
    if (!report || bytes != 65 || report[0] != 0 || report[1] != 0x55 ||
        report[2] != 0x68) return BatchReplyKind::Unrelated;
    // The vendor decoder calls byte 2 lenOrType and accepts either meaning.
    // The address must match; byte 2 is recorded by the caller, not rejected.
    if (!indices || !count || count > kBatchKeys ||
        report[4] != request[4] ||
        report[5] != request[5]) return BatchReplyKind::Malformed;
    // The captured exact104 dispatcher does this. It is an ACK, not travel.
    if (std::equal(report + 9, report + 9 + count * 8, request.data() + 9))
        return BatchReplyKind::Echo;
    auto next = out;
    for (std::size_t i = 0; i < count; ++i) {
        const auto* p = report + 9 + i * 8;
        next[i] = {indices[i], p[0],
                   static_cast<std::uint16_t>(p[1] | (p[2] << 8)),
                   static_cast<std::uint16_t>(p[3] | (p[4] << 8))};
    }
    out = next;
    return BatchReplyKind::Candidate;
}

// Windows HID buffers include the report-ID byte. Official UI displays
// keyStroke/100 mm and maxStroke/10 mm, hence denominator maxStroke*10.
inline bool ParseTravel(const std::uint8_t* report, std::size_t bytes,
                        TravelSample& out) noexcept {
    if (!report || bytes != 65 || report[0] != 0 || report[1] != 0x55 ||
        report[2] != 0xfb || report[3] >= kFactoryHid.size()) return false;
    out = {};
    out.index = report[3];
    out.stroke = static_cast<std::uint16_t>(report[11] | (report[12] << 8));
    out.maximum = static_cast<std::uint16_t>(report[13] | (report[14] << 8));
    out.mapped = kFactoryHid[out.index] != 0;
    if (!out.maximum) {
        out.zeroMaximumPositive = out.stroke != 0;
        return true;
    }
    const std::uint32_t full = std::uint32_t(out.maximum) * 10u;
    out.aboveFull = out.stroke > full;
    const auto scaled = std::uint32_t(out.stroke) * 1000u / full;
    out.milli = static_cast<std::uint16_t>(scaled > 1000u ? 1000u : scaled);
    return true;
}

inline bool SelfTest() noexcept {
    unsigned mapped = 0;
    for (auto hid : kFactoryHid) {
        if (hid) ++mapped;
        if (hid && !halljoy::keycode::IsSupported(hid)) return false;
    }
    if (!ExactProduct(L"Alumix 104 Yotei Magnetic") ||
        ExactProduct(L"Alumix TKL Horizon") ||
        !ExactCollection(0xff68, 0x61, 65, 65) ||
        ExactCollection(0xff68, 0x61, 33, 33) ||
        ExactCollection(0xff68, 0x62, 65, 65) ||
        mapped != 103 || kFactoryHid[49] != 4 || kFactoryHid[69] != 5 ||
        kFactoryHid[85] != halljoy::keycode::kFn || kFactoryHid[120] != 0)
        return false;
    std::array<std::uint8_t,65> report{};
    report[1] = 0x55; report[2] = 0xfb; report[3] = 49;
    report[11] = 175; report[13] = 35; // 175 / (35*10) = 500 milli.
    TravelSample sample{};
    if (!ParseTravel(report.data(), report.size(), sample) || sample.milli != 500 ||
        !sample.mapped || sample.aboveFull) return false;
    report[11] = 0;
    if (!ParseTravel(report.data(), report.size(), sample) || sample.milli) return false;
    report[11] = 5; report[13] = 0;
    if (!ParseTravel(report.data(), report.size(), sample) ||
        !sample.zeroMaximumPositive || sample.milli) return false;
    report[11] = 0xff; report[12] = 0xff; report[13] = 35;
    if (!ParseTravel(report.data(), report.size(), sample) ||
        !sample.aboveFull || sample.milli != 1000) return false;
    return !ParseTravel(report.data(), 14, sample);
}
}
