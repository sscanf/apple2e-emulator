#include "disk2.h"
#include "state.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace apple2e {

namespace {

constexpr int kSectors = 16;
constexpr size_t kSectorBytes = 256;
constexpr size_t kTrackImageBytes = kSectors * kSectorBytes;
constexpr size_t kSectorImageSize = DiskImage::kTracks * kTrackImageBytes;      // 143360
constexpr size_t kNibbleImageSize = DiskImage::kTracks * DiskImage::kTrackBytes;  // 232960
constexpr uint8_t kVolume = 254;
constexpr uint64_t kSpinDownCycles = 1020484;  // ~1 second

// Physical sector -> sector index within the image's track
constexpr uint8_t kDosOrder[kSectors] = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
constexpr uint8_t kProDosOrder[kSectors] = {0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15};

// 6-and-2 "disk bytes": the 64 valid nibbles for 6-bit values
constexpr uint8_t kEncode62[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6, 0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

constexpr std::array<int8_t, 256> makeDecodeTable() {
    std::array<int8_t, 256> table{};
    for (auto& v : table) v = -1;
    for (int i = 0; i < 64; i++) table[kEncode62[i]] = static_cast<int8_t>(i);
    return table;
}
constexpr auto kDecode62 = makeDecodeTable();

// Low two bits of a byte, swapped, as stored in the 6-and-2 auxiliary buffer
uint8_t swap2(uint8_t v) { return ((v & 0x01) << 1) | ((v & 0x02) >> 1); }

std::string lowerExtension(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

// ProDOS volume directory key block: no previous block, next block 3, storage type $F
bool hasProDosDirectory(const std::vector<uint8_t>& image, size_t offset) {
    return image[offset] == 0 && image[offset + 1] == 0 && image[offset + 2] == 3 &&
           image[offset + 3] == 0 && (image[offset + 4] >> 4) == 0x0F;
}

} // namespace

// ============================================================
// DiskImage
// ============================================================

std::string DiskImage::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "Cannot open " + path;
    std::vector<uint8_t> image((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    m_tracks.assign(kTracks, {});
    std::string ext = lowerExtension(path);

    if (image.size() == kNibbleImageSize) {
        m_format = Format::Nibble;
        for (int t = 0; t < kTracks; t++) {
            std::copy_n(image.begin() + t * kTrackBytes, kTrackBytes, m_tracks[t].begin());
        }
    } else if (image.size() == kSectorImageSize) {
        // .dsk files are usually DOS-ordered, but ProDOS-ordered ones exist too:
        // in that case the volume directory (block 2) sits at $400 of the file
        bool proDos = ext == ".po" ||
                      (ext != ".do" && hasProDosDirectory(image, 0x400) && !hasProDosDirectory(image, 0xB00));
        m_format = proDos ? Format::ProDosOrder : Format::DosOrder;
        for (int t = 0; t < kTracks; t++) nibblizeTrack(t, &image[t * kTrackImageBytes]);
    } else {
        m_tracks.clear();
        return "Unsupported disk image size (" + std::to_string(image.size()) +
               " bytes); expected a 140 KB .dsk/.do/.po or a .nib";
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    auto perms = fs::status(path, ec).permissions();
    m_writeProtected = ec || (perms & fs::perms::owner_write) == fs::perms::none;

    m_path = path;
    m_dirty = false;
    return {};
}

std::string DiskImage::save() {
    if (!m_dirty || m_path.empty()) return {};

    std::vector<uint8_t> image;
    if (m_format == Format::Nibble) {
        for (const auto& track : m_tracks) image.insert(image.end(), track.begin(), track.end());
    } else {
        image.resize(kSectorImageSize);
        for (int t = 0; t < kTracks; t++) {
            if (!denibblizeTrack(t, &image[t * kTrackImageBytes])) {
                return "Track " + std::to_string(t) + " of " + m_path +
                       " could not be decoded; changes were not saved";
            }
        }
    }

    std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
    if (!file.write(reinterpret_cast<const char*>(image.data()), image.size())) {
        return "Cannot write " + m_path;
    }
    m_dirty = false;
    return {};
}

void DiskImage::writeNibble(int track, size_t pos, uint8_t val) {
    if (m_writeProtected || m_tracks.empty()) return;
    m_tracks[track][pos % kTrackBytes] = val;
    m_dirty = true;
}

// Standard DOS 3.3 track layout: gap, then per sector an address field
// (D5 AA 96, 4-and-4 volume/track/sector/checksum, DE AA EB), a gap, a data
// field (D5 AA AD, 342 6-and-2 nibbles + checksum, DE AA EB) and another gap
void DiskImage::nibblizeTrack(int track, const uint8_t* data) {
    const uint8_t* order = m_format == Format::ProDosOrder ? kProDosOrder : kDosOrder;
    auto& out = m_tracks[track];
    size_t pos = 0;
    auto put = [&](uint8_t b) { out[pos++] = b; };
    auto put44 = [&](uint8_t v) { put((v >> 1) | 0xAA); put(v | 0xAA); };
    auto gap = [&](int n) { for (int i = 0; i < n; i++) put(0xFF); };

    gap(48);
    for (int s = 0; s < kSectors; s++) {
        const uint8_t* sector = data + order[s] * kSectorBytes;

        put(0xD5); put(0xAA); put(0x96);
        put44(kVolume);
        put44(static_cast<uint8_t>(track));
        put44(static_cast<uint8_t>(s));
        put44(static_cast<uint8_t>(kVolume ^ track ^ s));
        put(0xDE); put(0xAA); put(0xEB);
        gap(6);

        // 86 bytes holding the low two bits of every data byte, then the top six bits
        uint8_t buf[342];
        for (int i = 0; i < 86; i++) {
            uint8_t v = swap2(sector[i]) | (swap2(sector[i + 86]) << 2);
            if (i + 172 < 256) v |= swap2(sector[i + 172]) << 4;
            buf[i] = v;
        }
        for (int i = 0; i < 256; i++) buf[86 + i] = sector[i] >> 2;

        put(0xD5); put(0xAA); put(0xAD);
        uint8_t prev = 0;
        for (uint8_t v : buf) {
            put(kEncode62[v ^ prev]);
            prev = v;
        }
        put(kEncode62[prev]);
        put(0xDE); put(0xAA); put(0xEB);
        gap(27);
    }
    while (pos < out.size()) put(0xFF);
}

bool DiskImage::denibblizeTrack(int track, uint8_t* data) const {
    const uint8_t* order = m_format == Format::ProDosOrder ? kProDosOrder : kDosOrder;
    const auto& in = m_tracks[track];
    auto nib = [&](size_t i) { return in[i % kTrackBytes]; };
    auto get44 = [&](size_t i) -> uint8_t { return ((nib(i) << 1) | 0x01) & nib(i + 1); };

    bool found[kSectors] = {};
    for (size_t i = 0; i < kTrackBytes; i++) {
        if (nib(i) != 0xD5 || nib(i + 1) != 0xAA || nib(i + 2) != 0x96) continue;
        uint8_t sector = get44(i + 7);
        if (sector >= kSectors) continue;

        // Data field follows within a few nibbles of the address field
        size_t start = 0;
        for (size_t j = i + 11; j < i + 11 + 64; j++) {
            if (nib(j) == 0xD5 && nib(j + 1) == 0xAA && nib(j + 2) == 0xAD) {
                start = j + 3;
                break;
            }
        }
        if (!start) continue;

        uint8_t buf[342];
        uint8_t prev = 0;
        bool valid = true;
        for (int k = 0; k < 342 && valid; k++) {
            int8_t v = kDecode62[nib(start + k)];
            if (v < 0) valid = false;
            prev ^= static_cast<uint8_t>(v);
            buf[k] = prev;
        }
        if (!valid || kDecode62[nib(start + 342)] != prev) continue;

        uint8_t* out = data + order[sector] * kSectorBytes;
        for (int k = 0; k < 256; k++) {
            uint8_t low = (buf[k % 86] >> (2 * (k / 86))) & 0x03;
            out[k] = static_cast<uint8_t>((buf[86 + k] << 2) | swap2(low));
        }
        found[sector] = true;
    }

    return std::all_of(std::begin(found), std::end(found), [](bool f) { return f; });
}

// ============================================================
// Disk2Controller
// ============================================================

bool Disk2Controller::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() != m_rom.size()) {
        std::cerr << "Disk II ROM " << path << " must be 256 bytes" << std::endl;
        return false;
    }
    std::copy(data.begin(), data.end(), m_rom.begin());
    std::cout << "Disk II ROM loaded from " << path << std::endl;
    return true;
}

std::string Disk2Controller::insert(int drive, const std::string& path) {
    std::string saveError = eject(drive);
    DiskImage image;
    std::string error = image.load(path);
    if (!error.empty()) return error;
    m_drives[drive].disk = std::move(image);
    return saveError;
}

std::string Disk2Controller::eject(int drive) {
    std::string error = m_drives[drive].disk.save();
    m_drives[drive].disk = DiskImage{};
    return error;
}

void Disk2Controller::flush() {
    for (auto& drive : m_drives) {
        std::string error = drive.disk.save();
        if (!error.empty()) std::cerr << error << std::endl;
    }
}

bool Disk2Controller::spinning() const {
    return m_motorOn || (m_motorOffCycle && m_cycles - m_motorOffCycle < kSpinDownCycles);
}

// Each phase magnet pulls the head half a track; phase n is aligned with
// half tracks n, n+4, n+8... so energising the next/previous phase steps in/out
void Disk2Controller::stepPhase(int phase, bool on) {
    if (on) {
        m_phases |= 1 << phase;
    } else {
        m_phases &= ~(1 << phase);
        return;
    }

    Drive& drive = current();
    int diff = (phase - drive.halfTrack) & 3;
    int target = drive.halfTrack;
    if (diff == 1) target++;
    if (diff == 3) target--;
    if (target == drive.halfTrack) return;

    int clamped = std::clamp(target, 0, (DiskImage::kTracks - 1) * 2);
    if (m_onHeadEvent) m_onHeadEvent(clamped == target ? HeadEvent::Step : HeadEvent::Bump, m_cycles);
    drive.halfTrack = clamped;
}

// $C0n0-$C0nF: 0-7 phases off/on, 8/9 motor, A/B drive select,
// C/D Q6 (shift/load), E/F Q7 (read/write). Even addresses access the latch.
uint8_t Disk2Controller::io(uint8_t reg, bool isWrite, uint8_t val) {
    switch (reg) {
        case 0x0: case 0x1: case 0x2: case 0x3:
        case 0x4: case 0x5: case 0x6: case 0x7:
            stepPhase(reg >> 1, reg & 1);
            break;
        case 0x8:
            if (m_motorOn) m_motorOffCycle = m_cycles;
            m_motorOn = false;
            break;
        case 0x9: m_motorOn = true; break;
        case 0xA: m_selected = 0; break;
        case 0xB: m_selected = 1; break;
        case 0xC: m_q6 = false; break;
        case 0xD: m_q6 = true; break;
        case 0xE: m_q7 = false; break;
        case 0xF: m_q7 = true; break;
    }

    if (isWrite && (reg & 1)) m_latch = val;
    if (reg & 1) return 0;

    Drive& drive = current();
    DiskImage& disk = drive.disk;
    int track = drive.halfTrack / 2;

    if (m_q7) {
        // Write mode: each shift (Q6L) access stores the latch on the disk
        if (!m_q6 && spinning() && disk.loaded()) {
            disk.writeNibble(track, drive.position, m_latch);
            drive.position = (drive.position + 1) % DiskImage::kTrackBytes;
        }
    } else if (m_q6) {
        // Sense write protect in bit 7
        m_latch = disk.writeProtected() ? 0x80 : 0x00;
    } else if (spinning() && disk.loaded()) {
        // A real drive delivers a nibble every 32 cycles while the latch keeps
        // shifting in between. Alternate a complete nibble with a partial one
        // (bit 7 clear): read loops just wait for bit 7, and DOS's "is the
        // disk spinning?" check sees the value change even in sync gaps.
        m_nibbleReady = !m_nibbleReady;
        if (m_nibbleReady) {
            m_latch = disk.readNibble(track, drive.position);
            drive.position = (drive.position + 1) % DiskImage::kTrackBytes;
        } else {
            m_latch &= 0x7F;
        }
    }
    return m_latch;
}

// ============================================================
// Save states
// ============================================================

void DiskImage::saveState(StateWriter& w) const {
    w.putString(m_path);
    w.put(m_format);
    w.put(m_writeProtected);
    w.put(m_dirty);
    w.put(static_cast<uint32_t>(m_tracks.size()));
    for (const auto& track : m_tracks) w.putBytes(track.data(), track.size());
}

void DiskImage::loadState(StateReader& r) {
    m_path = r.getString();
    r.get(m_format);
    r.get(m_writeProtected);
    r.get(m_dirty);
    auto tracks = r.get<uint32_t>();
    if (tracks != 0 && tracks != kTracks) tracks = 0;  // corrupt; reader ok() catches the rest
    m_tracks.assign(tracks, {});
    for (auto& track : m_tracks) r.getBytes(track.data(), track.size());
}

void Disk2Controller::saveState(StateWriter& w) const {
    w.put(m_selected); w.put(m_motorOn); w.put(m_motorOffCycle);
    w.put(m_q6); w.put(m_q7); w.put(m_phases); w.put(m_latch); w.put(m_nibbleReady);
    for (const auto& drive : m_drives) {
        w.put(drive.halfTrack);
        w.put(drive.position);
        drive.disk.saveState(w);
    }
}

void Disk2Controller::loadState(StateReader& r) {
    r.get(m_selected); r.get(m_motorOn); r.get(m_motorOffCycle);
    r.get(m_q6); r.get(m_q7); r.get(m_phases); r.get(m_latch); r.get(m_nibbleReady);
    for (auto& drive : m_drives) {
        r.get(drive.halfTrack);
        r.get(drive.position);
        drive.disk.loadState(r);
    }
}

} // namespace apple2e
