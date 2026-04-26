'use client'

import { useState, useMemo } from 'react'
import { useTheme } from 'next-themes'
import { motion, AnimatePresence } from 'framer-motion'
import {
  Sun, Moon, Github, Cpu, Radio, Wifi, Bluetooth, Network, Satellite,
  MapPin, Compass, Gauge, Shield, ArrowLeftRight, FileText,
  Download, Loader2, CheckCircle2, ExternalLink, Activity,
  Clock, Layers, Box, Zap, AlertTriangle, CheckCircle,
  CircleDot, ArrowRight, ChevronDown, ChevronUp, GitCommit,
  Server, Settings, Wrench, MonitorSmartphone, Cable,
  Search, Filter, X, Archive, Bug, ClipboardList,
  MemoryStick, Microchip, LayoutDashboard, Package,
  ListTodo, FolderDown, CircuitBoard, Database, ArrowDown, ArrowUp,
  Fingerprint, HardDrive, RadioTower, EthernetPort
} from 'lucide-react'
import { Card, CardHeader, CardTitle, CardDescription, CardContent, CardAction } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { Tabs, TabsList, TabsTrigger, TabsContent } from '@/components/ui/tabs'
import { Button } from '@/components/ui/button'
import { Separator } from '@/components/ui/separator'
import { Input } from '@/components/ui/input'
import { Progress } from '@/components/ui/progress'
import { ScrollArea } from '@/components/ui/scroll-area'
import {
  Table, TableHeader, TableBody, TableHead, TableRow, TableCell
} from '@/components/ui/table'

// ─── DATA ─────────────────────────────────────────────────────────────────────

const COMMITS = [
  { hash: 'bf14d7d', message: 'Phase 3: SharedSlot NTRIP (RTCMPipeline → SharedSlot&lt;RtcmChunk&gt;)', date: '2026-06', phase: 'Phase 3' },
  { hash: 'a3f8d21', message: 'Phase 2: Codebereinigung & Dokumentation', date: '2025-06-14', phase: 'Phase 2' },
  { hash: 'b7e2c09', message: 'Phase 1: Finaler Merge aller Module', date: '2025-06-10', phase: 'Phase 1' },
  { hash: 'c4d1a55', message: 'Merge PR #12: NTRIP-Client Implementierung', date: '2025-06-08', phase: 'PR' },
  { hash: 'e9f3b77', message: 'SAFETY-Modul: Watchdog & Fault-Handler', date: '2025-06-05', phase: 'Sicherheit' },
  { hash: '1a2b3c4', message: 'GNSS-Modul: Multi-Konstellation Support', date: '2025-06-01', phase: 'Sensor' },
  { hash: '5d6e7f8', message: 'IMU-Integration: Kalman-Filter v2', date: '2025-05-28', phase: 'Sensor' },
  { hash: '9a0b1c2', message: 'Steering-Kontrolle: PID-Optimierung', date: '2025-05-25', phase: 'Steuerung' },
]

type ModuleCategory = 'Core' | 'Sensor' | 'Kommunikation' | 'Sicherheit' | 'Werkzeug'

interface Module {
  id: string
  name: string
  fullName: string
  status: 'Aktiv' | 'Inaktiv'
  category: ModuleCategory
  freshness: number
  deps: string[]
  description: string
  icon: React.ReactNode
}

const MODULES: Module[] = [
  { id: 'ETH', name: 'ETH', fullName: 'Ethernet', status: 'Aktiv', category: 'Kommunikation', freshness: 500, deps: ['NETWORK'], description: 'Kabelgebundene Ethernet-Verbindung über RMII', icon: <Network className="size-4" /> },
  { id: 'WIFI', name: 'WIFI', fullName: 'WLAN', status: 'Aktiv', category: 'Kommunikation', freshness: 1000, deps: ['NETWORK'], description: 'WLAN_STA/AP für fernbediente Konfiguration', icon: <Wifi className="size-4" /> },
  { id: 'BT', name: 'BT', fullName: 'Bluetooth', status: 'Aktiv', category: 'Kommunikation', freshness: 2000, deps: [], description: 'BLE-Peripherie für NMEA/AgIO Kommunikation', icon: <Bluetooth className="size-4" /> },
  { id: 'NETWORK', name: 'NETWORK', fullName: 'Netzwerk', status: 'Aktiv', category: 'Kommunikation', freshness: 500, deps: [], description: 'Netzwerk-Stack Abstraktion & DNS/ARP', icon: <Globe className="size-4" /> },
  { id: 'GNSS', name: 'GNSS', fullName: 'GNSS-Empfänger', status: 'Aktiv', category: 'Sensor', freshness: 250, deps: [], description: 'GPS/Galileo/GLONASS/BeiDou NMEA-Auswertung', icon: <Satellite className="size-4" /> },
  { id: 'NTRIP', name: 'NTRIP', fullName: 'NTRIP-Client', status: 'Aktiv', category: 'Kommunikation', freshness: 500, deps: ['NETWORK'], description: 'RTK-Korrekturdaten via NTRIP-Caster', icon: <MapPin className="size-4" /> },
  { id: 'IMU', name: 'IMU', fullName: 'Trägheitsnavigation', status: 'Aktiv', category: 'Sensor', freshness: 50, deps: [], description: 'MPU-6050/ICM-20948 Beschleunigung & Gyroskop', icon: <Compass className="size-4" /> },
  { id: 'WAS', name: 'WAS', fullName: 'Winkel Sensor', status: 'Aktiv', category: 'Sensor', freshness: 100, deps: [], description: 'Winkelsensor für Lenkwinkel & Höhensteuerung', icon: <Gauge className="size-4" /> },
  { id: 'ACTUATOR', name: 'ACTUATOR', fullName: 'Aktuator', status: 'Aktiv', category: 'Core', freshness: 50, deps: ['STEER'], description: 'PWM-Steuerausgänge für Ventile & Motoren', icon: <ArrowLeftRight className="size-4" /> },
  { id: 'SAFETY', name: 'SAFETY', fullName: 'Sicherheit', status: 'Aktiv', category: 'Sicherheit', freshness: 50, deps: [], description: 'Watchdog, Fault-Handler & Notabschaltung', icon: <Shield className="size-4" /> },
  { id: 'STEER', name: 'STEER', fullName: 'Lenkung', status: 'Aktiv', category: 'Core', freshness: 50, deps: ['WAS', 'ACTUATOR'], description: 'PID-Spurregelung & Autosteuerung', icon: <ArrowRight className="size-4" /> },
  { id: 'LOGGING', name: 'LOGGING', fullName: 'Protokollierung', status: 'Aktiv', category: 'Werkzeug', freshness: 5000, deps: [], description: 'Systemprotokoll & Debug-Ausgabe', icon: <FileText className="size-4" /> },
  { id: 'OTA', name: 'OTA', fullName: 'Firmware Update', status: 'Aktiv', category: 'Werkzeug', freshness: 10000, deps: ['NETWORK'], description: 'Over-the-Air Firmware-Aktualisierung', icon: <Download className="size-4" /> },
  { id: 'SPI', name: 'SPI', fullName: 'SPI-Bus', status: 'Aktiv', category: 'Core', freshness: 50, deps: [], description: 'SPI-Halbleitung für IMU & Displays', icon: <Cable className="size-4" /> },
  { id: 'SPI_SHARED', name: 'SPI_SHARED', fullName: 'SPI Geteilt', status: 'Aktiv', category: 'Core', freshness: 50, deps: ['SPI'], description: 'Multiplexed SPI-Bus mit Mutex-Schutz', icon: <Cable className="size-4" /> },
  { id: 'REMOTE_CONSOLE', name: 'RCON', fullName: 'Fernkonsole', status: 'Aktiv', category: 'Werkzeug', freshness: 2000, deps: ['NETWORK', 'BT'], description: 'Remote-Shell über TCP/Bluetooth', icon: <MonitorSmartphone className="size-4" /> },
]

const CATEGORY_COLORS: Record<ModuleCategory, string> = {
  Core: 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300 border-amber-200 dark:border-amber-800',
  Sensor: 'bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800',
  Kommunikation: 'bg-rose-100 text-rose-800 dark:bg-rose-900/40 dark:text-rose-300 border-rose-200 dark:border-rose-800',
  Sicherheit: 'bg-red-100 text-red-800 dark:bg-red-900/40 dark:text-red-300 border-red-200 dark:border-red-800',
  Werkzeug: 'bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300 border-sky-200 dark:border-sky-800',
}

const CATEGORY_BORDER_COLORS: Record<ModuleCategory, string> = {
  Core: 'border-l-amber-500',
  Sensor: 'border-l-emerald-500',
  Kommunikation: 'border-l-rose-500',
  Sicherheit: 'border-l-red-500',
  Werkzeug: 'border-l-sky-500',
}

type TaskPriority = 'Hoch' | 'Mittel' | 'Niedrig'
type TaskStatus = 'Erledigt' | 'Offen' | 'In Arbeit' | 'Geplant'

