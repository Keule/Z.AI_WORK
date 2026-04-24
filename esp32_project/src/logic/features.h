#pragma once

// ===================================================================
// Feature Flags — hardware-nahe Funktionseinheiten
//
// Compile-Time: -DFEAT_XXX (platformio.ini / Build-Profil)
// Runtime:      moduleActivate(MOD_XXX) / moduleDeactivate(MOD_XXX)
// ===================================================================

#if defined(FEAT_IMU)
#define FEAT_COMPILED_IMU 1
#else
#define FEAT_COMPILED_IMU 0
#endif

#if defined(FEAT_ADS)
#define FEAT_COMPILED_ADS 1
#else
#define FEAT_COMPILED_ADS 0
#endif

#if defined(FEAT_ACT)
#define FEAT_COMPILED_ACT 1
#else
#define FEAT_COMPILED_ACT 0
#endif

#if defined(FEAT_ETH)
#define FEAT_COMPILED_ETH 1
#else
#define FEAT_COMPILED_ETH 0
#endif

#if defined(FEAT_GNSS)
#define FEAT_COMPILED_GNSS 1
#else
#define FEAT_COMPILED_GNSS 0
#endif

#if defined(FEAT_NTRIP)
#define FEAT_COMPILED_NTRIP 1
#else
#define FEAT_COMPILED_NTRIP 0
#endif

#if defined(FEAT_SD)
#define FEAT_COMPILED_SD 1
#else
#define FEAT_COMPILED_SD 0
#endif

#if defined(FEAT_SAFETY)
#define FEAT_COMPILED_SAFETY 1
#else
#define FEAT_COMPILED_SAFETY 0
#endif

#if defined(FEAT_LOGSW)
#define FEAT_COMPILED_LOGSW 1
#else
#define FEAT_COMPILED_LOGSW 0
#endif

static_assert(FEAT_COMPILED_ETH, "FEAT_ETH ist Pflicht (mindestens Ethernet/UDP).");

#define FEAT_ENABLED(flag)  ((flag) != 0)
#define FEAT_DISABLED(flag) ((flag) == 0)

namespace feat {
inline constexpr bool imu()    { return FEAT_ENABLED(FEAT_COMPILED_IMU); }
inline constexpr bool ads()    { return FEAT_ENABLED(FEAT_COMPILED_ADS); }
inline constexpr bool act()    { return FEAT_ENABLED(FEAT_COMPILED_ACT); }
inline constexpr bool eth()    { return FEAT_ENABLED(FEAT_COMPILED_ETH); }
inline constexpr bool gnss()   { return FEAT_ENABLED(FEAT_COMPILED_GNSS); }
inline constexpr bool ntrip()  { return FEAT_ENABLED(FEAT_COMPILED_NTRIP); }
inline constexpr bool sd()     { return FEAT_ENABLED(FEAT_COMPILED_SD); }
inline constexpr bool safety() { return FEAT_ENABLED(FEAT_COMPILED_SAFETY); }
inline constexpr bool logsw()  { return FEAT_ENABLED(FEAT_COMPILED_LOGSW); }
}  // namespace feat
