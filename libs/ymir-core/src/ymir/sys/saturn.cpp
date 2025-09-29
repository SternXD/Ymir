#include <ymir/sys/saturn.hpp>

#include <ymir/db/game_db.hpp>

#include <ymir/util/dev_log.hpp>

#include <bit>
#include <cassert>

namespace ymir {

namespace static_config {

    // Reduces timeslices to the minimum possible -- one MSH2 instruction at a time.
    // Maximizes component synchronization at a massive cost to performance.
    static constexpr bool max_timing_granularity = false;

} // namespace static_config

namespace grp {

    // -----------------------------------------------------------------------------
    // Dev log groups

    // Hierarchy:
    //
    // system
    // bus
    // media

    struct system {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "System";
    };

    struct bus {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Bus";
    };

    struct media {
        static constexpr bool enabled = true;
        static constexpr devlog::Level level = devlog::level::debug;
        static constexpr std::string_view name = "Media";
    };

} // namespace grp

Saturn::Saturn()
    : masterSH2(m_scheduler, mainBus, true, m_systemFeatures)
    , slaveSH2(m_scheduler, mainBus, false, m_systemFeatures)
    , SCU(m_scheduler, mainBus)
    , VDP(m_scheduler, configuration)
    , SMPC(m_scheduler, smpcOps, configuration.rtc)
    , SCSP(m_scheduler, configuration.audio)
    , CDBlock(m_scheduler, m_disc, m_fs, configuration.cdblock)
    , SH1(SH1Bus)
    , CDDrive(m_scheduler, m_disc, m_fs, configuration.cdblock) {

    mainBus.MapNormal(
        0x000'0000, 0x7FF'FFFF, nullptr,
        [](uint32 address, void *) -> uint8 {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, void *) -> uint16 {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, void *) -> uint32 {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus read from {:07X}", address);
            return 0;
        },
        [](uint32 address, uint8 value, void *) {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus write to {:07X} = {:02X}", address, value);
        },
        [](uint32 address, uint16 value, void *) {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus write to {:07X} = {:04X}", address, value);
        },
        [](uint32 address, uint32 value, void *) {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus write to {:07X} = {:07X}", address, value);
        });

    SH1Bus.MapNormal(
        0x000'0000, 0xFFF'FFFF, nullptr,
        [](uint32 address, void *) -> uint8 {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus read from {:07X}\n", address);
            return 0;
        },
        [](uint32 address, void *) -> uint16 {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus read from {:07X}\n", address);
            return 0;
        },
        [](uint32 address, void *) -> uint32 {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus read from {:07X}\n", address);
            return 0;
        },
        [](uint32 address, uint8 value, void *) {
            devlog::debug<grp::bus>("Unhandled 8-bit main bus write to {:07X} = {:02X}\n", address, value);
        },
        [](uint32 address, uint16 value, void *) {
            devlog::debug<grp::bus>("Unhandled 16-bit main bus write to {:07X} = {:04X}\n", address, value);
        },
        [](uint32 address, uint32 value, void *) {
            devlog::debug<grp::bus>("Unhandled 32-bit main bus write to {:07X} = {:08X}\n", address, value);
        });

    SH1Bus.MapArray(0x1000000, 0x1FFFFFF, CDBlockDRAM, true);
    SH1Bus.MapArray(0x9000000, 0x9FFFFFF, CDBlockDRAM, true);

    masterSH2.MapCallbacks(SCU.CbAckExtIntr);
    // Slave SH2 IVECF# pin is not connected, so the external interrupt vector fetch callback shouldn't be mapped
    SCU.MapCallbacks(masterSH2.CbExtIntr, slaveSH2.CbExtIntr);
    VDP.MapCallbacks(SCU.CbHBlankStateChange, SCU.CbVBlankStateChange, SCU.CbTriggerSpriteDrawEnd,
                     SMPC.CbTriggerOptimizedINTBACKRead, SMPC.CbTriggerVBlankIN);
    SMPC.MapCallbacks(SCU.CbTriggerSystemManager, SCU.CbTriggerPad);
    SCSP.MapCallbacks(SCU.CbTriggerSoundRequest);
    SH1.SetSCI0Callbacks(CDDrive.CbSerialRx, CDDrive.CbSerialTx);
    CDDrive.MapCallbacks(SH1.CbSetCOMSYNCn, SH1.CbSetCOMREQn, SH1.CbCDBDataSector, SCSP.CbCDDASector,
                         YGR.CbSectorTransferDone);
    YGR.MapCallbacks(SH1.CbAssertIRQ6, SH1.CbAssertIRQ7, SH1.CbSetDREQ0n, SH1.CbSetDREQ1n, SH1.CbStepDMAC1,
                     SCU.CbTriggerExtIntr0);
    CDBlock.MapCallbacks(SCU.CbTriggerExtIntr0, SCSP.CbCDDASector);

    m_system.AddClockSpeedChangeCallback(SCSP.CbClockSpeedChange);
    m_system.AddClockSpeedChangeCallback(SMPC.CbClockSpeedChange);
    m_system.AddClockSpeedChangeCallback(CDDrive.CbClockSpeedChange);
    m_system.AddClockSpeedChangeCallback(CDBlock.CbClockSpeedChange);

    masterSH2.UseDebugBreakManager(&m_debugBreakMgr);
    slaveSH2.UseDebugBreakManager(&m_debugBreakMgr);

    mem.MapMemory(mainBus);
    masterSH2.MapMemory(mainBus);
    slaveSH2.MapMemory(mainBus);
    SCU.MapMemory(mainBus);
    VDP.MapMemory(mainBus);
    SMPC.MapMemory(mainBus);
    SCSP.MapMemory(mainBus);
    if (m_cdblockLLE) {
        YGR.MapMemory(mainBus);
    } else {
        CDBlock.MapMemory(mainBus);
    }
    YGR.MapMemory(SH1Bus);

    // These timings avoid issues with some games:
    // - Virtua Fighter 2 -- sound effects go missing if the SH-2 runs too fast
    // - Resident Evil -- can't go past title screen if timings are too slow
    // CD block area timings help with BIOS CD player track switching

    mainBus.SetAccessCycles(0x000'0000, 0x7FF'FFFF, 4, 2); // Default timings for all regions

    mainBus.SetAccessCycles(0x000'0000, 0x00F'FFFF, 2, 2);   // IPL/BIOS ROM
    mainBus.SetAccessCycles(0x018'0000, 0x01F'FFFF, 2, 2);   // Internal Backup RAM
    mainBus.SetAccessCycles(0x020'0000, 0x02F'FFFF, 2, 2);   // Low Work RAM
    mainBus.SetAccessCycles(0x010'0000, 0x017'FFFF, 4, 2);   // SMPC registers
    mainBus.SetAccessCycles(0x100'0000, 0x1FF'FFFF, 4, 2);   // MINIT/SINIT area
    mainBus.SetAccessCycles(0x200'0000, 0x4FF'FFFF, 2, 2);   // SCU A-Bus CS0/CS1 area (TODO: variable timings)
    mainBus.SetAccessCycles(0x500'0000, 0x57F'FFFF, 8, 2);   // SCU A-Bus dummy area
    mainBus.SetAccessCycles(0x580'0000, 0x58F'FFFF, 40, 40); // SCU A-Bus CS2 area (CD block, Netlink)
    mainBus.SetAccessCycles(0x5A0'0000, 0x5BF'FFFF, 40, 2);  // SCSP RAM, registers
    mainBus.SetAccessCycles(0x5C0'0000, 0x5C7'FFFF, 22, 2);  // VDP1 VRAM (TODO: VDP1 drawing contention)
    mainBus.SetAccessCycles(0x5C8'0000, 0x5CF'FFFF, 22, 2);  // VDP1 FB (TODO: variable timings)
    mainBus.SetAccessCycles(0x5D0'0000, 0x5D7'FFFF, 14, 2);  // VDP1 registers
    mainBus.SetAccessCycles(0x5E0'0000, 0x5FB'FFFF, 20, 2);  // VDP2 VRAM, CRAM, registers
    mainBus.SetAccessCycles(0x5FE'0000, 0x5FE'FFFF, 4, 2);   // SCU registers (TODO: delay on some registers)
    mainBus.SetAccessCycles(0x600'0000, 0x7FF'FFFF, 2, 2);   // High Work RAM

    // The timings below pass misctest, but are too slow in practice

    // mainBus.SetAccessCycles(0x000'0000, 0x7FF'FFFF, 4, 4); // Default timings for all regions

    // mainBus.SetAccessCycles(0x000'0000, 0x00F'FFFF, 9, 9);   // IPL/BIOS ROM
    // mainBus.SetAccessCycles(0x018'0000, 0x01F'FFFF, 9, 9);   // Internal Backup RAM
    // mainBus.SetAccessCycles(0x020'0000, 0x02F'FFFF, 8, 8);   // Low Work RAM
    // mainBus.SetAccessCycles(0x010'0000, 0x017'FFFF, 9, 9);   // SMPC registers
    // mainBus.SetAccessCycles(0x100'0000, 0x1FF'FFFF, 9, 9);   // MINIT/SINIT area
    // mainBus.SetAccessCycles(0x200'0000, 0x4FF'FFFF, 4, 4);   // SCU A-Bus CS0/CS1 area (TODO: variable timings)
    // mainBus.SetAccessCycles(0x500'0000, 0x57F'FFFF, 16, 16); // SCU A-Bus dummy area
    // mainBus.SetAccessCycles(0x580'0000, 0x58F'FFFF, 8, 8);   // SCU A-Bus CS2 area (CD block, Netlink)
    // mainBus.SetAccessCycles(0x5A0'0000, 0x5BF'FFFF, 47, 47); // SCSP RAM, registers
    // mainBus.SetAccessCycles(0x5C0'0000, 0x5C7'FFFF, 45, 45); // VDP1 VRAM (TODO: VDP1 drawing contention)
    // mainBus.SetAccessCycles(0x5C8'0000, 0x5CF'FFFF, 45, 45); // VDP1 FB (TODO: variable timings)
    // mainBus.SetAccessCycles(0x5D0'0000, 0x5D7'FFFF, 29, 29); // VDP1 registers
    // mainBus.SetAccessCycles(0x5E0'0000, 0x5FB'FFFF, 40, 40); // VDP2 VRAM, CRAM, registers
    // mainBus.SetAccessCycles(0x5FE'0000, 0x5FE'FFFF, 8, 8);   // SCU registers (TODO: delay on some registers)
    // mainBus.SetAccessCycles(0x600'0000, 0x7FF'FFFF, 8, 8);   // High Work RAM

    m_systemFeatures.enableDebugTracing = false;
    m_systemFeatures.emulateSH2Cache = false;
    UpdateFunctionPointers();

    configuration.system.preferredRegionOrder.Observe(
        [&](const std::vector<core::config::sys::Region> &regions) { UpdatePreferredRegionOrder(regions); });
    configuration.system.emulateSH2Cache.Observe([&](bool enabled) { UpdateSH2CacheEmulation(enabled); });
    configuration.system.videoStandard.Observe(
        [&](core::config::sys::VideoStandard videoStandard) { UpdateVideoStandard(videoStandard); });
    configuration.cdblock.useLLE.Observe([&](bool enabled) { SetCDBlockLLE(enabled); });

    Reset(true);
}

void Saturn::Reset(bool hard) {
    m_system.clockSpeed = sys::ClockSpeed::_320;
    m_system.UpdateClockRatios();

    if (hard) {
        m_scheduler.Reset();
    }

    masterSH2.Reset(hard);
    slaveSH2.Reset(hard);
    slaveSH2Enabled = false;
    m_msh2SpilloverCycles = 0;
    m_ssh2SpilloverCycles = 0;
    m_sh1SpilloverCycles = 0;
    m_sh1FracCycles = 0;

    SCU.Reset(hard);
    VDP.Reset(hard);
    SMPC.Reset(hard);
    SCSP.Reset(hard);
    if (m_cdblockLLE) {
        SH1.Reset(hard);
        if (hard) {
            YGR.Reset();
        }
        CDDrive.Reset();
    } else {
        CDBlock.Reset(hard);
    }
}

void Saturn::FactoryReset() {
    SMPC.FactoryReset();
    Reset(true);
}

sys::ClockSpeed Saturn::GetClockSpeed() const noexcept {
    return m_system.clockSpeed;
}

void Saturn::SetClockSpeed(sys::ClockSpeed clockSpeed) {
    m_system.clockSpeed = clockSpeed;
    m_system.UpdateClockRatios();
}

const sys::ClockRatios &Saturn::GetClockRatios() const noexcept {
    return m_system.GetClockRatios();
}

void Saturn::LoadIPL(std::span<uint8, sys::kIPLSize> ipl) {
    mem.LoadIPL(ipl);
}

void Saturn::LoadCDBlockROM(std::span<uint8, sh1::kROMSize> rom) {
    SH1.LoadROM(rom);
}

void Saturn::LoadInternalBackupMemoryImage(std::filesystem::path path, std::error_code &error) {
    mem.LoadInternalBackupMemoryImage(path, error);
}

XXH128Hash Saturn::GetIPLHash() const noexcept {
    return mem.GetIPLHash();
}

const media::Disc &Saturn::GetDisc() const noexcept {
    return m_disc;
}

XXH128Hash Saturn::GetDiscHash() const noexcept {
    return m_fs.GetHash();
}

void Saturn::LoadDisc(media::Disc &&disc) {
    // Configure area code based on compatible area codes from the disc
    AutodetectRegion(disc.header.compatAreaCode);
    m_disc.Swap(std::move(disc));

    // Try building filesystem structure
    if (m_fs.Read(m_disc)) {
        devlog::info<grp::media>("Filesystem built successfully");
    } else {
        devlog::warn<grp::media>("Failed to build filesystem");
    }

    if (m_cdblockLLE) {
        CDDrive.OnDiscLoaded();
    } else {
        CDBlock.OnDiscLoaded();
    }

    // Apply game-specific settings if needed
    const db::GameInfo *info = db::GetGameInfo(m_disc.header.productNumber, m_fs.GetHash());
    ForceSH2CacheEmulation(info && info->sh2Cache);
}

void Saturn::EjectDisc() {
    if (!m_disc.sessions.empty()) {
        m_disc = {};
        m_fs.Clear();
        if (m_cdblockLLE) {
            CDDrive.OnDiscEjected();
        } else {
            CDBlock.OnDiscEjected();
        }
    }
}

void Saturn::OpenTray() {
    if (m_cdblockLLE) {
        CDDrive.OpenTray();
    } else {
        CDBlock.OpenTray();
    }
}

void Saturn::CloseTray() {
    if (m_cdblockLLE) {
        CDDrive.CloseTray();
    } else {
        CDBlock.CloseTray();
    }
}

bool Saturn::IsTrayOpen() const noexcept {
    if (m_cdblockLLE) {
        return CDDrive.IsTrayOpen();
    } else {
        return CDBlock.IsTrayOpen();
    }
}

void Saturn::UsePreferredRegion() {
    if (m_preferredRegionOrder.empty()) {
        return;
    }

    // Pick the first available preferred region
    const uint8 areaCode = std::countr_zero(static_cast<uint16>(m_preferredRegionOrder.front()));

    // Apply configuration and hard reset system if changed
    const uint8 currAreaCode = SMPC.GetAreaCode();
    SMPC.SetAreaCode(areaCode);
    if (areaCode != currAreaCode) {
        Reset(true);
    }
}

void Saturn::AutodetectRegion(media::AreaCode areaCodes) {
    if (!configuration.system.autodetectRegion) {
        return;
    }
    if (areaCodes == media::AreaCode::None) {
        return;
    }

    const uint8 currAreaCode = SMPC.GetAreaCode();

    // The area code enum is a bitmap where each bit corresponds to an SMPC area code
    const auto areaCodeVal = static_cast<uint16>(areaCodes);

    // Pick from the preferred list if possible or use the first one found
    uint8 selectedAreaCode = std::countr_zero<uint16>(areaCodeVal & -areaCodeVal);
    for (auto areaCode : m_preferredRegionOrder) {
        if (BitmaskEnum(areaCodes).AnyOf(areaCode)) {
            selectedAreaCode = std::countr_zero(static_cast<uint16>(areaCode));
            break;
        }
    }

    // Apply configuration and hard reset system if changed
    SMPC.SetAreaCode(selectedAreaCode);

    // Also change PAL/NTSC setting accordingly
    switch (selectedAreaCode) {
    case 0x1: [[fallthrough]];
    case 0x2: [[fallthrough]];
    case 0x4: [[fallthrough]];
    case 0x5: [[fallthrough]];
    case 0x6: SetVideoStandard(core::config::sys::VideoStandard::NTSC); break;

    case 0xA: [[fallthrough]];
    case 0xC: [[fallthrough]];
    case 0xD: SetVideoStandard(core::config::sys::VideoStandard::PAL); break;
    }

    if (currAreaCode != selectedAreaCode) {
        Reset(true);
    }
}

void Saturn::EnableDebugTracing(bool enable) {
    if (m_systemFeatures.enableDebugTracing && !enable) {
        DetachAllTracers();
    }
    m_systemFeatures.enableDebugTracing = enable;
    UpdateFunctionPointers();
    SCSP.SetDebugTracing(enable);
}

void Saturn::SaveState(state::State &state) const {
    m_scheduler.SaveState(state.scheduler);
    m_system.SaveState(state.system);
    mem.SaveState(state.system);
    state.system.slaveSH2Enabled = slaveSH2Enabled;
    state.msh2SpilloverCycles = m_msh2SpilloverCycles;
    state.ssh2SpilloverCycles = m_ssh2SpilloverCycles;
    masterSH2.SaveState(state.msh2);
    slaveSH2.SaveState(state.ssh2);
    SCU.SaveState(state.scu);
    SMPC.SaveState(state.smpc);
    VDP.SaveState(state.vdp);
    SCSP.SaveState(state.scsp);
    state.cdblockLLE = m_cdblockLLE;
    if (m_cdblockLLE) {
        SH1.SaveState(state.sh1);
        YGR.SaveState(state.ygr);
        CDDrive.SaveState(state.cddrive);
        state.cdblockDRAM = CDBlockDRAM;
        state.sh1SpilloverCycles = m_sh1SpilloverCycles;
        state.sh1FracCycles = m_sh1FracCycles;
    } else {
        CDBlock.SaveState(state.cdblock);
    }
    state.discHash = GetDiscHash();
}

bool Saturn::LoadState(const state::State &state, bool skipROMChecks) {
    if (!m_scheduler.ValidateState(state.scheduler)) {
        return false;
    }
    if (!m_system.ValidateState(state.system)) {
        return false;
    }
    if (!mem.ValidateState(state.system, skipROMChecks)) {
        return false;
    }
    if (!masterSH2.ValidateState(state.msh2)) {
        return false;
    }
    if (!slaveSH2.ValidateState(state.ssh2)) {
        return false;
    }
    if (!SCU.ValidateState(state.scu)) {
        return false;
    }
    if (!SMPC.ValidateState(state.smpc)) {
        return false;
    }
    if (!VDP.ValidateState(state.vdp)) {
        return false;
    }
    if (!SCSP.ValidateState(state.scsp)) {
        return false;
    }

    if (state.cdblockLLE) {
        if (!SH1.ValidateState(state.sh1, skipROMChecks)) {
            return false;
        }
        if (!YGR.ValidateState(state.ygr)) {
            return false;
        }
        if (!CDDrive.ValidateState(state.cddrive)) {
            return false;
        }
    } else {
        if (!CDBlock.ValidateState(state.cdblock)) {
            return false;
        }
    }
    if (state.discHash != m_fs.GetHash()) {
        return false;
    }

    // Changing this option causes a hard reset, so do it before loading the state
    SetCDBlockLLE(state.cdblockLLE);

    m_scheduler.LoadState(state.scheduler);
    m_system.LoadState(state.system);
    mem.LoadState(state.system);
    slaveSH2Enabled = state.system.slaveSH2Enabled;
    m_msh2SpilloverCycles = state.msh2SpilloverCycles;
    m_ssh2SpilloverCycles = state.ssh2SpilloverCycles;
    masterSH2.LoadState(state.msh2);
    slaveSH2.LoadState(state.ssh2);
    SCU.LoadState(state.scu);
    SMPC.LoadState(state.smpc);
    VDP.LoadState(state.vdp);
    SCSP.LoadState(state.scsp);
    if (m_cdblockLLE) {
        SH1.LoadState(state.sh1);
        YGR.LoadState(state.ygr);
        CDDrive.LoadState(state.cddrive);
        CDBlockDRAM = state.cdblockDRAM;
        m_sh1SpilloverCycles = state.sh1SpilloverCycles;
        m_sh1FracCycles = state.sh1FracCycles;
    } else {
        CDBlock.LoadState(state.cdblock);
    }

    return true;
}

void Saturn::DumpCDBlockDRAM(std::ostream &out) {
    out.write((const char *)CDBlockDRAM.data(), CDBlockDRAM.size());
}

// Run scenarios:
// [x] Run a full frame -- RunFrameImpl()
// [x] Run until next event -- Run()
// [ ] Run for a number of cycles
// [ ] Run until an event from a selection of events is triggered (or a frame is completed, whichever happens first)
//     [ ] On any schedulable event or a subset of them
//     [ ] A breakpoint or watchpoint is triggered
//     [ ] Slave SH-2 is enabled
//     [ ] M68K is enabled
//     [ ] SCU DSP starts running
//     [ ] If any debug tracers ask to suspend emulation (when supported)
// [x] Single-step master SH-2 -- StepMasterSH2()
// [x] Single-step slave SH-2 (if enabled) -- StepSlaveSH2()
// [ ] Single-step M68K (if enabled)
// [ ] Single-step SCU DSP (if running)
// [ ] Single-step SCSP DSP
// [ ] Single-step CD Block SH-1
// Note:
// - Step out/return can be implemented in terms of single-stepping and instruction tracing events

template <bool debug, bool enableSH2Cache, bool cdblockLLE>
void Saturn::RunFrameImpl() {
    // Use the last line phase as reference to give some leeway if we overshoot the target cycles
    while (VDP.InLastLinePhase()) {
        if (!Run<debug, enableSH2Cache, cdblockLLE>()) {
            return;
        }
    }
    while (!VDP.InLastLinePhase()) {
        if (!Run<debug, enableSH2Cache, cdblockLLE>()) {
            return;
        }
    }
}

template <bool debug, bool enableSH2Cache, bool cdblockLLE>
bool Saturn::Run() {
    static constexpr uint64 kSH2SyncMaxStep = 32;

    const uint64 cycles = static_config::max_timing_granularity ? 1 : std::max<sint64>(m_scheduler.RemainingCount(), 0);

    uint64 execCycles;
    if (SCU.IsDMAActive()) {
        // Stall both SH2 CPUs and only run the SCU and other stuff
        execCycles = cycles;
        SCU.Advance<debug>(execCycles);
    } else {
        execCycles = m_msh2SpilloverCycles;
        m_msh2SpilloverCycles = 0;
        if (slaveSH2Enabled) {
            uint64 slaveCycles = m_ssh2SpilloverCycles;
            do {
                const uint64 prevExecCycles = execCycles;
                const uint64 targetCycles = std::min(execCycles + kSH2SyncMaxStep, cycles);
                execCycles = masterSH2.Advance<debug, enableSH2Cache>(targetCycles, execCycles);
                slaveCycles = slaveSH2.Advance<debug, enableSH2Cache>(execCycles, slaveCycles);
                SCU.Advance<debug>(execCycles - prevExecCycles);
                if constexpr (debug) {
                    if (m_debugBreakMgr.IsDebugBreakRaised()) {
                        break;
                    }
                }
            } while (execCycles < cycles);
            if constexpr (debug) {
                // If the SSH2 hits a breakpoint early, the cycle count may be shorter than the total executed cycles.
                if (slaveCycles > execCycles) {
                    m_ssh2SpilloverCycles = slaveCycles - execCycles;
                } else {
                    m_msh2SpilloverCycles = execCycles - slaveCycles;
                }
            } else {
                m_ssh2SpilloverCycles = slaveCycles - execCycles;
            }
        } else {
            do {
                const uint64 prevExecCycles = execCycles;
                const uint64 targetCycles = std::min(execCycles + kSH2SyncMaxStep, cycles);
                execCycles = masterSH2.Advance<debug, enableSH2Cache>(targetCycles, execCycles);
                SCU.Advance<debug>(execCycles - prevExecCycles);
                if constexpr (debug) {
                    if (m_debugBreakMgr.IsDebugBreakRaised()) {
                        break;
                    }
                }
            } while (execCycles < cycles);
        }
    }
    VDP.Advance<debug>(execCycles);

    // SCSP+M68K and CD block are ticked by the scheduler

    if constexpr (cdblockLLE) {
        AdvanceSH1(execCycles);
        // CD drive is ticked by the scheduler
    }

    const auto &cc = GetClockRatios();

    // TODO: AdvanceSMPC(execCycles);
    /*const auto &clockRatios = GetClockRatios();
    const uint64 smpcScaledCycles = cycles * clockRatios.SMPCNum + m_smpcFracCycles;
    const uint64 smpcCycles = smpcScaledCycles / clockRatios.SMPCDen;
    m_smpcFracCycles = smpcScaledCycles % clockRatios.SMPCDen;
    if (smpcCycles > 0) {
        SMPC.Advance(smpcCycles);
    }*/

    m_scheduler.Advance(execCycles);

    if constexpr (debug) {
        if (m_debugBreakMgr.LowerDebugBreak()) {
            return false;
        }
    }

    return true;
}

template <bool debug, bool enableSH2Cache, bool cdblockLLE>
uint64 Saturn::StepMasterSH2Impl() {
    while (SCU.IsDMAActive()) {
        const uint64 cycles = 64;
        SCU.Advance<debug>(cycles);
        VDP.Advance<debug>(cycles);
        // SCSP+M68K and CD block are ticked by the scheduler
        if constexpr (cdblockLLE) {
            AdvanceSH1(cycles);
            // CD drive is ticked by the scheduler
        }
        // TODO: AdvanceSMPC(cycles);
        m_scheduler.Advance(cycles);
    }

    uint64 masterCycles = masterSH2.Step<debug, enableSH2Cache>();
    if (masterCycles >= m_msh2SpilloverCycles) {
        masterCycles -= m_msh2SpilloverCycles;
        m_msh2SpilloverCycles = 0;
        if (slaveSH2Enabled) {
            const uint64 slaveCycles = slaveSH2.Advance<debug, enableSH2Cache>(masterCycles, m_ssh2SpilloverCycles);
            m_ssh2SpilloverCycles = slaveCycles - masterCycles;
        }
        SCU.Advance<debug>(masterCycles);
        VDP.Advance<debug>(masterCycles);
        // SCSP+M68K and CD block are ticked by the scheduler
        if constexpr (cdblockLLE) {
            AdvanceSH1(masterCycles);
            // CD drive is ticked by the scheduler
        }
        // TODO: AdvanceSMPC(masterCycles);
        m_scheduler.Advance(masterCycles);
    } else {
        m_msh2SpilloverCycles -= masterCycles;
    }
    return masterCycles;
}

template <bool debug, bool enableSH2Cache, bool cdblockLLE>
uint64 Saturn::StepSlaveSH2Impl() {
    if (!slaveSH2Enabled) {
        return 0;
    }

    while (SCU.IsDMAActive()) {
        const uint64 cycles = 64;
        SCU.Advance<debug>(cycles);
        VDP.Advance<debug>(cycles);
        // SCSP+M68K and CD block are ticked by the scheduler
        if constexpr (cdblockLLE) {
            AdvanceSH1(cycles);
            // CD drive is ticked by the scheduler
        }
        // TODO: AdvanceSMPC(cycles);
        m_scheduler.Advance(cycles);
    }

    uint64 slaveCycles = slaveSH2.Step<debug, enableSH2Cache>();
    if (slaveCycles >= m_ssh2SpilloverCycles) {
        slaveCycles -= m_ssh2SpilloverCycles;
        m_ssh2SpilloverCycles = 0;
        const uint64 masterCycles = masterSH2.Advance<debug, enableSH2Cache>(slaveCycles, m_msh2SpilloverCycles);
        m_msh2SpilloverCycles = masterCycles - slaveCycles;
        SCU.Advance<debug>(slaveCycles);
        VDP.Advance<debug>(slaveCycles);
        // SCSP+M68K and CD block are ticked by the scheduler
        if constexpr (cdblockLLE) {
            AdvanceSH1(slaveCycles);
            // CD drive is ticked by the scheduler
        }
        // TODO: AdvanceSMPC(slaveCycles);
        m_scheduler.Advance(slaveCycles);
    } else {
        m_ssh2SpilloverCycles -= slaveCycles;
    }
    return slaveCycles;
}

void Saturn::UpdateFunctionPointers() {
    UpdateFunctionPointersTemplate(m_systemFeatures.enableDebugTracing, m_systemFeatures.emulateSH2Cache, m_cdblockLLE);
}

template <bool... t_features>
void Saturn::UpdateFunctionPointersTemplate(bool feature, auto... features) {
    feature ? UpdateFunctionPointersTemplate<t_features..., true>(features...)
            : UpdateFunctionPointersTemplate<t_features..., false>(features...);
}

template <bool... t_features>
void Saturn::UpdateFunctionPointersTemplate() {
    m_runFrameFn = &Saturn::RunFrameImpl<t_features...>;
    m_stepMSH2Fn = &Saturn::StepMasterSH2Impl<t_features...>;
    m_stepSSH2Fn = &Saturn::StepSlaveSH2Impl<t_features...>;
}

FORCE_INLINE void Saturn::AdvanceSH1(uint64 cycles) {
    const auto &clockRatios = GetClockRatios();
    const uint64 sh1ScaledCycles = cycles * clockRatios.CDBlockNum + m_sh1FracCycles;
    const uint64 sh1Cycles = sh1ScaledCycles / clockRatios.CDBlockDen;
    m_sh1FracCycles = sh1ScaledCycles % clockRatios.CDBlockDen;
    if (sh1Cycles > 0) {
        const uint64 sh1ExecCycles = SH1.Advance(sh1Cycles, m_sh1SpilloverCycles);
        m_sh1SpilloverCycles = sh1ExecCycles - sh1Cycles;
    }
}

void Saturn::UpdatePreferredRegionOrder(std::span<const core::config::sys::Region> regions) {
    m_preferredRegionOrder.clear();
    media::AreaCode usedAreaCodes = media::AreaCode::None;
    auto addAreaCode = [&](media::AreaCode areaCode) {
        if (BitmaskEnum(usedAreaCodes).NoneOf(areaCode)) {
            usedAreaCodes |= areaCode;
            m_preferredRegionOrder.push_back(areaCode);
        }
    };

    using Region = core::config::sys::Region;
    for (const Region region : regions) {
        switch (region) {
        case Region::Japan: addAreaCode(media::AreaCode::Japan); break;
        case Region::AsiaNTSC: addAreaCode(media::AreaCode::AsiaNTSC); break;
        case Region::NorthAmerica: addAreaCode(media::AreaCode::NorthAmerica); break;
        case Region::CentralSouthAmericaNTSC: addAreaCode(media::AreaCode::CentralSouthAmericaNTSC); break;
        case Region::Korea: addAreaCode(media::AreaCode::Korea); break;
        case Region::AsiaPAL: addAreaCode(media::AreaCode::AsiaPAL); break;
        case Region::EuropePAL: addAreaCode(media::AreaCode::EuropePAL); break;
        case Region::CentralSouthAmericaPAL: addAreaCode(media::AreaCode::CentralSouthAmericaPAL); break;
        }
    }
}

void Saturn::UpdateSH2CacheEmulation(bool enabled) {
    enabled |= m_forceSH2CacheEmulation;
    if (!m_systemFeatures.emulateSH2Cache && enabled) {
        masterSH2.PurgeCache();
        slaveSH2.PurgeCache();
    }
    m_systemFeatures.emulateSH2Cache = enabled;
    UpdateFunctionPointers();
}

void Saturn::UpdateVideoStandard(core::config::sys::VideoStandard videoStandard) {
    m_system.videoStandard = videoStandard;
    m_system.UpdateClockRatios();
}

void Saturn::SetCDBlockLLE(bool enabled) {
    if (m_cdblockLLE != enabled) {
        m_cdblockLLE = enabled;
        if (enabled) {
            YGR.MapMemory(mainBus);
        } else {
            CDBlock.MapMemory(mainBus);
        }
        UpdateFunctionPointers();
        Reset(true);
    }
}

// -----------------------------------------------------------------------------
// System operations (SMPC) - smpc::ISMPCOperations implementation

Saturn::SMPCOperations::SMPCOperations(Saturn &saturn)
    : m_saturn(saturn) {}

bool Saturn::SMPCOperations::GetNMI() const {
    return m_saturn.masterSH2.GetNMI();
}

void Saturn::SMPCOperations::RaiseNMI() {
    m_saturn.masterSH2.SetNMI();
}

void Saturn::SMPCOperations::EnableAndResetSlaveSH2() {
    m_saturn.slaveSH2Enabled = true;
    m_saturn.slaveSH2.Reset(true);
}

void Saturn::SMPCOperations::DisableSlaveSH2() {
    m_saturn.slaveSH2Enabled = false;
}

void Saturn::SMPCOperations::EnableAndResetM68K() {
    m_saturn.SCSP.SetCPUEnabled(true);
}

void Saturn::SMPCOperations::DisableM68K() {
    m_saturn.SCSP.SetCPUEnabled(false);
}

void Saturn::SMPCOperations::SoftResetSystem() {
    m_saturn.Reset(false);
}

void Saturn::SMPCOperations::ClockChangeSoftReset() {
    m_saturn.VDP.Reset(false);
    m_saturn.SCU.Reset(false);
    m_saturn.SCSP.Reset(false);
}

sys::ClockSpeed Saturn::SMPCOperations::GetClockSpeed() const {
    return m_saturn.GetClockSpeed();
}

void Saturn::SMPCOperations::SetClockSpeed(sys::ClockSpeed clockSpeed) {
    m_saturn.SetClockSpeed(clockSpeed);
}

} // namespace ymir