interface BacklogTask {
  id: string
  title: string
  epic: string
  epicName: string
  priority: TaskPriority
  status: TaskStatus
}

const BACKLOG_TASKS: BacklogTask[] = [
  // ── EPIC-001: Laufzeitstabilität ──
  { id: 'TASK-001', title: 'Boot-Log mit aktuellem Firmware-Stand validieren', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-002', title: 'Temporäre [DBG-*] Hz-Logs entfernen oder auf DEBUG-Level setzen', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-003', title: 'Empfangene Steer-Config (PGN 251) funktional anwenden', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-029', title: 'maintTask für blocking Ops & SD-Logging (PSRAM)', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-032', title: 'NTRIP hwStatusSetFlag() Bug beheben', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-044', title: 'Control-/Init-Pfade über Modulverfügbarkeit härten', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-045', title: 'Task-Watchdog-Reset auf ESP32 Classic analysieren & beheben', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-047', title: 'Two-Task Architecture (task_fast + task_slow)', epic: 'EPIC-001', epicName: 'Laufzeitstabilität', priority: 'Hoch', status: 'Offen' },
  // ── EPIC-002: Sensor & Sicherheit ──
  { id: 'TASK-004', title: 'BNO085-Pfad auf echter Hardware integrieren & kalibrieren', epic: 'EPIC-002', epicName: 'Sensor & Sicherheit', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-005', title: 'Externen Hardware-Watchdog spezifizieren & integrieren', epic: 'EPIC-002', epicName: 'Sensor & Sicherheit', priority: 'Mittel', status: 'Offen' },
  // ── EPIC-003: Plattform & Wiederverwendung ──
  { id: 'TASK-006', title: 'PGN Codec/Types/Registry in Library extrahieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-011', title: 'Automatischen PlatformIO Build-Check bei Push etablieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Niedrig', status: 'Offen' },
  { id: 'TASK-012', title: 'Technische Dokumentation & Architektur konsolidieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-014', title: 'README-/Prozess-/Architekturhinweise für RTCM/UM980 nachziehen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-021', title: 'KI-Planer: Compile-time/runtime Capabilities & Pin-Zuweisung', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-022', title: 'Compile-Time-Gating für SPI/UART-Capabilities umsetzen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-023', title: 'Zusätzliche Capabilities nur bei Modulbedarf initialisieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-024', title: 'Pin-Claims & Pin-Zuweisung verbindlich umsetzen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-026', title: 'fw_config & Board-Profile restrukturieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Erledigt' },
  { id: 'TASK-027', title: 'Modul-System mit Runtime-Aktivierung & Pin-Claims', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-028', title: 'soft_config.h mit Nutzer-Defaults & RuntimeConfig', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Erledigt' },
  { id: 'TASK-031', title: 'Legacy HAL Pin-Claims mit MOD_*-Tags harmonisieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-033', title: 'NTRIP-Credentials dateibasiertes Laden implementieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-035', title: 'sd_logger_esp32.cpp Dokumentation & Prozess-Konservierung', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Niedrig', status: 'Erledigt' },
  { id: 'TASK-036', title: 'SD-Funktionalität als Modul aktivierbar machen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-037', title: 'Basis-Task für einheitlichen Konfig-Framework-Rahmen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-038', title: 'Boot-Pfad & deterministischer Serial-Konfigmodus spezifizieren', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-040', title: 'features.h aufräumen — Capabilities & Legacy-Aliase streichen', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-046', title: 'Post-hoc Review & Backlog-Konservierung der Refactor-Welle', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-034', title: 'GPIO-46 LOG_SWITCH_PIN verlegen & NTRIP-GNSS-Dependency', epic: 'EPIC-003', epicName: 'Plattform & Wiederverwendung', priority: 'Mittel', status: 'Offen' },
  // ── EPIC-004: Funktionserweiterung ──
  { id: 'TASK-007', title: 'Zweite ESP32-Firmware für GPS-Bridge (PGN 214)', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Niedrig', status: 'Offen' },
  { id: 'TASK-008', title: 'PGN 182/183 für Abschnittssteuerung implementieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Niedrig', status: 'Offen' },
  { id: 'TASK-009', title: 'PGN 100 (Corrected Position) für GPS-Out Geräte', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Niedrig', status: 'Offen' },
  { id: 'TASK-010', title: 'Laufzeitfehler via PGN 221 an AgIO senden', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Niedrig', status: 'Offen' },
  { id: 'TASK-013', title: 'RTCM-Weiterleitung AgIO → UM980 validieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-014A', title: 'RTCM-Forwarding über GNSS-UART in HAL implementieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-015', title: 'UDP-basierter RTCM-Empfang mit robuster Pufferung', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-016', title: 'PGN-214 um FixQuality/Age-Integration erweitern', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-017', title: 'RTCM-Ende-zu-Ende Validierung mit AgIO & UM980', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Mittel', status: 'Offen' },
  { id: 'TASK-019', title: 'Integrationsplanung für zwei UM980-Module', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'In Arbeit' },
  { id: 'TASK-019F', title: 'Dual-UM980 Failover-Logik Primär/Sekundär umsetzen', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-019G', title: 'Labor- & Feldvalidierung für Dual-UM980', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Offen' },
  { id: 'TASK-025', title: 'NTRIP-Client für Single-Base-Caster implementieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-030', title: 'NTRIP auf neues Modul-System & maintTask migrieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Mittel', status: 'Erledigt' },
  { id: 'TASK-039', title: 'Ethernet-Netzwerkmodus im seriellen Konfigmodus konfigurierbar', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-041', title: 'Konfigurierbare GNSS-Datenausgabe im Konfigmodus', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-042', title: 'NTRIP-Konfigurationspfad funktional & sicher spezifizieren', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Erledigt' },
  { id: 'TASK-043', title: 'Planung: Parametrisierung beider UM980-UARTs', epic: 'EPIC-004', epicName: 'Funktionserweiterung', priority: 'Hoch', status: 'Erledigt' },
]

const PRIORITY_ORDER: Record<TaskPriority, number> = { Hoch: 0, Mittel: 1, Niedrig: 2 }

const PRIORITY_BADGE: Record<TaskPriority, { className: string; icon: React.ReactNode }> = {
  Hoch: { className: 'bg-red-100 text-red-800 dark:bg-red-900/40 dark:text-red-300 border-red-200 dark:border-red-800', icon: <AlertTriangle className="size-3" /> },
  Mittel: { className: 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300 border-amber-200 dark:border-amber-800', icon: <Clock className="size-3" /> },
  Niedrig: { className: 'bg-muted text-muted-foreground', icon: <CircleDot className="size-3" /> },
}

const STATUS_BADGE: Record<TaskStatus, { className: string; icon: React.ReactNode }> = {
  Erledigt: { className: 'bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800', icon: <CheckCircle className="size-3" /> },
  Offen: { className: 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300 border-amber-200 dark:border-amber-800', icon: <Bug className="size-3" /> },
  'In Arbeit': { className: 'bg-blue-100 text-blue-800 dark:bg-blue-900/40 dark:text-blue-300 border-blue-200 dark:border-blue-800', icon: <Loader2 className="size-3" /> },
  Geplant: { className: 'bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300 border-sky-200 dark:border-sky-800', icon: <ClipboardList className="size-3" /> },
}

// ─── GLOBE ICON FIX ───────────────────────────────────────────────────────────

function Globe(props: React.SVGProps<SVGSVGElement> & { className?: string }) {
  return (
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" {...props} className={props.className}>
      <circle cx="12" cy="12" r="10" />
      <path d="M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20" />
      <path d="M2 12h20" />
    </svg>
  )
}

// ─── COMPONENTS ───────────────────────────────────────────────────────────────

function ThemeToggle() {
  const { resolvedTheme, setTheme } = useTheme()

  return (
    <Button
      variant="ghost"
      size="icon"
      className="size-9"
      onClick={() => setTheme(resolvedTheme === 'dark' ? 'light' : 'dark')}
      aria-label="Design wechseln"
      suppressHydrationWarning
    >
      {resolvedTheme === 'dark' ? <Sun className="size-4" /> : <Moon className="size-4" />}
    </Button>
  )
}

// ─── OVERVIEW TAB ─────────────────────────────────────────────────────────────

function OverviewTab() {
  return (
    <div className="space-y-6">
      {/* Quick Stats */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        {[
          { label: 'Module gesamt', value: '15', icon: <Box className="size-5" />, sub: 'Alle aktiv' },
          { label: 'Aktive ADRs', value: '21', icon: <FileText className="size-5" />, sub: '12 Top-Level + 8 Subsystem + ADR-ZAI' },
          { label: 'Backlog-Aufgaben', value: '47', icon: <ListTodo className="size-5" />, sub: '22 erledigt · 24 offen · 1 in Arbeit' },
          { label: 'Build-Status', value: '✓', icon: <CheckCircle className="size-5 text-emerald-500" />, sub: 'RAM 26% · Flash 46%' },
        ].map((stat) => (
          <Card key={stat.label} className="py-4 gap-3">
            <CardContent className="px-4 flex items-center gap-3">
              <div className="rounded-lg bg-muted p-2 shrink-0">
                {stat.icon}
              </div>
              <div className="min-w-0">
                <p className="text-2xl font-bold tracking-tight">{stat.value}</p>
                <p className="text-xs text-muted-foreground truncate">{stat.label}</p>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      {/* Project Info & Architecture */}
      <div className="grid md:grid-cols-2 gap-6">
        {/* Project Card */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Microchip className="size-4 text-muted-foreground" />
              Projektinformationen
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-3 text-sm">
              {[
                ['Board', 'LilyGO T-ETH-Lite-S3'],
                ['Prozessor', 'ESP32-S3 (Dual-Core, 240 MHz)'],
                ['Framework', 'FreeRTOS v10.5.1'],
                ['WiFi/BLE', '802.11 b/g/n + BLE 5.0'],
                ['Ethernet', 'RMII (IEEE 802.3)'],
                ['Flash', '4 MB (QIO)'],
              ].map(([k, v]) => (
                <div key={k} className="flex justify-between items-center">
                  <span className="text-muted-foreground">{k}</span>
                  <span className="font-medium text-right">{v}</span>
                </div>
              ))}
            </div>
            <Separator />
            <div className="space-y-3">
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">RAM Nutzung</span>
                <span className="font-medium">26%</span>
              </div>
              <Progress value={26} className="h-2" />
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">Flash Nutzung</span>
                <span className="font-medium">46%</span>
              </div>
              <Progress value={46} className="h-2" />
            </div>
          </CardContent>
        </Card>

        {/* Architecture Card */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Server className="size-4 text-muted-foreground" />
              Architektur: Zwei-Task-Design
            </CardTitle>
            <CardDescription>
              Zwei-Task-Architektur (ADR-007) — maintTask integriert in task_slow
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="space-y-3">
              {/* Core 1 - task_fast */}
              <div className="rounded-lg border border-emerald-300 dark:border-emerald-700 bg-emerald-50 dark:bg-emerald-950/30 p-4">
                <div className="flex items-center gap-2 mb-2">
                  <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800 text-xs">
                    Core 1
                  </Badge>
                  <span className="font-semibold text-sm">task_fast</span>
                  <Badge variant="outline" className="text-xs ml-auto">Priorität 5</Badge>
                </div>
                <p className="text-xs text-muted-foreground">
                  GNSS → IMU → WAS → STEER → ACTUATOR
                </p>
                <p className="text-xs text-muted-foreground mt-1">
                  Periodisch · 100 Hz · Sensor → Aktuator Pfad
                </p>
              </div>

              {/* Core 0 - task_slow */}
              <div className="rounded-lg border border-amber-300 dark:border-amber-700 bg-amber-50 dark:bg-amber-950/30 p-4">
                <div className="flex items-center gap-2 mb-2">
                  <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800 text-xs">
                    Core 0
                  </Badge>
                  <span className="font-semibold text-sm">task_slow</span>
                  <Badge variant="outline" className="text-xs ml-auto">Priorität 2</Badge>
                </div>
                <p className="text-xs text-muted-foreground">
                  HW-Monitor · WDT Feed · SD Flush · NTRIP Tick · ETH Monitor
                </p>
                <p className="text-xs text-muted-foreground">
                  SharedSlot RTCM → UART · DBG.loop() · CLI Polling
                </p>
                <p className="text-xs text-muted-foreground mt-1">
                  Eventgesteuert · Lifecycle-Owner · Kommunikation & Verwaltung
                </p>
              </div>

              {/* ADR-007: maintTask ENTFALLEN — Verantwortlichkeiten in task_slow integriert */}
              <div className="rounded-lg border border-dashed border-muted-foreground/20 bg-muted/30 p-3">
                <div className="flex items-center gap-2 mb-1">
                  <Badge variant="outline" className="text-[10px] px-1.5 py-0 text-muted-foreground">
                    ADR-007
                  </Badge>
                  <span className="text-xs text-muted-foreground line-through">maintTask</span>
                  <span className="text-[10px] text-muted-foreground">→ task_slow integriert</span>
                </div>
                <p className="text-[10px] text-muted-foreground">
                  SD Flush · NTRIP Tick · ETH Monitor · WDT Feed · SharedSlot RTCM
                </p>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Mode System Card */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <Settings className="size-4 text-muted-foreground" />
            Betriebsmodi (OpMode)
          </CardTitle>
          <CardDescription>
            Zwei-Zustands-Automat: Konfiguration ←→ Arbeit
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="flex flex-col sm:flex-row items-center gap-4 justify-center">
            {/* CONFIG Mode */}
            <div className="rounded-lg border-2 border-dashed border-amber-400 dark:border-amber-600 p-4 flex-1 max-w-xs w-full">
              <div className="flex items-center gap-2 mb-2">
                <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800">
                  CONFIG
                </Badge>
                <Wrench className="size-4 text-amber-600 dark:text-amber-400" />
              </div>
              <p className="text-sm font-medium">Konfigurationsmodus</p>
              <p className="text-xs text-muted-foreground mt-1">
                Web-UI erreichbar · Parameter setzen · Kalibrierung · Firmware-Update
              </p>
            </div>

            {/* Arrow */}
            <div className="flex flex-col items-center gap-1 text-muted-foreground">
              <ArrowRight className="size-5 rotate-90 sm:rotate-0" />
              <span className="text-xs">Übergang</span>
              <ArrowRight className="size-5 rotate-90 sm:rotate-0" />
            </div>

            {/* WORK Mode */}
            <div className="rounded-lg border-2 border-solid border-emerald-400 dark:border-emerald-600 p-4 flex-1 max-w-xs w-full">
              <div className="flex items-center gap-2 mb-2">
                <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800">
                  WORK
                </Badge>
                <Zap className="size-4 text-emerald-600 dark:text-emerald-400" />
              </div>
              <p className="text-sm font-medium">Arbeitsmodus</p>
              <p className="text-xs text-muted-foreground mt-1">
                Autosteuerung aktiv · Echtzeit-Sensorpfad · AgOpenGPS-Datenstream
              </p>
            </div>
          </div>
          <div className="mt-4 text-center">
            <p className="text-xs text-muted-foreground">
              Übergang: CONFIG → WORK bei Safety-Pin HIGH · WORK → CONFIG bei Safety-Pin LOW + Geschwindigkeit &lt; Schwellwert oder CLI `mode setup`
            </p>
          </div>
        </CardContent>
      </Card>

      {/* Recent Activity */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <GitCommit className="size-4 text-muted-foreground" />
            Letzte Aktivitäten
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="space-y-1">
            {COMMITS.map((commit, i) => (
              <div
                key={commit.hash}
                className="flex items-start gap-3 py-3 group hover:bg-muted/50 rounded-lg px-3 -mx-3 transition-colors"
              >
                <div className="mt-0.5 size-2 rounded-full bg-muted-foreground/40 shrink-0 group-hover:bg-primary" />
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2 flex-wrap">
                    <code className="text-xs font-mono text-muted-foreground">{commit.hash}</code>
                    <Badge variant="outline" className="text-[10px] px-1.5 py-0">{commit.phase}</Badge>
                  </div>
                  <p className="text-sm mt-0.5 truncate">{commit.message}</p>
                </div>
                <span className="text-xs text-muted-foreground whitespace-nowrap shrink-0">{commit.date}</span>
              </div>
            ))}
          </div>
        </CardContent>
      </Card>
    </div>
  )
}

// ─── MODULE TAB ───────────────────────────────────────────────────────────────

function ModuleTab() {
  const [search, setSearch] = useState('')
  const [filterCategory, setFilterCategory] = useState<ModuleCategory | 'Alle'>('Alle')
  const [showInactive, setShowInactive] = useState(false)
  const [expandedModule, setExpandedModule] = useState<string | null>(null)

  const filteredModules = useMemo(() => {
    return MODULES.filter((m) => {
      const matchSearch = search === '' ||
        m.name.toLowerCase().includes(search.toLowerCase()) ||
        m.fullName.toLowerCase().includes(search.toLowerCase()) ||
        m.description.toLowerCase().includes(search.toLowerCase())
      const matchCategory = filterCategory === 'Alle' || m.category === filterCategory
      const matchStatus = showInactive || m.status === 'Aktiv'
      return matchSearch && matchCategory && matchStatus
    })
  }, [search, filterCategory, showInactive])

  const categories: (ModuleCategory | 'Alle')[] = ['Alle', 'Core', 'Sensor', 'Kommunikation', 'Sicherheit', 'Werkzeug']

  return (
    <div className="space-y-4">
      {/* Filters */}
      <div className="flex flex-col sm:flex-row gap-3">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 size-4 text-muted-foreground" />
          <Input
            placeholder="Module durchsuchen..."
            className="pl-9"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
          {search && (
            <button
              onClick={() => setSearch('')}
              className="absolute right-3 top-1/2 -translate-y-1/2 text-muted-foreground hover:text-foreground"
            >
              <X className="size-3.5" />
            </button>
          )}
        </div>
        <div className="flex gap-2 flex-wrap">
          {categories.map((cat) => (
            <Button
              key={cat}
              variant={filterCategory === cat ? 'default' : 'outline'}
              size="sm"
              onClick={() => setFilterCategory(cat)}
              className="text-xs"
            >
              {cat}
            </Button>
          ))}
        </div>
      </div>

      {/* Module Grid */}
      <div className="grid sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-3">
        <AnimatePresence mode="popLayout">
          {filteredModules.map((mod) => (
            <motion.div
              key={mod.id}
              layout
              initial={{ opacity: 0, scale: 0.95 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.95 }}
              transition={{ duration: 0.2 }}
            >
              <Card
                className={`cursor-pointer transition-all hover:shadow-md border-l-4 ${CATEGORY_BORDER_COLORS[mod.category]} py-0 gap-0`}
                onClick={() => setExpandedModule(expandedModule === mod.id ? null : mod.id)}
              >
                <CardHeader className="px-4 py-3 gap-1">
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      {mod.icon}
                      <CardTitle className="text-sm font-semibold">{mod.name}</CardTitle>
                    </div>
                    <div className="flex items-center gap-1.5">
                      <Badge variant="outline" className={`text-[10px] px-1.5 py-0 border ${CATEGORY_COLORS[mod.category]}`}>
                        {mod.category}
                      </Badge>
                    </div>
                  </div>
                  <CardDescription className="text-xs">{mod.fullName}</CardDescription>
                </CardHeader>
                <AnimatePresence>
                  {expandedModule === mod.id && (
                    <motion.div
                      initial={{ height: 0, opacity: 0 }}
                      animate={{ height: 'auto', opacity: 1 }}
                      exit={{ height: 0, opacity: 0 }}
                      transition={{ duration: 0.2 }}
                      className="overflow-hidden"
                    >
                      <CardContent className="px-4 pb-3 pt-0 space-y-2">
                        <p className="text-xs text-muted-foreground">{mod.description}</p>
                        <div className="flex items-center justify-between">
                          <span className="text-[10px] text-muted-foreground">Freshness</span>
                          <code className="text-[10px] font-mono">{mod.freshness} ms</code>
                        </div>
                        <div className="flex items-center justify-between">
                          <span className="text-[10px] text-muted-foreground">Abhängigkeiten</span>
                          <div className="flex gap-1">
                            {mod.deps.length > 0 ? mod.deps.map((d) => (
                              <Badge key={d} variant="secondary" className="text-[10px] px-1.5 py-0">{d}</Badge>
                            )) : (
                              <span className="text-[10px] text-muted-foreground">Keine</span>
                            )}
                          </div>
                        </div>
                        <div className="flex items-center justify-between">
                          <span className="text-[10px] text-muted-foreground">Status</span>
                          <Badge variant={mod.status === 'Aktiv' ? 'default' : 'secondary'} className="text-[10px] px-1.5 py-0">
                            {mod.status === 'Aktiv' ? <CheckCircle className="size-2.5" /> : <CircleDot className="size-2.5" />}
                            {mod.status}
                          </Badge>
                        </div>
                      </CardContent>
                    </motion.div>
                  )}
                </AnimatePresence>
                {expandedModule !== mod.id && (
                  <CardContent className="px-4 pb-3 pt-0">
                    <div className="flex items-center justify-between">
                      <code className="text-[10px] font-mono text-muted-foreground">{mod.freshness}ms</code>
                      <div className="flex items-center gap-1">
                        {mod.deps.map((d) => (
                          <span key={d} className="text-[10px] text-muted-foreground">← {d}</span>
                        ))}
                      </div>
                    </div>
                  </CardContent>
                )}
              </Card>
            </motion.div>
          ))}
        </AnimatePresence>
      </div>

      {filteredModules.length === 0 && (
        <div className="text-center py-12 text-muted-foreground">
          <Box className="size-10 mx-auto mb-3 opacity-30" />
          <p className="text-sm">Keine Module gefunden</p>
          <Button variant="link" size="sm" onClick={() => { setSearch(''); setFilterCategory('Alle'); }}>
            Filter zurücksetzen
          </Button>
        </div>
      )}
    </div>
  )
}

// ─── BACKLOG TAB ──────────────────────────────────────────────────────────────

function BacklogTab() {
  const [filterPriority, setFilterPriority] = useState<TaskPriority | 'Alle'>('Alle')
  const [filterStatus, setFilterStatus] = useState<TaskStatus | 'Alle'>('Alle')
  const [filterEpic, setFilterEpic] = useState<string>('Alle')
  const [sortBy, setSortBy] = useState<'priority' | 'id'>('priority')

  const sortedTasks = useMemo(() => {
    return BACKLOG_TASKS
      .filter((t) => {
        if (filterPriority !== 'Alle' && t.priority !== filterPriority) return false
        if (filterStatus !== 'Alle' && t.status !== filterStatus) return false
        if (filterEpic !== 'Alle' && t.epic !== filterEpic) return false
        return true
      })
      .sort((a, b) => {
        if (sortBy === 'priority') {
          const diff = PRIORITY_ORDER[a.priority] - PRIORITY_ORDER[b.priority]
          return diff !== 0 ? diff : a.id.localeCompare(b.id)
        }
        return a.id.localeCompare(b.id)
      })
  }, [filterPriority, filterStatus, filterEpic, sortBy])

  const epics = ['Alle', 'EPIC-001', 'EPIC-002', 'EPIC-003', 'EPIC-004']
  const epicNames: Record<string, string> = {
    'Alle': 'Alle Epics',
    'EPIC-001': 'Laufzeitstabilität',
    'EPIC-002': 'Sensor & Sicherheit',
    'EPIC-003': 'Plattform & Wiederverwendung',
    'EPIC-004': 'Funktionserweiterung',
  }

  const statusCounts = useMemo(() => ({
    Erledigt: BACKLOG_TASKS.filter(t => t.status === 'Erledigt').length,
    Offen: BACKLOG_TASKS.filter(t => t.status === 'Offen').length,
    'In Arbeit': BACKLOG_TASKS.filter(t => t.status === 'In Arbeit').length,
  }), [])

  return (
    <div className="space-y-4">
      {/* Status Summary */}
      <div className="grid grid-cols-3 gap-3">
        {(['Erledigt', 'Offen', 'In Arbeit'] as TaskStatus[]).map((s) => (
          <Card key={s} className="py-0 gap-0">
            <CardContent className="px-4 py-3 flex items-center gap-3">
              {STATUS_BADGE[s].icon}
              <div>
                <p className="text-lg font-bold">{statusCounts[s]}</p>
                <p className="text-xs text-muted-foreground">{s}</p>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      {/* Filters */}
      <div className="flex flex-col sm:flex-row gap-3">
        <div className="flex items-center gap-2 text-xs text-muted-foreground shrink-0">
          <Filter className="size-3.5" />
          <span>Filter:</span>
        </div>
        <div className="flex gap-2 flex-wrap">
          <div className="flex gap-1">
            {(['Alle', 'Hoch', 'Mittel', 'Niedrig'] as const).map((p) => (
              <Button
                key={p}
                variant={filterPriority === p ? 'default' : 'outline'}
                size="sm"
                onClick={() => setFilterPriority(p)}
                className="text-xs h-7"
              >
                {p === 'Alle' ? 'Priorität' : p}
              </Button>
            ))}
          </div>
          <Separator orientation="vertical" className="h-7" />
          <div className="flex gap-1">
            {(['Alle', 'Erledigt', 'Offen', 'In Arbeit'] as const).map((s) => (
              <Button
                key={s}
                variant={filterStatus === s ? 'default' : 'outline'}
                size="sm"
                onClick={() => setFilterStatus(s)}
                className="text-xs h-7"
              >
                {s === 'Alle' ? 'Status' : s}
              </Button>
            ))}
          </div>
          <Separator orientation="vertical" className="h-7" />
          <div className="flex gap-1">
            {epics.map((e) => (
              <Button
                key={e}
                variant={filterEpic === e ? 'default' : 'outline'}
                size="sm"
                onClick={() => setFilterEpic(e)}
                className="text-xs h-7"
              >
                {epicNames[e]}
              </Button>
            ))}
          </div>
        </div>
        <div className="flex items-center gap-2 ml-auto">
          <span className="text-xs text-muted-foreground">Sortierung:</span>
          <Button
            variant={sortBy === 'priority' ? 'default' : 'outline'}
            size="sm"
            onClick={() => setSortBy('priority')}
            className="text-xs h-7"
          >
            Priorität
          </Button>
          <Button
            variant={sortBy === 'id' ? 'default' : 'outline'}
            size="sm"
            onClick={() => setSortBy('id')}
            className="text-xs h-7"
          >
            ID
          </Button>
        </div>
      </div>

      {/* Task Table */}
      <Card className="py-0 gap-0 overflow-hidden">
        <div className="max-h-[500px] overflow-y-auto">
          <Table>
            <TableHeader>
              <TableRow className="bg-muted/50 hover:bg-muted/50">
                <TableHead className="w-[100px] text-xs font-semibold">ID</TableHead>
                <TableHead className="text-xs font-semibold">Aufgabe</TableHead>
                <TableHead className="w-[140px] hidden md:table-cell text-xs font-semibold">Epic</TableHead>
                <TableHead className="w-[90px] text-xs font-semibold">Priorität</TableHead>
                <TableHead className="w-[90px] text-xs font-semibold">Status</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {sortedTasks.map((task) => (
                <TableRow key={task.id} className="cursor-pointer">
                  <TableCell>
                    <code className="text-xs font-mono text-muted-foreground">{task.id}</code>
                  </TableCell>
                  <TableCell>
                    <span className="text-sm">{task.title}</span>
                  </TableCell>
                  <TableCell className="hidden md:table-cell">
                    <div className="flex flex-col">
                      <code className="text-xs font-mono text-muted-foreground">{task.epic}</code>
                      <span className="text-xs text-muted-foreground">{task.epicName}</span>
                    </div>
                  </TableCell>
                  <TableCell>
                    <Badge className={`text-[10px] px-1.5 py-0 border ${PRIORITY_BADGE[task.priority].className}`}>
                      {PRIORITY_BADGE[task.priority].icon}
                      {task.priority}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <Badge className={`text-[10px] px-1.5 py-0 border ${STATUS_BADGE[task.status].className}`}>
                      {STATUS_BADGE[task.status].icon}
                      {task.status}
                    </Badge>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </div>
        {sortedTasks.length === 0 && (
          <div className="text-center py-8 text-muted-foreground text-sm">
            Keine Aufgaben entsprechen den gewählten Filtern
          </div>
        )}
        <div className="border-t px-4 py-2 flex justify-between items-center">
          <span className="text-xs text-muted-foreground">
            {sortedTasks.length} von {BACKLOG_TASKS.length} Aufgaben
          </span>
          <span className="text-xs text-muted-foreground">
            TASK-001 bis TASK-047 · {BACKLOG_TASKS.filter(t => t.status === 'Erledigt').length}/{BACKLOG_TASKS.length} erledigt
          </span>
        </div>
      </Card>
    </div>
  )
}

// ─── DOWNLOADS TAB ────────────────────────────────────────────────────────────

function DownloadsTab() {
  const [downloading, setDownloading] = useState(false)
  const [downloaded, setDownloaded] = useState(false)

  const handleDownload = async () => {
    setDownloading(true)
    setDownloaded(false)
    try {
      const res = await fetch('/api/download')
      if (!res.ok) throw new Error('Download fehlgeschlagen')
      const blob = await res.blob()
      const url = window.URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = 'ZAI_GPS_Firmware_Architecture.pdf'
      document.body.appendChild(a)
      a.click()
      window.URL.revokeObjectURL(url)
      document.body.removeChild(a)
      setDownloaded(true)
    } catch {
      alert('Download fehlgeschlagen. Versuche es nochmal.')
    } finally {
      setDownloading(false)
    }
  }

  return (
    <div className="space-y-6">
      {/* PDF Download */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <Archive className="size-4 text-muted-foreground" />
            Firmware-Architektur PDF
          </CardTitle>
          <CardDescription>
            Umfassende Dokumentation der Firmware-Architektur, Module und Schnittstellen
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="rounded-lg bg-muted/50 p-4 space-y-2">
            {[
              ['Datei', 'ZAI_GPS_Firmware_Architecture.pdf'],
              ['Größe', '149 KB'],
              ['Typ', 'PDF Dokument'],
              ['Stand', 'Phase 3 — Juni 2026'],
            ].map(([k, v]) => (
              <div key={k} className="flex justify-between text-sm">
                <span className="text-muted-foreground">{k}</span>
                <span className={k === 'Datei' ? 'font-mono text-xs' : ''}>{v}</span>
              </div>
            ))}
          </div>
          <Button
            onClick={handleDownload}
            disabled={downloading}
            className="w-full sm:w-auto"
          >
            {downloading ? (
              <>
                <Loader2 className="size-4 animate-spin" />
                Wird heruntergeladen...
              </>
            ) : downloaded ? (
              <>
                <CheckCircle2 className="size-4" />
                Erneut herunterladen
              </>
            ) : (
              <>
                <Download className="size-4" />
                PDF herunterladen
              </>
            )}
          </Button>
          {downloaded && (
            <p className="text-xs text-emerald-600 dark:text-emerald-400">
              ✓ Download erfolgreich gestartet
            </p>
          )}
        </CardContent>
      </Card>

      {/* Architecture Overview */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <LayoutDashboard className="size-4 text-muted-foreground" />
            Architektur-Diagramm
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="rounded-lg border bg-muted/30 p-4">
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4 text-center">
              <div className="space-y-2">
                <div className="rounded-lg border-2 border-dashed border-amber-400 dark:border-amber-600 p-3">
                  <p className="text-xs font-semibold">Input Layer</p>
                  <p className="text-[10px] text-muted-foreground mt-1">GNSS · IMU · WAS</p>
                  <p className="text-[10px] text-muted-foreground">NTRIP · NMEA</p>
                </div>
              </div>
              <div className="space-y-2">
                <div className="rounded-lg border-2 border-solid border-primary p-3">
                  <p className="text-xs font-semibold">Processing Core</p>
                  <p className="text-[10px] text-muted-foreground mt-1">task_fast (100 Hz) · task_slow (variabel)</p>
                  <p className="text-[10px] text-muted-foreground">PID · Kalman · SAFETY · NTRIP · SD</p>
                </div>
              </div>
              <div className="space-y-2">
                <div className="rounded-lg border-2 border-dashed border-emerald-400 dark:border-emerald-600 p-3">
                  <p className="text-xs font-semibold">Output Layer</p>
                  <p className="text-[10px] text-muted-foreground mt-1">ACTUATOR · STEER</p>
                  <p className="text-[10px] text-muted-foreground">ETH · BT · WiFi</p>
                </div>
              </div>
            </div>
            <div className="flex items-center justify-center gap-1 mt-3 text-muted-foreground">
              <ArrowRight className="size-3" />
              <span className="text-[10px]">Datenfluss von links nach rechts</span>
              <ArrowRight className="size-3" />
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Documentation Links */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <FolderDown className="size-4 text-muted-foreground" />
            Dokumentation & Ressourcen
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-2">
          {[
            { name: 'GitHub Repository', desc: 'Quellcode, Issues & Pull Requests', href: 'https://github.com/Keule/Z.AI_WORK', icon: <Github className="size-4" /> },
            { name: 'ADR-007: Zwei-Task-Architektur', desc: 'Aktuelle Task-Aufteilung (ersetzt ADR-002)', href: 'https://github.com/Keule/Z.AI_WORK/blob/main/esp32_project/docs/adr/ADR-007-two-task-architecture.md', icon: <FileText className="size-4" /> },
            { name: 'ADR-ZAI-001: Git/GitHub Workflow', desc: 'Agenten-Workflow Regeln & Backup-Strategie', href: 'https://github.com/Keule/Z.AI_WORK/blob/main/esp32_project/docs/adr/ADR-ZAI-001-git-github-workflow.md', icon: <FileText className="size-4" /> },
            { name: 'AgOpenGPS Dokumentation', desc: 'Externe API-Referenz und Protokollspezifikation', href: 'https://github.com/AgOpenGPS', icon: <ExternalLink className="size-4" /> },
          ].map((link) => (
            <a
              key={link.name}
              href={link.href}
              target="_blank"
              rel="noopener noreferrer"
              className="flex items-center gap-3 rounded-lg border p-3 hover:bg-muted/50 transition-colors group"
            >
              <div className="rounded-md bg-muted p-2 group-hover:bg-background transition-colors">
                {link.icon}
              </div>
              <div className="flex-1 min-w-0">
                <p className="text-sm font-medium">{link.name}</p>
                <p className="text-xs text-muted-foreground truncate">{link.desc}</p>
              </div>
              <ExternalLink className="size-3.5 text-muted-foreground group-hover:text-foreground transition-colors shrink-0" />
            </a>
          ))}
        </CardContent>
      </Card>
    </div>
  )
}

// ─── ARCHITEKTUR TAB ─────────────────────────────────────────────────────────

const ARCH_FADE = {
  initial: { opacity: 0, y: 20 },
  animate: { opacity: 1, y: 0 },
  transition: { duration: 0.4 },
} as const

function ArchLayer({ title, children, color }: { title: string; children: React.ReactNode; color: string }) {
  return (
    <motion.div {...ARCH_FADE} className={`rounded-lg border-2 p-4 transition-shadow hover:shadow-md ${color}`}>
      <p className="text-xs font-bold uppercase tracking-wider mb-2 opacity-70">{title}</p>
      {children}
    </motion.div>
  )
}

function ArchConnector({ direction = 'down' }: { direction?: 'down' | 'up' }) {
  return (
    <div className="flex justify-center py-1">
      <div className="flex flex-col items-center gap-0.5 text-muted-foreground/50">
        <div className="w-px h-3 bg-current" />
        <span className="text-sm leading-none">{direction === 'down' ? '↓' : '↑'}</span>
        <div className="w-px h-3 bg-current" />
      </div>
    </div>
  )
}

function ArchitectureTab() {
  const moduleCategories: { cat: ModuleCategory; modules: string[]; color: string }[] = [
    { cat: 'Core', modules: ['ACTUATOR', 'STEER', 'SPI', 'SPI_SHARED'], color: CATEGORY_COLORS.Core },
    { cat: 'Sensor', modules: ['GNSS', 'IMU', 'WAS'], color: CATEGORY_COLORS.Sensor },
    { cat: 'Kommunikation', modules: ['ETH', 'WIFI', 'BT', 'NETWORK', 'NTRIP'], color: CATEGORY_COLORS.Kommunikation },
    { cat: 'Sicherheit', modules: ['SAFETY'], color: CATEGORY_COLORS.Sicherheit },
    { cat: 'Werkzeug', modules: ['LOGGING', 'OTA', 'REMOTE_CONSOLE'], color: CATEGORY_COLORS.Werkzeug },
  ]

  return (
    <div className="space-y-6">
      {/* ═══ Section A: System-Overview Block Diagram ═══ */}
      <motion.div {...ARCH_FADE}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Layers className="size-4 text-muted-foreground" />
              System-Übersicht — Schichtenarchitektur
            </CardTitle>
            <CardDescription>
              Top-to-Bottom Layered Architecture des ESP32-S3 Firmware-Stacks
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-0">
            {/* Layer 1: External Interface */}
            <ArchLayer
              title="Externe Schnittstellen"
              color="border-rose-300 dark:border-rose-700 bg-rose-50 dark:bg-rose-950/20"
            >
              <div className="flex flex-wrap gap-2 justify-center">
                {[
                  { label: 'AgOpenGPS', icon: <Radio className="size-3.5" /> },
                  { label: 'Web-UI', icon: <MonitorSmartphone className="size-3.5" /> },
                  { label: 'NTRIP Caster', icon: <RadioTower className="size-3.5" /> },
                  { label: 'SD-Karte', icon: <Database className="size-3.5" /> },
                  { label: 'Remote Console', icon: <Cable className="size-3.5" /> },
                ].map((item) => (
                  <span key={item.label} className="inline-flex items-center gap-1.5 rounded-md border border-rose-200 dark:border-rose-800 bg-rose-100 dark:bg-rose-900/30 px-2.5 py-1 text-[11px] font-medium text-rose-800 dark:text-rose-300">
                    {item.icon}
                    {item.label}
                  </span>
                ))}
              </div>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 2: Protocol */}
            <ArchLayer
              title="Protokoll-Schicht"
              color="border-amber-300 dark:border-amber-700 bg-amber-50 dark:bg-amber-950/20"
            >
              <div className="flex flex-wrap gap-2 justify-center">
                {['PGN-Codec', 'NMEA-Parser', 'AOG-UDP', 'NTRIP-Client'].map((p) => (
                  <span key={p} className="rounded-md border border-amber-200 dark:border-amber-800 bg-amber-100 dark:bg-amber-900/30 px-2.5 py-1 text-[11px] font-mono font-medium text-amber-800 dark:text-amber-300">
                    {p}
                  </span>
                ))}
              </div>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 3: Module System (Center) */}
            <ArchLayer
              title="Modul-System — 15 Module"
              color="border-primary/40 bg-primary/[0.03] dark:bg-primary/[0.06]"
            >
              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
                {moduleCategories.map(({ cat, modules, color }) => (
                  <div key={cat} className="space-y-1.5">
                    <Badge className={`text-[10px] border ${color}`}>{cat}</Badge>
                    <div className="flex flex-wrap gap-1">
                      {modules.map((m) => (
                        <span key={m} className="rounded border border-border bg-background px-2 py-0.5 text-[10px] font-mono font-semibold">
                          {m}
                        </span>
                      ))}
                    </div>
                  </div>
                ))}
              </div>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 4: FreeRTOS */}
            <ArchLayer
              title="FreeRTOS — Zwei-Task-Schicht (ADR-007)"
              color="border-emerald-300 dark:border-emerald-700 bg-emerald-50 dark:bg-emerald-950/20"
            >
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                <div className="rounded-md border border-emerald-200 dark:border-emerald-800 bg-emerald-100 dark:bg-emerald-900/30 p-3 text-center">
                  <p className="text-xs font-bold">task_fast</p>
                  <p className="text-[10px] text-muted-foreground mt-1">Core 1 (exklusiv) · Priorität 5</p>
                  <Badge variant="outline" className="mt-1.5 text-[9px]">100 Hz · 10 ms · 4096 B Stack</Badge>
                </div>
                <div className="rounded-md border border-emerald-200 dark:border-emerald-800 bg-emerald-100 dark:bg-emerald-900/30 p-3 text-center">
                  <p className="text-xs font-bold">task_slow</p>
                  <p className="text-[10px] text-muted-foreground mt-1">Core 0 · Priorität 2 · Lifecycle-Owner</p>
                  <Badge variant="outline" className="mt-1.5 text-[9px]">Event-driven · 8192 B Stack</Badge>
                </div>
              </div>
              <p className="text-[9px] text-muted-foreground mt-2 text-center">maintTask ENTFALLEN (ADR-007) — Aufgaben in task_slow integriert</p>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 5: Shared State */}
            <ArchLayer
              title="Shared-State-Schicht"
              color="border-sky-300 dark:border-sky-700 bg-sky-50 dark:bg-sky-950/20"
            >
              <div className="flex flex-wrap gap-2 justify-center">
                {[
                  { label: 'SharedSlot<T>', sub: 'Lock-free Producer/Consumer' },
                  { label: 'GlobalState', sub: 'Zentraler System-Zustand' },
                  { label: 'StateLock', sub: 'Mutex-geschützter Zugriff' },
                ].map((s) => (
                  <span key={s.label} className="rounded-md border border-sky-200 dark:border-sky-800 bg-sky-100 dark:bg-sky-900/30 px-3 py-1.5 text-[11px]">
                    <span className="font-mono font-semibold text-sky-800 dark:text-sky-300">{s.label}</span>
                    <span className="block text-[9px] text-muted-foreground mt-0.5">{s.sub}</span>
                  </span>
                ))}
              </div>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 6: HAL */}
            <ArchLayer
              title="Hardware-Abstraktion (HAL)"
              color="border-violet-300 dark:border-violet-700 bg-violet-50 dark:bg-violet-950/20"
            >
              <div className="flex flex-wrap gap-1.5 justify-center">
                {['hal_gpio', 'hal_spi', 'hal_eth', 'hal_uart', 'hal_i2c', 'hal_ads1118', 'hal_bno085', 'hal_logging'].map((h) => (
                  <span key={h} className="rounded border border-violet-200 dark:border-violet-800 bg-violet-100 dark:bg-violet-900/30 px-2 py-0.5 text-[10px] font-mono text-violet-800 dark:text-violet-300">
                    {h}
                  </span>
                ))}
              </div>
            </ArchLayer>

            <ArchConnector />

            {/* Layer 7: Hardware */}
            <ArchLayer
              title="Hardware-Schicht"
              color="border-zinc-300 dark:border-zinc-600 bg-zinc-50 dark:bg-zinc-900/30"
            >
              <div className="flex flex-wrap gap-1.5 justify-center">
                {[
                  { label: 'ESP32-S3', icon: <Cpu className="size-3" /> },
                  { label: 'RMII ETH PHY', icon: <EthernetPort className="size-3" /> },
                  { label: 'SPI Bus', icon: <Cable className="size-3" /> },
                  { label: 'UART 0/1/2', icon: <Cable className="size-3" /> },
                  { label: 'I2C', icon: <Cable className="size-3" /> },
                  { label: 'GPIO', icon: <CircuitBoard className="size-3" /> },
                  { label: 'PSRAM', icon: <HardDrive className="size-3" /> },
                  { label: 'SD-Card', icon: <Database className="size-3" /> },
                ].map((h) => (
                  <span key={h.label} className="inline-flex items-center gap-1 rounded border border-zinc-200 dark:border-zinc-700 bg-zinc-100 dark:bg-zinc-800/50 px-2 py-0.5 text-[10px] font-medium text-zinc-700 dark:text-zinc-300">
                    {h.icon}
                    {h.label}
                  </span>
                ))}
              </div>
            </ArchLayer>

            {/* Data Flow Legend */}
            <div className="mt-4 flex flex-wrap gap-4 justify-center text-[10px] text-muted-foreground">
              <span className="flex items-center gap-1"><span>↓</span> Datenfluss nach unten</span>
              <span className="flex items-center gap-1"><span>↑</span> Rückmeldung nach oben</span>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* ═══ Section B: Task-Architecture Detail ═══ */}
      <motion.div {...ARCH_FADE} transition={{ duration: 0.4, delay: 0.1 }}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Cpu className="size-4 text-muted-foreground" />
              Task-Architektur — Detaillansicht
            </CardTitle>
            <CardDescription>
              Zwei-Task-Design mit deterministischer Echtzeitverarbeitung
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-6">
            {/* task_fast Pipeline */}
            <div>
              <div className="flex items-center gap-2 mb-3">
                <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800">
                  task_fast · Core 1
                </Badge>
                <span className="text-xs text-muted-foreground">100 Hz · 10 ms Zyklus · Periodisch</span>
              </div>
              <div className="flex flex-wrap items-center gap-1.5">
                {[
                  { label: 'GNSS', color: CATEGORY_COLORS.Sensor },
                  { label: 'IMU', color: CATEGORY_COLORS.Sensor },
                  { label: 'WAS', color: CATEGORY_COLORS.Sensor },
                  { label: 'SAFETY', color: CATEGORY_COLORS.Sicherheit },
                  { label: 'STEER', color: CATEGORY_COLORS.Core },
                  { label: 'ACTUATOR', color: CATEGORY_COLORS.Core },
                ].map((step, i) => (
                  <div key={step.label} className="flex items-center gap-1.5">
                    <span className={`rounded-md border px-2.5 py-1.5 text-[11px] font-bold font-mono ${step.color}`}>
                      {step.label}
                    </span>
                    {i < 5 && (
                      <ArrowRight className="size-3.5 text-emerald-400 shrink-0" />
                    )}
                  </div>
                ))}
              </div>
              <p className="text-[10px] text-muted-foreground mt-2">
                Pipeline: Sensor-Input → Sicherheitsprüfung → Regelung → Aktuator-Output (nur in WORK)
              </p>
            </div>

            <Separator />

            {/* task_slow */}
            <div>
              <div className="flex items-center gap-2 mb-3">
                <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800">
                  task_slow · Core 0
                </Badge>
                <span className="text-xs text-muted-foreground">Lifecycle-Owner · Kommunikation & Verwaltung</span>
              </div>
              <div className="grid grid-cols-2 sm:grid-cols-4 gap-2">
                {[
                  { label: 'HW-Monitor', timing: '~1 Hz' },
                  { label: 'WDT Feed', timing: '5 s Timeout' },
                  { label: 'SD Flush', timing: 'Alle 2 s' },
                  { label: 'NTRIP Tick', timing: 'Alle 1 s' },
                  { label: 'ETH Monitor', timing: 'Kontinuierlich' },
                  { label: 'GPIO Toggle', timing: '500 ms Debounce' },
                  { label: 'DBG.loop()', timing: 'TCP Console' },
                  { label: 'CLI Polling', timing: 'Serial/TCP' },
                ].map((t) => (
                  <div key={t.label} className="rounded-md border border-border bg-muted/50 p-2.5 text-center">
                    <p className="text-[11px] font-semibold">{t.label}</p>
                    <p className="text-[9px] text-muted-foreground mt-0.5">{t.timing}</p>
                  </div>
                ))}
              </div>
              <p className="text-[10px] text-muted-foreground mt-2">
                SharedSlot RTCM → UART · Setup Wizard (CONFIG) · Sub-Task Management
              </p>
            </div>

            <Separator />

            {/* SharedSlot Connection */}
            <div>
              <p className="text-xs font-semibold mb-2 flex items-center gap-1.5">
                <Fingerprint className="size-3.5" />
                Inter-Task-Kommunikation via SharedSlot&lt;T&gt; (ADR-007 §4)
              </p>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                <div className="rounded-md border border-sky-200 dark:border-sky-800 bg-sky-50 dark:bg-sky-950/20 p-3">
                  <p className="text-[11px] font-semibold text-sky-800 dark:text-sky-300">Producer: task_slow (NTRIP)</p>
                  <p className="text-[10px] text-muted-foreground mt-1">
                    <code className="text-[10px] font-mono">SharedSlot&lt;RtcmChunk&gt;</code> — NTRIP TCP → 512 B Chunk → task_fast liest für UART-Forwarding
                  </p>
                </div>
                <div className="rounded-md border border-sky-200 dark:border-sky-800 bg-sky-50 dark:bg-sky-950/20 p-3">
                  <p className="text-[11px] font-semibold text-sky-800 dark:text-sky-300">Zugriff: StateLock-geschützt (ADR-STATE-001)</p>
                  <p className="text-[10px] text-muted-foreground mt-1">
                    <code className="text-[10px] font-mono">dirty</code>-Flag + <code className="text-[10px] font-mono">last_update_ms</code> für Freshness-Check · Alle Cross-Task-Zugriffe unter Mutex
                  </p>
                </div>
              </div>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* ═══ Section C: Modul-Abhängigkeitsgraph ═══ */}
      <motion.div {...ARCH_FADE} transition={{ duration: 0.4, delay: 0.2 }}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Network className="size-4 text-muted-foreground" />
              Modul-Abhängigkeitsgraph
            </CardTitle>
            <CardDescription>
              Compilerungs- und Laufzeitabhängigkeiten zwischen Modulen
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="space-y-4">
              {/* Dependency Rows */}
              {[
                { from: 'STEER', to: ['WAS', 'ACTUATOR', 'IMU', 'SAFETY'], color: CATEGORY_COLORS.Core },
                { from: 'NTRIP', to: ['NETWORK'], color: CATEGORY_COLORS.Kommunikation },
                { from: 'NETWORK', to: ['ETH', 'WIFI'], color: CATEGORY_COLORS.Kommunikation },
                { from: 'REMOTE_CONSOLE', to: ['NETWORK', 'BT'], color: CATEGORY_COLORS.Werkzeug },
                { from: 'OTA', to: ['NETWORK'], color: CATEGORY_COLORS.Werkzeug },
                { from: 'SPI_SHARED', to: ['SPI'], color: CATEGORY_COLORS.Core },
                { from: 'ACTUATOR', to: ['STEER'], color: CATEGORY_COLORS.Core, note: 'Ready-Check (bidirektional)' },
              ].map((dep) => (
                <div key={dep.from} className="flex flex-wrap items-center gap-2">
                  <span className={`rounded-md border px-3 py-1.5 text-[11px] font-bold font-mono min-w-[100px] text-center ${dep.color}`}>
                    {dep.from}
                  </span>
                  <div className="flex flex-col items-center text-muted-foreground">
                    <ArrowRight className="size-3" />
                  </div>
                  <span className="text-[10px] text-muted-foreground">abhängig von</span>
                  <div className="flex flex-col items-center text-muted-foreground">
                    <ArrowRight className="size-3" />
                  </div>
                  <div className="flex flex-wrap gap-1.5">
                    {dep.to.map((t) => {
                      const targetModule = MODULES.find(m => m.id === t)
                      const tColor = targetModule ? CATEGORY_COLORS[targetModule.category] : 'bg-muted border-border text-muted-foreground'
                      return (
                        <span key={t} className={`rounded-md border px-2.5 py-1.5 text-[11px] font-bold font-mono ${tColor}`}>
                          {t}
                        </span>
                      )
                    })}
                  </div>
                  {dep.note && (
                    <span className="text-[9px] text-muted-foreground italic ml-1">({dep.note})</span>
                  )}
                </div>
              ))}

              <Separator />

              <p className="text-[10px] text-muted-foreground">
                STEER ↔ ACTUATOR: Bidirektionale Abhängigkeit — STEER steuert ACTUATOR, ACTUATOR meldet Ready-Status an STEER.
                Alle Abhängigkeiten werden zur Compile-Zeit geprüft und zur Laufzeit via Freshness-Monitoring überwacht.
              </p>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* ═══ Section D: State-Management ═══ */}
      <motion.div {...ARCH_FADE} transition={{ duration: 0.4, delay: 0.3 }}>
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base">
              <Settings className="size-4 text-muted-foreground" />
              State-Management — Betriebsmodi
            </CardTitle>
            <CardDescription>
              CONFIG ↔ WORK Übergang mit Sicherheits Guards
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-5">
            {/* Mode Diagram */}
            <div className="flex flex-col sm:flex-row items-stretch gap-4">
              {/* CONFIG Mode */}
              <div className="flex-1 rounded-lg border-2 border-dashed border-amber-400 dark:border-amber-600 bg-amber-50 dark:bg-amber-950/20 p-4">
                <div className="flex items-center gap-2 mb-3">
                  <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800">
                    CONFIG
                  </Badge>
                  <Wrench className="size-3.5 text-amber-600 dark:text-amber-400" />\n                </div>
                <div className="space-y-1.5 text-[11px]">
                  <p className="font-semibold">Konfigurationsmodus</p>
                  <div className="space-y-1">
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-amber-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground"><code className="text-[10px] font-mono">g_nav.sw.paused = true</code></span>
                    </div>
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-amber-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground">Modul-Pipeline gestoppt</span>
                    </div>
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-amber-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground">Web-UI & CLI aktiv</span>
                    </div>
                  </div>
                </div>
              </div>

              {/* Transition Arrow */}
              <div className="flex flex-col items-center justify-center gap-1 px-2">
                <ArrowRight className="size-5 text-muted-foreground rotate-90 sm:rotate-0" />
                <span className="text-[9px] text-muted-foreground font-medium">Übergang</span>
                <ArrowLeftRight className="size-4 text-muted-foreground" />
                <span className="text-[9px] text-muted-foreground font-medium">bidirektional</span>
                <ArrowRight className="size-5 text-muted-foreground rotate-90 sm:rotate-0" />
              </div>

              {/* WORK Mode */}
              <div className="flex-1 rounded-lg border-2 border-solid border-emerald-400 dark:border-emerald-600 bg-emerald-50 dark:bg-emerald-950/20 p-4">
                <div className="flex items-center gap-2 mb-3">
                  <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800">
                    WORK
                  </Badge>
                  <Zap className="size-3.5 text-emerald-600 dark:text-emerald-400" />\n                </div>
                <div className="space-y-1.5 text-[11px]">
                  <p className="font-semibold">Arbeitsmodus</p>
                  <div className="space-y-1">
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-emerald-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground"><code className="text-[10px] font-mono">g_nav.sw.paused = false</code></span>
                    </div>
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-emerald-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground">task_fast Pipeline aktiv (100 Hz)</span>
                    </div>
                    <div className="flex items-start gap-1.5">
                      <CheckCircle className="size-3 text-emerald-500 mt-0.5 shrink-0" />
                      <span className="text-muted-foreground">AgOpenGPS Datenstream aktiv</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>

            <Separator />

            {/* Transition Guards */}
            <div>
              <p className="text-xs font-semibold mb-2 flex items-center gap-1.5">
                <Shield className="size-3.5" />
                Übergangs-Guards: <code className="font-mono text-[10px]">modeSet(WORK)</code>
              </p>
              <div className="rounded-lg border border-red-200 dark:border-red-800 bg-red-50 dark:bg-red-950/20 p-3">
                <p className="text-[11px] text-red-800 dark:text-red-300 font-semibold mb-1.5">
                  Folgende 5 Module müssen aktiv sein:
                </p>
                <div className="flex flex-wrap gap-1.5">
                  {[
                    { label: 'IMU', color: CATEGORY_COLORS.Sensor },
                    { label: 'WAS', color: CATEGORY_COLORS.Sensor },
                    { label: 'ACTUATOR', color: CATEGORY_COLORS.Core },
                    { label: 'SAFETY', color: CATEGORY_COLORS.Sicherheit },
                    { label: 'STEER', color: CATEGORY_COLORS.Core },
                  ].map((m) => (
                    <span key={m.label} className={`rounded-md border px-2 py-1 text-[10px] font-bold font-mono ${m.color}`}>
                      {m.label}
                    </span>
                  ))}
                </div>
                <p className="text-[10px] text-muted-foreground mt-2">
                  Falls ein Modul seine Freshness verliert → Automatischer Übergang zurück zu CONFIG (Fail-Safe)
                </p>
              </div>
            </div>
          </CardContent>
        </Card>
      </motion.div>

      {/* ═══ Layer Legend ═══ */}
      <motion.div {...ARCH_FADE} transition={{ duration: 0.4, delay: 0.4 }}>
        <Card>
          <CardContent className="py-4">
            <p className="text-xs font-semibold mb-3 text-muted-foreground">Schichten-Legende</p>
            <div className="grid grid-cols-2 sm:grid-cols-4 lg:grid-cols-7 gap-2">
              {[
                { label: 'Externe Schnittstellen', color: 'bg-rose-100 dark:bg-rose-900/30 border-rose-300 dark:border-rose-700' },
                { label: 'Protokolle', color: 'bg-amber-100 dark:bg-amber-900/30 border-amber-300 dark:border-amber-700' },
                { label: 'Modul-System', color: 'bg-primary/10 border-primary/40' },
                { label: 'FreeRTOS Tasks', color: 'bg-emerald-100 dark:bg-emerald-900/30 border-emerald-300 dark:border-emerald-700' },
                { label: 'Shared State', color: 'bg-sky-100 dark:bg-sky-900/30 border-sky-300 dark:border-sky-700' },
                { label: 'HAL', color: 'bg-violet-100 dark:bg-violet-900/30 border-violet-300 dark:border-violet-700' },
                { label: 'Hardware', color: 'bg-zinc-100 dark:bg-zinc-800/50 border-zinc-300 dark:border-zinc-600' },
              ].map((l) => (
                <div key={l.label} className={`rounded-md border p-2 text-center ${l.color}`}>
                  <p className="text-[9px] font-semibold leading-tight">{l.label}</p>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>
      </motion.div>
    </div>
  )
}

// ─── MAIN PAGE ────────────────────────────────────────────────────────────────

export default function Home() {
  return (
    <div className="min-h-screen flex flex-col bg-background">
      {/* Sticky Header */}
      <header className="sticky top-0 z-50 w-full border-b bg-background/80 backdrop-blur-md supports-[backdrop-filter]:bg-background/60">
        <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8">
          <div className="flex h-14 items-center justify-between">
            {/* Logo */}
            <div className="flex items-center gap-3">
              <div className="flex items-center gap-2">
                <div className="size-7 rounded-md bg-primary flex items-center justify-center">
                  <Activity className="size-4 text-primary-foreground" />
                </div>
                <div>
                  <h1 className="text-sm font-bold tracking-tight leading-none">ZAI_GPS</h1>
                  <p className="text-[10px] text-muted-foreground leading-none">ESP32-S3 Firmware</p>
                </div>
              </div>
            </div>

            {/* Right Actions */}
            <div className="flex items-center gap-2">
              <a
                href="https://github.com/Keule/Z.AI_WORK"
                target="_blank"
                rel="noopener noreferrer"
                className="inline-flex items-center justify-center size-9 rounded-md hover:bg-accent transition-colors"
                aria-label="GitHub"
              >
                <Github className="size-4" />
              </a>
              <ThemeToggle />
            </div>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="flex-1 mx-auto w-full max-w-7xl px-4 sm:px-6 lg:px-8 py-6">
        <Tabs defaultValue="uebersicht" className="w-full">
          <TabsList className="mb-6 w-full sm:w-auto overflow-x-auto">
            <TabsTrigger value="uebersicht" className="gap-1.5">
              <LayoutDashboard className="size-3.5" />
              <span className="hidden sm:inline">Übersicht</span>
              <span className="sm:hidden">Übers.</span>
            </TabsTrigger>
            <TabsTrigger value="module" className="gap-1.5">
              <Package className="size-3.5" />
              Module
            </TabsTrigger>
            <TabsTrigger value="backlog" className="gap-1.5">
              <ListTodo className="size-3.5" />
              Backlog
            </TabsTrigger>
            <TabsTrigger value="downloads" className="gap-1.5">
              <FolderDown className="size-3.5" />
              <span className="hidden sm:inline">Downloads</span>
              <span className="sm:hidden">DL</span>
            </TabsTrigger>
            <TabsTrigger value="architektur" className="gap-1.5">
              <CircuitBoard className="size-3.5" />
              <span className="hidden sm:inline">Architektur</span>
              <span className="sm:hidden">Arch.</span>
            </TabsTrigger>
          </TabsList>

          <TabsContent value="uebersicht">
            <OverviewTab />
          </TabsContent>

          <TabsContent value="module">
            <ModuleTab />
          </TabsContent>

          <TabsContent value="backlog">
            <BacklogTab />
          </TabsContent>

          <TabsContent value="downloads">
            <DownloadsTab />
          </TabsContent>

          <TabsContent value="architektur">
            <ArchitectureTab />
          </TabsContent>
        </Tabs>
      </main>

      {/* Footer */}
      <footer className="border-t mt-auto">
        <div className="mx-auto max-w-7xl px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex flex-col sm:flex-row items-center justify-between gap-2">
            <p className="text-xs text-muted-foreground">
              ESP32-S3 · AgOpenGPS · LilyGO T-ETH-Lite-S3 · FreeRTOS v10.5.1
            </p>
            <p className="text-xs text-muted-foreground">
              Build <code className="font-mono text-[10px]">2025.06.14</code> ·{' '}
              <a
                href="https://github.com/Keule/Z.AI_WORK"
                target="_blank"
                rel="noopener noreferrer"
                className="hover:text-foreground transition-colors inline-flex items-center gap-1"
              >
                GitHub <ExternalLink className="size-2.5" />
              </a>
            </p>
          </div>
        </div>
      </footer>
    </div>
  )
}
