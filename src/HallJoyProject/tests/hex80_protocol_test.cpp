#include "../HallJoy/hex80_protocol.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
std::array<std::uint8_t, hex80::kPayloadBytes> MakeTravelInfo(std::uint16_t maximum)
{
    std::array<std::uint8_t, hex80::kPayloadBytes> response{};
    response[0] = hex80::kGetValue;
    response[1] = hex80::kCustomCommand;
    response[2] = hex80::kTravelInfo;
    response[3] = static_cast<std::uint8_t>(maximum >> 8);
    response[4] = static_cast<std::uint8_t>(maximum & 0xFFu);
    return response;
}

std::array<std::uint8_t, hex80::kPayloadBytes> MakeChunk(
    std::uint16_t offset,
    std::uint8_t count,
    const std::array<std::uint16_t, hex80::kChunkSize>& travel)
{
    std::array<std::uint8_t, hex80::kPayloadBytes> response{};
    response[0] = hex80::kGetValue;
    response[1] = hex80::kCustomCommand;
    response[2] = hex80::kTravelBuffer;
    response[5] = static_cast<std::uint8_t>(offset >> 8);
    response[6] = static_cast<std::uint8_t>(offset & 0xFFu);
    response[7] = count;
    std::size_t cursor = 8;
    for (std::size_t index = 0; index < count; ++index, cursor += 5)
    {
        const std::uint16_t adc = static_cast<std::uint16_t>(0x1200u + index);
        response[cursor] = static_cast<std::uint8_t>(adc >> 8);
        response[cursor + 1] = static_cast<std::uint8_t>(adc & 0xFFu);
        response[cursor + 2] = static_cast<std::uint8_t>(travel[index] >> 8);
        response[cursor + 3] = static_cast<std::uint8_t>(travel[index] & 0xFFu);
        response[cursor + 4] = static_cast<std::uint8_t>(0x80u + index);
    }
    return response;
}
}

