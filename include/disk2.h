#pragma once

#include "card.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace apple2e {

class StateReader;
class StateWriter;

// 5.25" floppy held as a nibble stream per track, as the drive head sees it.
// Sector images (.dsk/.do/.po) are 6-and-2 encoded on load and decoded back
// when saved; .nib images are used as-is.
class DiskImage {
public:
    static constexpr int kTracks = 35;
    static constexpr int kTrackBytes = 6656;  // nibbles per track

    // Returns an error message on failure, empty on success
    std::string load(const std::string& path);
    // Writes the image back if it was modified. Returns an error message on failure.
    std::string save();

    bool loaded() const { return !m_path.empty(); }
    const std::string& path() const { return m_path; }
    bool writeProtected() const { return m_writeProtected; }
    bool dirty() const { return m_dirty; }

    uint8_t readNibble(int track, size_t pos) const { return m_tracks[track][pos % kTrackBytes]; }
    void writeNibble(int track, size_t pos, uint8_t val);

    // Save states (see state.h)
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

private:
    enum class Format { DosOrder, ProDosOrder, Nibble };

    void nibblizeTrack(int track, const uint8_t* data);
    bool denibblizeTrack(int track, uint8_t* data) const;

    std::string m_path;
    Format m_format = Format::DosOrder;
    bool m_writeProtected = false;
    bool m_dirty = false;
    std::vector<std::array<uint8_t, kTrackBytes>> m_tracks;
};

// Disk II controller card with two drives (normally in slot 6)
class Disk2Controller : public Card {
public:
    static constexpr int kDrives = 2;

    // `cycles` is the CPU cycle counter, used for the motor spin-down delay
    explicit Disk2Controller(const uint64_t& cycles) : m_cycles(cycles) {}

    // 256-byte P5 boot ROM (341-0027)
    bool loadRom(const std::string& path);

    uint8_t io(uint8_t reg, bool isWrite, uint8_t val) override;
    uint8_t rom(uint8_t offset) const override { return m_rom[offset]; }

    // Returns an error message on failure, empty on success
    std::string insert(int drive, const std::string& path);
    // Ejects (saving changes first); returns an error message if the save failed
    std::string eject(int drive);
    // Save all modified disks (e.g. on exit)
    void flush();

    // Save states (see state.h); disks are stored with their full contents
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

    const DiskImage& disk(int drive) const { return m_drives[drive].disk; }
    // Drive activity light: motor spinning and drive selected
    bool active(int drive) const { return spinning() && m_selected == drive; }

    // The drive keeps spinning for about a second after the motor is switched
    // off; DOS relies on this to skip the spin-up wait between operations
    bool spinning() const;

    // Mechanical events for sound effects: head moved, or pushed against the
    // track 0 stop. Called with the CPU cycle at which it happened.
    enum class HeadEvent { Step, Bump };
    void setHeadEventCallback(std::function<void(HeadEvent, uint64_t)> cb) { m_onHeadEvent = std::move(cb); }

private:
    struct Drive {
        DiskImage disk;
        int halfTrack = 0;     // head position in half tracks (0-69)
        size_t position = 0;   // nibble index within the track
    };

    void stepPhase(int phase, bool on);
    Drive& current() { return m_drives[m_selected]; }

    const uint64_t& m_cycles;
    uint64_t m_motorOffCycle = 0;
    std::function<void(HeadEvent, uint64_t)> m_onHeadEvent;

    std::array<uint8_t, 256> m_rom{};
    std::array<Drive, kDrives> m_drives;
    int m_selected = 0;
    bool m_motorOn = false;
    bool m_q6 = false;  // load / sense write protect
    bool m_q7 = false;  // write mode
    uint8_t m_phases = 0;
    uint8_t m_latch = 0;
    bool m_nibbleReady = false;
};

} // namespace apple2e
