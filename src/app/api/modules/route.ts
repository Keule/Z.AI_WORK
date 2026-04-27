import { NextResponse } from 'next/server'
import { readFileSync, existsSync, readdirSync, statSync } from 'fs'
import { join } from 'path'

const ESP32_SRC = join(process.cwd(), 'esp32_project', 'src', 'logic')
const MODULE_INTERFACE = join(ESP32_SRC, 'module_interface.h')
const MODULE_SYSTEM = join(ESP32_SRC, 'module_system.cpp')

// Module metadata derived from source code analysis
// Category mapping based on ModuleId enum comments in module_interface.h
const MODULE_CATEGORIES: Record<string, { category: string; fullName: string; description: string }> = {
  ETH: { category: 'Kommunikation', fullName: 'Ethernet', description: 'Transport module: W5500 Ethernet. Provides physical transport layer for UDP communication with AgOpenGPS.' },
  WIFI: { category: 'Kommunikation', fullName: 'WLAN', description: 'Transport module: WiFi (AP/STA). Fallback transport when Ethernet is not connected.' },
  BT: { category: 'Kommunikation', fullName: 'Bluetooth', description: 'Transport module: Bluetooth SPP. BLE-Peripherie für NMEA/AgIO Kommunikation.' },
  NETWORK: { category: 'Kommunikation', fullName: 'Netzwerk', description: 'Protocol module: PGN Codec RX/TX over UDP. All PGN encoding/decoding for AgOpenGPS.' },
  GNSS: { category: 'Sensor', fullName: 'GNSS-Empfänger', description: 'Sensor module: UM980 dual UART management. UART initialization and NMEA parsing. GPS/Galileo/GLONASS/BeiDou.' },
  NTRIP: { category: 'Kommunikation', fullName: 'NTRIP-Client', description: 'Service module: NTRIP client. RTCM correction stream from NTRIP caster.' },
  IMU: { category: 'Sensor', fullName: 'Trägheitsnavigation', description: 'Sensor module: BNO085 SPI. Beschleunigung & Gyroskop (MPU-6050/ICM-20948).' },
  WAS: { category: 'Sensor', fullName: 'Winkel Sensor', description: 'Sensor module: ADS1118 / Wheel Angle Sensor. Winkelsensor für Lenkwinkel & Höhensteuerung.' },
  ACTUATOR: { category: 'Core', fullName: 'Aktuator', description: 'Actuator module: DRV8263 SPI. PWM-Steuerausgänge für Ventile & Motoren.' },
  SAFETY: { category: 'Sicherheit', fullName: 'Sicherheit', description: 'Sensor module: Safety-Loop GPIO + Watchdog. Watchdog, Fault-Handler & Notabschaltung.' },
  STEER: { category: 'Core', fullName: 'Lenkung', description: 'Logic module: PID Controller. Reads sensor snapshots, computes PID, writes actuator. PID-Spurregelung & Autosteuerung.' },
  LOGGING: { category: 'Werkzeug', fullName: 'Protokollierung', description: 'Service module: SD-Card Logger. Systemprotokoll & Debug-Ausgabe.' },
  OTA: { category: 'Werkzeug', fullName: 'Firmware Update', description: 'Service module: OTA Update. Over-the-Air Firmware-Aktualisierung.' },
  SPI: { category: 'Core', fullName: 'SPI-Bus', description: 'Infrastructure: sensor SPI bus (single-consumer, no mutex). SPI-Halbleitung für IMU & Displays.' },
  SPI_SHARED: { category: 'Core', fullName: 'SPI Geteilt', description: 'Infrastructure: multi-client SPI arbitration (mutex + CS). Multiplexed SPI-Bus mit Mutex-Schutz.' },
  REMOTE_CONSOLE: { category: 'Werkzeug', fullName: 'Fernkonsole', description: 'Service module: TCP/Telnet Remote Console (DebugConsole wrapper). Remote-Shell über TCP/Bluetooth.' },
}

// Dependencies parsed from module_system.cpp forward declarations
// Based on s_deps arrays in each mod_*.cpp file
const MODULE_DEPS: Record<string, string[]> = {
  ETH: [],
  WIFI: [],
  BT: [],
  NETWORK: ['ETH'],
  GNSS: [],
  NTRIP: ['NETWORK', 'GNSS'],
  IMU: [],
  WAS: [],
  ACTUATOR: ['IMU', 'WAS'],
  SAFETY: [],
  STEER: ['IMU', 'WAS', 'ACTUATOR', 'SAFETY'],
  LOGGING: [],
  OTA: ['NETWORK'],
  SPI: [],
  SPI_SHARED: ['SPI'],
  REMOTE_CONSOLE: ['NETWORK', 'BT'],
}

// Freshness timeouts from kDefaultFreshness[] in module_system.cpp
const MODULE_FRESHNESS: Record<string, number> = {
  ETH: 5000,
  WIFI: 5000,
  BT: 5000,
  NETWORK: 1000,
  GNSS: 2000,
  NTRIP: 10000,
  IMU: 500,
  WAS: 300,
  ACTUATOR: 1000,
  SAFETY: 500,
  STEER: 500,
  LOGGING: 5000,
  OTA: 10000,
  SPI: 0,
  SPI_SHARED: 0,
  REMOTE_CONSOLE: 5000,
}