int main()
{
    using namespace hex80;

    static_assert(kTotalSlots == 104);
    static_assert(kChunkSize == 4);
    static_assert(kSlotToHid.size() == kTotalSlots);
    static_assert(MappedKeyCount() == 87);

    assert(IsKnownProductId(0x1176));
    assert(IsKnownProductId(0x1177));
    assert(IsKnownProductId(0x1250));
    assert(!IsKnownProductId(0x1109));

    assert(kSlotToHid[36] == 0x1A); // W
    assert(kSlotToHid[52] == 0x04); // A
    assert(kSlotToHid[53] == 0x16); // S
    assert(kSlotToHid[54] == 0x07); // D
    assert(kSlotToHid[90] == 0x2C); // Space
    assert(kSlotToHid[96] == 0x409); // Fn at physical row5/col11.
    assert(kSlotToHid[102] == 0 && kSlotToHid[103] == 0);

    assert(kSlotToHid[13] == 0 && kSlotToHid[14] == 0x46); // Mute gap, PrintScreen.
    assert(kSlotToHid[15] == 0x47 && kSlotToHid[16] == 0x48);
    assert(kSlotToHid[32] == 0x4A && kSlotToHid[33] == 0x4B); // Home/PageUp.
    assert(kSlotToHid[48] == 0x4C && kSlotToHid[49] == 0x4D && kSlotToHid[50] == 0x4E);
    assert(kSlotToHid[64] == 0 && kSlotToHid[65] == 0x28); // Enter's physical gap.
    assert(kSlotToHid[83] == 0x52 && kSlotToHid[82] == 0);
    assert(kSlotToHid[95] == 0xE7 && kSlotToHid[98] == 0xE4); // Right Win/Ctrl.
    assert(kSlotToHid[99] == 0x50 && kSlotToHid[100] == 0x51 && kSlotToHid[101] == 0x4F);
    assert(!IsFresh(0, 100) && !IsFresh(100, 99));
    assert(IsFresh(100, 600) && !IsFresh(100, 601));

    const auto finish = BuildCalibrationFinishPayload();
    assert(finish[0] == kSetValue && finish[1] == kCustomCommand && finish[2] == kCalibrationFinish);
    const auto infoRequest = BuildTravelInfoPayload();
    assert(infoRequest[0] == kGetValue && infoRequest[1] == kCustomCommand && infoRequest[2] == kTravelInfo);
    const auto chunkRequest = BuildTravelBufferPayload(100, 4);
    assert(chunkRequest[0] == kGetValue && chunkRequest[1] == kCustomCommand && chunkRequest[2] == kTravelBuffer);
    assert(chunkRequest[5] == 0 && chunkRequest[6] == 100 && chunkRequest[7] == 4);

    std::array<std::uint8_t,33> smallReport{};
    std::array<std::uint8_t,129> legacyReport{};
    assert(EncodeOutputReport(chunkRequest,smallReport.data(),smallReport.size()));
    assert(EncodeOutputReport(chunkRequest,legacyReport.data(),legacyReport.size()));
    assert(std::equal(smallReport.begin(),smallReport.end(),legacyReport.begin()));
    assert(smallReport[0]==0 && smallReport[1]==2 && smallReport[8]==4);
    assert(!EncodeOutputReport(chunkRequest,smallReport.data(),32));
    assert(!EncodeOutputReport(chunkRequest,nullptr,33));
    auto tooLong=chunkRequest;tooLong[32]=1;const auto savedReport=smallReport;
    assert(!EncodeOutputReport(tooLong,smallReport.data(),smallReport.size()));
    assert(smallReport==savedReport);
    assert(EncodeOutputReport(tooLong,legacyReport.data(),legacyReport.size()));

    assert(NormalizeTravelToMilli(0, 3300) == 0);
    assert(NormalizeTravelToMilli(8, 3300) == 0);
    assert(NormalizeTravelToMilli(3300, 3300) == 1000);
    assert(NormalizeTravelToMilli(4000, 3300) == 1000);
    assert(NormalizeTravelToMilli(1654, 3300) == 500);
    assert(NormalizeTravelToMilli(100, 8) == 0);

    auto info = MakeTravelInfo(3300);
    std::uint16_t travelMax = 0;
    assert(DecodeTravelInfo(info.data(), info.size(), travelMax));
    assert(travelMax == 3300);

    std::array<std::uint8_t, kPayloadBytes + 1> prefixed{};
    std::copy(info.begin(), info.end(), prefixed.begin() + 1);
    assert(DecodeTravelInfo(prefixed.data(), prefixed.size(), travelMax));
    assert(travelMax == 3300);

    info[3] = 0;
    info[4] = 64;
    assert(!DecodeTravelInfo(info.data(), info.size(), travelMax));

    const std::array<std::uint16_t, kChunkSize> values{{ 0, 8, 1654, 3300 }};
    auto chunk = MakeChunk(52, 4, values);
    std::array<TravelEntry, kChunkSize> entries{};
    std::size_t count = 0;
    assert(DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));
    assert(count == 4);
    assert(entries[0].slot == 52 && entries[0].hid == 0x04 && entries[0].milli == 0);
    assert(entries[1].slot == 53 && entries[1].hid == 0x16 && entries[1].milli == 0);
    assert(entries[2].slot == 54 && entries[2].hid == 0x07 && entries[2].milli == 500);
    assert(entries[3].slot == 55 && entries[3].hid == 0x09 && entries[3].milli == 1000);
    assert(entries[2].adc == 0x1202 && entries[2].status == 0x82);

    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 48, 4, 3300, entries, count));
    chunk[7] = 3;
    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));
    chunk = MakeChunk(52, 4, values);
    assert(!DecodeTravelChunk(chunk.data(), 20, 52, 4, 3300, entries, count));
    chunk[8 + 2] = 0xFF;
    chunk[8 + 3] = 0xFF;
    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));

    // A malformed last record must not expose any preceding partial records.
    chunk = MakeChunk(52, 4, values);
    chunk[8 + 3*5 + 2] = 0xFF; chunk[8 + 3*5 + 3] = 0xFF;
    assert(!DecodeTravelChunk(chunk.data(), chunk.size(), 52, 4, 3300, entries, count));
    assert(count == 0);
    for (const auto& entry : entries) assert(entry.hid == 0 && entry.milli == 0);
    unsigned mapped = 0;
    for (std::uint16_t offset=0; offset<kTotalSlots; offset+=kChunkSize) {
        const auto request=BuildTravelBufferPayload(offset,4);
        chunk=MakeChunk(offset,4,values);
        assert(MatchesRequest(request,chunk.data(),chunk.size()));
        assert(!MatchesRequest(BuildTravelBufferPayload((offset+4)%kTotalSlots,4),chunk.data(),chunk.size()));
        assert(!MatchesRequest(request,chunk.data(),8));
        std::array<std::uint8_t,kPayloadBytes+1> report{};
        std::copy(chunk.begin(),chunk.end(),report.begin()+1);
        assert(MatchesRequest(request,report.data(),report.size()));
        assert(DecodeTravelChunk(report.data(),report.size(),offset,4,3300,entries,count));
        for (const auto& entry : entries) {
            assert(entry.hid==kSlotToHid[entry.slot]);
            assert(entry.hid<kHidCount);
            mapped+=entry.hid!=0;
        }
    }
    assert(mapped==87);

    // MADLIONS MAD68 HE V2 Flagship: same frames, 5x15 matrix, fixed 0.01 mm
    // scale 330, never the Hex80 calibration SET.
    static_assert(MappedKeyCount(kMad68V2Model) == 68);
    static_assert(kMad68V2Model.slots == 75 && kMad68V2Model.fixedTravelMax == 330);
    static_assert(!kMad68V2Model.calibrationFinish && kHex80Model.calibrationFinish);
    static_assert(FindModel(0x1125) == &kMad68V2Model && FindModel(0x1176) == &kHex80Model);
    static_assert(FindModel(0x1109) == nullptr && FindModel(0x1021) == nullptr);
    assert(IsKnownProductId(0x1125));
    {
        // Every one of the 68 firmware keys is unique and mapped; Fn is 0x409.
        std::array<bool, kHidCount> seen{};
        unsigned keys = 0;
        for (std::size_t slot = 0; slot < kMad68V2Slots; ++slot) {
            const auto hid = kMad68V2SlotToHid[slot];
            if (!hid) continue;
            assert(hid < kHidCount && !seen[hid]);
            seen[hid] = true; ++keys;
        }
        assert(keys == 68 && seen[0x409] && seen[0x1a] && seen[0x4] && seen[0x16] && seen[0x7]);
        // Row/column addressing: slot = row * 15 + col (W = row 1 col 2, Space = row 4 col 6).
        assert(kMad68V2SlotToHid[1 * 15 + 2] == 0x1a && kMad68V2SlotToHid[4 * 15 + 6] == 0x2c);

        std::uint16_t offset = 0;
        unsigned seenKeys = 0;
        for (; offset < kMad68V2Slots; offset += kChunkSize) {
            const auto size = static_cast<std::uint8_t>(std::min<std::size_t>(kChunkSize, kMad68V2Slots - offset));
            chunk = MakeChunk(offset, size, {{165, 330, 400, 2}});
            assert(DecodeTravelChunk(kMad68V2Model, chunk.data(), chunk.size(), offset, size, 330, entries, count));
            assert(count == size);
            for (std::size_t i = 0; i < count; ++i) {
                assert(entries[i].hid == kMad68V2SlotToHid[offset + i]);
                seenKeys += entries[i].hid != 0;
            }
            assert(entries[0].milli == 495 && entries[1].milli == 1000); // round((165-3)*1000/(330-3))
            if (size > 2) assert(entries[2].milli == 1000);             // above the clamp
            if (size > 3) assert(entries[3].milli == 0);                // inside the deadzone
        }
        assert(seenKeys == 68);
        // The last firmware chunk is 72..74 (3 slots); slot 75+ is refused.
        chunk = MakeChunk(72, 4, values);
        assert(!DecodeTravelChunk(kMad68V2Model, chunk.data(), chunk.size(), 72, 4, 330, entries, count));
        chunk = MakeChunk(76, 1, values);
        assert(!DecodeTravelChunk(kMad68V2Model, chunk.data(), chunk.size(), 76, 1, 330, entries, count));
        // Hex80 decoding of the same frame still uses the 17-column Hex80 map.
        chunk = MakeChunk(16, 4, values);
        assert(DecodeTravelChunk(chunk.data(), chunk.size(), 16, 4, 3300, entries, count));
        assert(entries[0].hid == kSlotToHid[16]);
    }

    {
        // ATK keyboards on the Hex80 protocol (generated from the ATK hub).
        static_assert(kModels.size() == 2 + kAtkFamilyModels.size() && kAtkFamilyModels.size() == 15);
        const std::array<std::size_t, 15> keys{{ 61, 61, 64, 64, 68, 68, 68, 68, 68, 81, 80, 81, 80, 80, 67 }};
        std::array<const Model*, 0x10000> owner{};
        for (const auto* model : kModels)
            for (std::size_t i = 0; i < model->productCount; ++i) {
                assert(!owner[model->productIds[i]]); // no PID belongs to two models
                owner[model->productIds[i]] = model;
                assert(FindModel(model->productIds[i]) == model);
            }
        for (std::size_t m = 0; m < kAtkFamilyModels.size(); ++m) {
            const auto& model = *kAtkFamilyModels[m];
            assert(!model.calibrationFinish && model.productCount > 0);
            assert(MappedKeyCount(model) == keys[m]);
            std::array<bool, kHidCount> seen{};
            for (std::size_t slot = 0; slot < model.slots; ++slot) {
                const auto hid = model.slotToHid[slot];
                if (!hid) continue;
                assert(hid < kHidCount && !seen[hid]);
                seen[hid] = true;
            }
            assert(seen[0x1a] && seen[0x2c] && seen[0x409]); // W, Space and Fn everywhere
            // Every chunk, including a short last one, decodes against this model.
            for (std::uint16_t offset = 0; offset < model.slots; offset += kChunkSize) {
                const auto size = static_cast<std::uint8_t>(std::min<std::size_t>(kChunkSize, model.slots - offset));
                chunk = MakeChunk(offset, size, values);
                assert(DecodeTravelChunk(model, chunk.data(), chunk.size(), offset, size, 3300, entries, count));
                assert(count == size && entries[0].hid == model.slotToHid[offset]);
            }
        }
        // Slot = row * hub matrix cols: 5x14 EDGE 60, 5x15 RS6, 6x15 RS7 V2, 6x16 RS7.
        assert(kAtkEdge60Model.slots == 70 && kAtkEdge60SlotToHid[1 * 14 + 2] == 0x1a);
        assert(kAtkRs6Model.slots == 75 && kAtkRs6SlotToHid[1 * 15 + 2] == 0x1a && kAtkRs6SlotToHid[4 * 15 + 10] == 0x409);
        assert(kAtkRs7v2Model.slots == 90 && kAtkRs7v2SlotToHid[2 * 15 + 2] == 0x1a && kAtkRs7v2SlotToHid[0] == 0x29);
        assert(kAtkRs7Model.slots == 96 && kAtkRs7SlotToHid[2 * 16 + 2] == 0x1a && kAtkRs7SlotToHid[0 * 16 + 15] == 0x4c);
        // DUCKBREAD boards report 0.01 mm and have no 02 96 24: fixed scale.
        static_assert(kAtkRs7Model.fixedTravelMax == 340 && kAtkRs7Model.rawDeadzone == 3);
        static_assert(kAtk68v2proModel.fixedTravelMax == 330 && kAtkRs6Model.fixedTravelMax == 0);
        static_assert(FindModel(0x109C) == &kAtkRs6Model && FindModel(0x1044) == &kAtkRs7Model);
        static_assert(FindModel(0x105E) == nullptr); // ATK68 V2 (ARBITER) is a different protocol
    }

    std::cout << "hex80 protocol tests passed: Hex80 104 slots/87 keys, MAD68 HE V2 75 slots/68 keys, "
                 "15 ATK family models\n";
    return 0;
}