// Feature flags from features.h
const MODULE_FEATURE_FLAGS: Record<string, string> = {
  ETH: 'FEAT_ETH (Pflicht)',
  WIFI: 'FEAT_WIFI',
  BT: 'FEAT_WIFI',
  NETWORK: '(immer kompiliert)',
  GNSS: 'FEAT_GNSS',
  NTRIP: 'FEAT_NTRIP',
  IMU: 'FEAT_IMU',
  WAS: 'FEAT_ADS',
  ACTUATOR: 'FEAT_ACT',
  SAFETY: 'FEAT_SAFETY',
  STEER: 'FEAT_ACT && FEAT_SAFETY',
  LOGGING: 'FEAT_SD',
  OTA: '(immer kompiliert)',
  SPI: '(immer kompiliert)',
  SPI_SHARED: '(immer kompiliert)',
  REMOTE_CONSOLE: 'FEAT_REMOTE_CONSOLE',
}

interface ModuleSourceInfo {
  headerExists: boolean
  sourceExists: boolean
  headerSize: number
  sourceSize: number
  headerModified: string | null
  sourceModified: string | null
}

function getModuleSourceInfo(moduleId: string): ModuleSourceInfo {
  // Map module ID to file name
  const nameMap: Record<string, string> = {
    REMOTE_CONSOLE: 'mod_remote_console',
  }
  const fileName = nameMap[moduleId] || `mod_${moduleId.toLowerCase()}`
  const headerPath = join(ESP32_SRC, `${fileName}.h`)
  const sourcePath = join(ESP32_SRC, `${fileName}.cpp`)

  const getInfo = (path: string) => {
    if (!existsSync(path)) return { exists: false, size: 0, modified: null }
    const stat = statSync(path)
    return { exists: true, size: stat.size, modified: stat.mtime.toISOString() }
  }

  const header = getInfo(headerPath)
  const source = getInfo(sourcePath)

  return {
    headerExists: header.exists,
    sourceExists: source.exists,
    headerSize: header.size,
    sourceSize: source.size,
    headerModified: header.modified,
    sourceModified: source.modified,
  }
}

export async function GET() {
  try {
    // Parse module_interface.h to get the ModuleId enum order
    const interfaceContent = existsSync(MODULE_INTERFACE)
      ? readFileSync(MODULE_INTERFACE, 'utf-8')
      : ''

    // Extract module IDs from the enum
    const enumMatch = interfaceContent.match(/enum class ModuleId[\s\S]*?COUNT/m)
    const enumModules: string[] = []
    if (enumMatch) {
      const idMatches = enumMatch[0].matchAll(/^\s+(\w+)\s*=/gm)
      for (const m of idMatches) {
        enumModules.push(m[1])
      }
    }

    // Build modules list
    const moduleIds = Object.keys(MODULE_CATEGORIES)
    const modules = moduleIds.map(id => {
      const meta = MODULE_CATEGORIES[id]
      const sourceInfo = getModuleSourceInfo(id)

      // Try to extract config keys from the source file
      const nameMap: Record<string, string> = { REMOTE_CONSOLE: 'mod_remote_console' }
      const fileName = nameMap[id] || `mod_${id.toLowerCase()}`
      const sourcePath = join(ESP32_SRC, `${fileName}.cpp`)
      let configKeys: string[] = []
      if (existsSync(sourcePath)) {
        const src = readFileSync(sourcePath, 'utf-8')
        // Parse CfgKeyDef arrays
        const keysMatch = src.matchAll(/CfgKeyDef\s+\w+_keys\[\]\s*=\s*\{([^}]+)\}/g)
        for (const m of keysMatch) {
          const keyMatches = m[1].matchAll(/"(\w+)"/g)
          for (const km of keyMatches) {
            configKeys.push(km[1])
          }
        }
      }

      // Extract error codes
      let errorCodes: string[] = []
      if (existsSync(sourcePath)) {
        const src = readFileSync(sourcePath, 'utf-8')
        const errMatches = src.matchAll(/ERR_\w+\s*=\s*(\d+)/g)
        for (const m of errMatches) {
          errorCodes.push(m[0])
        }
      }

      return {
        id,
        name: id === 'REMOTE_CONSOLE' ? 'RCON' : id,
        fullName: meta.fullName,
        category: meta.category,
        description: meta.description,
        freshness: MODULE_FRESHNESS[id] ?? 0,
        deps: MODULE_DEPS[id] || [],
        featureFlag: MODULE_FEATURE_FLAGS[id] || '',
        configKeys,
        errorCodes,
        source: sourceInfo,
      }
    })

    // Stats
    const stats = {
      total: modules.length,
      byCategory: {} as Record<string, number>,
      totalFreshness: modules.reduce((sum, m) => sum + m.freshness, 0),
      withSource: modules.filter(m => m.source.sourceExists).length,
    }

    for (const m of modules) {
      stats.byCategory[m.category] = (stats.byCategory[m.category] || 0) + 1
    }

    return NextResponse.json({ modules, stats })
  } catch (err) {
    console.error('Modules API error:', err)
    return NextResponse.json({ error: 'Failed to parse modules' }, { status: 500 })
  }
}
