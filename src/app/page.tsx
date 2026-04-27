'use client'

import { useState, useMemo, useEffect, useCallback } from 'react'
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
  Fingerprint, HardDrive, RadioTower, EthernetPort,
  Sparkles, Copy, ClipboardCopy, Eye, Trash2, Send, RefreshCw
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

// ─── AI REQUEST TYPES ──────────────────────────────────────────────────────────

type AIRequestType = 'issue_request' | 'adr_request' | 'design_request' | 'task_request'
type AIContextType = 'module' | 'backlog_task' | 'architecture'

interface AIRequestDraft {
  id: string
  type: AIRequestType
  contextType: AIContextType
  contextId: string
  contextLabel: string
  userNote: string
  createdAt: string
  markdown: string
}

interface AIRequestGeneratorState {
  type: AIRequestType
  contextType: AIContextType
  contextId: string
  contextLabel: string
  userNote: string
}

// ─── DYNAMIC DATA TYPES ─────────────────────────────────────────────────────────

type ModuleCategory = 'Core' | 'Sensor' | 'Kommunikation' | 'Sicherheit' | 'Werkzeug'

interface ModuleData {
  id: string
  name: string
  fullName: string
  category: ModuleCategory
  description: string
  freshness: number
  deps: string[]
  featureFlag: string
  configKeys: string[]
  errorCodes: string[]
  source: { headerExists: boolean; sourceExists: boolean; headerSize: number; sourceSize: number; headerModified: string | null; sourceModified: string | null }
}

interface BacklogTaskData {
  id: string
  title: string
  file: string
  epic: string
  epicName: string
  epicNameDe: string
  priority: string
  priorityRaw: string
  status: string
  statusRaw: string
  deliveryMode: string
  taskCategory: string
  taskCategoryDe: string
  dependencies: string[]
}

interface CommitData {
  hash: string
  fullHash: string
  message: string
  date: string
  phase: string
}

// Global event-based communication between tabs
let pendingAIRequest: { contextType: AIContextType; contextId: string; contextLabel: string } | null = null

// ─── CONSTANTS (unchanged) ────────────────────────────────────────────────────

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

const MODULE_ICONS: Record<string, React.ReactNode> = {
  ETH: <Network className="size-4" />,
  WIFI: <Wifi className="size-4" />,
  BT: <Bluetooth className="size-4" />,
  NETWORK: <Network className="size-4" />,
  GNSS: <Satellite className="size-4" />,
  NTRIP: <MapPin className="size-4" />,
  IMU: <Compass className="size-4" />,
  WAS: <Gauge className="size-4" />,
  ACTUATOR: <ArrowLeftRight className="size-4" />,
  SAFETY: <Shield className="size-4" />,
  STEER: <ArrowRight className="size-4" />,
  LOGGING: <FileText className="size-4" />,
  OTA: <Download className="size-4" />,
  SPI: <Cable className="size-4" />,
  SPI_SHARED: <Cable className="size-4" />,
  REMOTE_CONSOLE: <MonitorSmartphone className="size-4" />,
}

type TaskPriority = 'Hoch' | 'Mittel' | 'Niedrig'
type TaskStatus = 'Erledigt' | 'Offen' | 'In Arbeit' | 'Geplant'

const PRIORITY_ORDER: Record<TaskPriority, number> = { Hoch: 0, Mittel: 1, Niedrig: 2 }

const PRIORITY_BADGE: Record<string, { className: string; icon: React.ReactNode }> = {
  Hoch: { className: 'bg-red-100 text-red-800 dark:bg-red-900/40 dark:text-red-300 border-red-200 dark:border-red-800', icon: <AlertTriangle className="size-3" /> },
  Mittel: { className: 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300 border-amber-200 dark:border-amber-800', icon: <Clock className="size-3" /> },
  Niedrig: { className: 'bg-muted text-muted-foreground', icon: <CircleDot className="size-3" /> },
}

const STATUS_BADGE: Record<string, { className: string; icon: React.ReactNode }> = {
  Erledigt: { className: 'bg-emerald-100 text-emerald-800 dark:bg-emerald-900/40 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800', icon: <CheckCircle className="size-3" /> },
  Offen: { className: 'bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300 border-amber-200 dark:border-amber-800', icon: <Bug className="size-3" /> },
  'In Arbeit': { className: 'bg-blue-100 text-blue-800 dark:bg-blue-900/40 dark:text-blue-300 border-blue-200 dark:border-blue-800', icon: <Loader2 className="size-3" /> },
  Geplant: { className: 'bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300 border-sky-200 dark:border-sky-800', icon: <ClipboardList className="size-3" /> },
}

const AI_REQUEST_TYPES: { value: AIRequestType; label: string; description: string }[] = [
  { value: 'issue_request', label: 'Issue Request', description: 'Problem, Lösung, Acceptance Criteria' },
  { value: 'adr_request', label: 'ADR Request', description: 'Architektur-Entscheidung dokumentieren' },
  { value: 'design_request', label: 'Design Request', description: 'UI/UX, Datenmodell, Dateistruktur' },
  { value: 'task_request', label: 'Task Request', description: 'Implementierungsaufgabe planen' },
]

const AI_REQUEST_TYPE_OUTPUTS: Record<AIRequestType, string[]> = {
  issue_request: ['GitHub Issue Titel', 'Problemstellung', 'Vorgeschlagene Lösung', 'Betroffene Module/Dateien', 'Akzeptanzkriterien', 'Test-Strategie', 'Risiken', 'Implementierungsschritte'],
  adr_request: ['ADR Titel', 'Entscheidungskontext', 'Betrachtete Optionen', 'Trade-offs', 'Empfohlene Entscheidung', 'Konsequenzen', 'Follow-up Tasks'],
  design_request: ['Design-Zusammenfassung', 'User Flow', 'UI/Komponenten', 'Datenmodell', 'Datei-/Ordnerstruktur', 'Risiken', 'Implementierungsaufgaben'],
  task_request: ['Task Titel', 'Ziel', 'Nicht-Ziele', 'Wahrscheinlich betroffene Dateien/Module', 'Implementierungsschritte', 'Akzeptanzkriterien', 'Test-Kommandos', 'Merge-Konflikt-Risiken'],
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

// ─── DATA FETCHING HOOKS ──────────────────────────────────────────────────────

function useModules() {
  const [modules, setModules] = useState<ModuleData[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const fetchModules = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const res = await fetch('/api/modules')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      setModules(data.modules || [])
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load modules')
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchModules() }, [fetchModules])

  return { modules, loading, error, refetch: fetchModules }
}

function useBacklog() {
  const [tasks, setTasks] = useState<BacklogTaskData[]>([])
  const [epics, setEpics] = useState<{ id: string; title: string; taskIds: string[]; taskCount: number; taskCountDone: number; taskCountOpen: number }[]>([])
  const [stats, setStats] = useState<{ total: number; done: number; open: number; inProgress: number; blocked: number; byPriority: { hoch: number; mittel: number; niedrig: number } } | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const fetchBacklog = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const res = await fetch('/api/backlog')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      setTasks(data.tasks || [])
      setEpics(data.epics || [])
      setStats(data.stats || null)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load backlog')
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchBacklog() }, [fetchBacklog])

  return { tasks, epics, stats, loading, error, refetch: fetchBacklog }
}

function useCommits() {
  const [commits, setCommits] = useState<CommitData[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const fetchCommits = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const res = await fetch('/api/commits')
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      setCommits(data.commits || [])
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load commits')
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => { fetchCommits() }, [fetchCommits])

  return { commits, loading, error, refetch: fetchCommits }
}

// ─── AI REQUEST GENERATION ────────────────────────────────────────────────────

function generateAIRequestMarkdown(state: AIRequestGeneratorState, modules: ModuleData[], tasks: BacklogTaskData[]): string {
  const today = new Date().toISOString().split('T')[0]
  const isoNow = new Date().toISOString()
  const contextSlug = state.contextId.toLowerCase().replace(/[^a-z0-9]+/g, '_')
  const typeSlug = state.type.replace('_request', '')
  const autoId = `AI-${today.replace(/-/g, '')}-${typeSlug}-${contextSlug}`

  let contextSection = ''
  if (state.contextType === 'module') {
    const mod = modules.find(m => m.id === state.contextId)
    if (mod) {
      // Find related tasks by keyword matching on title
      const relatedTasks = tasks.filter(t => {
        const title = t.title.toLowerCase()
        const searchTerms = [mod.id.toLowerCase(), mod.fullName.toLowerCase(), mod.name.toLowerCase()]
        // Also match module name variations (e.g. "SPI" should match "SPI-Bus", "SPI_SHARED")
        if (mod.id === 'SPI') searchTerms.push('spi')
        if (mod.id === 'NETWORK') searchTerms.push('netzwerk', 'pgn', 'network')
        if (mod.id === 'NTRIP') searchTerms.push('ntrip', 'rtcm')
        if (mod.id === 'GNSS') searchTerms.push('gnss', 'nmea', 'um980', 'uart')
        if (mod.id === 'SAFETY') searchTerms.push('safety', 'watchdog', 'sicherheit')
        if (mod.id === 'STEER') searchTerms.push('steer', 'lenkung', 'pid')
        if (mod.id === 'ACTUATOR') searchTerms.push('actuator', 'aktuator', 'pwm')
        if (mod.id === 'IMU') searchTerms.push('imu', 'bno', 'beschleunigung', 'gyroskop')
        if (mod.id === 'WAS') searchTerms.push('was', 'winkel', 'ads')
        if (mod.id === 'OTA') searchTerms.push('ota', 'firmware update')
        if (mod.id === 'LOGGING') searchTerms.push('logging', 'sd', 'log')
        if (mod.id === 'REMOTE_CONSOLE') searchTerms.push('remote', 'console', 'debug', 'cli', 'shell')
        if (mod.id === 'ETH') searchTerms.push('ethernet', 'eth', 'rmii')
        if (mod.id === 'WIFI') searchTerms.push('wifi', 'wlan')
        if (mod.id === 'BT') searchTerms.push('bluetooth', 'ble')
        return searchTerms.some(term => title.includes(term))
      })
      contextSection = `### Modul: ${mod.name} (${mod.fullName})

| Feld | Wert |
|------|------|
| **ID** | ${mod.id} |
| **Kategorie** | ${mod.category} |
| **Freshness** | ${mod.freshness} ms |
| **Feature Flag** | ${mod.featureFlag} |
| **Beschreibung** | ${mod.description} |
| **Abhängigkeiten** | ${mod.deps.length > 0 ? mod.deps.join(', ') : 'Keine'} |
| **Config Keys** | ${mod.configKeys.length > 0 ? mod.configKeys.join(', ') : 'Keine'} |
| **Source Files** | \`${mod.source.headerExists ? 'mod_' + (mod.id === 'REMOTE_CONSOLE' ? 'remote_console' : mod.id.toLowerCase()) + '.h' : 'N/A'}\` / \`${mod.source.sourceExists ? 'mod_' + (mod.id === 'REMOTE_CONSOLE' ? 'remote_console' : mod.id.toLowerCase()) + '.cpp' : 'N/A'}\` |`
      if (relatedTasks.length > 0) {
        contextSection += `\n\n#### Zugehörige Backlog-Tasks (${relatedTasks.length})
${relatedTasks.map(t => `- **${t.id}**: ${t.title} (${t.status})`).join('\n')}`
      }
    }
  } else if (state.contextType === 'backlog_task') {
    const task = tasks.find(t => t.id === state.contextId)
    if (task) {
      contextSection = `### Backlog-Task: ${task.id}

| Feld | Wert |
|------|------|
| **Titel** | ${task.title} |
| **Epic** | ${task.epic} — ${task.epicName} |
| **Priorität** | ${task.priority} |
| **Status** | ${task.status} |
| **Delivery Mode** | ${task.deliveryMode} |
| **Task Category** | ${task.taskCategoryDe} |
| **Abhängigkeiten** | ${task.dependencies.length > 0 ? task.dependencies.join(', ') : 'Keine'} |`
      // Find related modules
      const relatedModules = modules.filter(m => {
        const title = task.title.toLowerCase()
        const searchTerms = [m.id.toLowerCase(), m.fullName.toLowerCase(), m.name.toLowerCase()]
        return searchTerms.some(term => title.includes(term))
      })
      if (relatedModules.length > 0) {
        contextSection += `\n\n#### Betroffene Module\n${relatedModules.map(m => `- **${m.id}** (${m.fullName}) — ${m.description}`).join('\n')}`
      }
    }
  } else if (state.contextType === 'architecture') {
    contextSection = `### Architekturkontext

Zwei-Task-Architektur (ADR-007):
- **task_fast** (Core 1, 100 Hz): GNSS → IMU → WAS → STEER → ACTUATOR
- **task_slow** (Core 0, Event): NTRIP, ETH Monitor, SD Flush, WDT, SharedSlot RTCM, CLI
- **Betriebsmodi**: CONFIG ↔ WORK (Safety-Pin gesteuert)
- **${modules.length} Module** in ${new Set(modules.map(m => m.category)).size} Kategorien`
  }

  const userNoteSection = state.userNote.trim()
    ? `\n## User Note\n\n${state.userNote.trim()}\n`
    : '\n## User Note\n\n_(keine Notiz angegeben)_\n'

  return `---
id: "${autoId}"
type: "${state.type}"
source: "dashboard"
context_type: "${state.contextType}"
context_id: "${state.contextId}"
status: "open"
priority: "normal"
created_at: "${isoNow}"
target_agent: "planning_agent"
repo: "Keule/Z.AI_WORK"
previous_repo: "Keule/ESP32_AGO_GNSS"
repo_context_required: true
---

# AI Request: ${AI_REQUEST_TYPES.find(t => t.value === state.type)?.label}

## Instruction

You are an AI planning/software architecture agent working on the repository \`Keule/Z.AI_WORK\`.

This request was generated by the local project dashboard.
It does not represent a direct AI call.
The selected dashboard context is attached below and should be treated as authoritative.

The predecessor repository is \`Keule/ESP32_AGO_GNSS\`.
Use that relationship when historical context is relevant.

## Requested Output

${AI_REQUEST_TYPE_OUTPUTS[state.type].map((item, i) => `${i + 1}. ${item}`).join('\n')}

## Selected Context

${contextSection}
${userNoteSection}
---
_Generated by Z.AI ESP32 Dashboard — ${new Date().toLocaleString('de-DE')}_
_Target: \`ai/requests/${state.type.replace('_request', 's')}/${today}_${contextSlug}.md\`_
`
}

function generateBatchMarkdown(requests: AIRequestDraft[]): string {
  const isoNow = new Date().toISOString()
  const today = isoNow.split('T')[0]
  const items = requests.map(r => `### ${r.id} — ${r.contextLabel}\n- Typ: ${AI_REQUEST_TYPES.find(t => t.value === r.type)?.label}\n- Kontext: ${r.contextType} (${r.contextId})`).join('\n\n')
  return `---
id: "BATCH-${today}"
type: "batch_request"
source: "dashboard"
status: "open"
created_at: "${isoNow}"
target_agent: "planning_agent"
repo: "Keule/Z.AI_WORK"
---

# AI Batch Request

## Instruction

You are an AI planning agent. Analyze the following ${requests.length} collected requests from the Z.AI_WORK dashboard.

For each request, provide your analysis. Then identify:
1. Overlapping concerns between requests
2. Dependencies between requests
3. Recommended implementation order
4. Consolidated action items

## Collected Requests

${items}
---
_Generated by Z.AI ESP32 Dashboard — Batch of ${requests.length} requests_
`
}

function downloadMarkdown(content: string, filename: string) {
  const blob = new Blob([content], { type: 'text/markdown;charset=utf-8' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}

async function copyToClipboard(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text)
    return true
  } catch {
    return false
  }
}

// ─── LOADING SKELETON ─────────────────────────────────────────────────────────

function LoadingCard() {
  return (
    <Card className="py-4 gap-3">
      <CardContent className="px-4 flex items-center gap-3">
        <div className="rounded-lg bg-muted p-2 animate-pulse" style={{ width: 36, height: 36 }} />
        <div className="flex-1 space-y-2">
          <div className="h-3 bg-muted rounded animate-pulse w-20" />
          <div className="h-5 bg-muted rounded animate-pulse w-10" />
        </div>
      </CardContent>
    </Card>
  )
}

function DataErrorBanner({ message, onRetry }: { message: string; onRetry: () => void }) {
  return (
    <Card className="border-destructive/50 bg-destructive/5">
      <CardContent className="px-4 py-3 flex items-center gap-3">
        <AlertTriangle className="size-4 text-destructive shrink-0" />
        <p className="text-sm text-destructive flex-1">{message}</p>
        <Button variant="outline" size="sm" onClick={onRetry} className="gap-1.5 text-xs shrink-0">
          <RefreshCw className="size-3" />
          Erneut laden
        </Button>
      </CardContent>
    </Card>
  )
}

// ─── THEME TOGGLE ─────────────────────────────────────────────────────────────

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

function OverviewTab({ modules, backlogStats, commits }: { modules: ModuleData[]; backlogStats: { total: number; done: number; open: number; inProgress: number } | null; commits: CommitData[] }) {
  const moduleCount = modules.length
  const activeModules = modules.filter(m => m.source.sourceExists).length
  const categoryCount = new Set(modules.map(m => m.category)).size

  return (
    <div className="space-y-6">
      {/* Quick Stats */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <Card key="modules" className="py-4 gap-3">
          <CardContent className="px-4 flex items-center gap-3">
            <div className="rounded-lg bg-muted p-2 shrink-0"><Box className="size-5" /></div>
            <div className="min-w-0">
              <p className="text-2xl font-bold tracking-tight">{moduleCount}</p>
              <p className="text-xs text-muted-foreground truncate">Module gesamt</p>
            </div>
          </CardContent>
        </Card>
        <Card key="adrs" className="py-4 gap-3">
          <CardContent className="px-4 flex items-center gap-3">
            <div className="rounded-lg bg-muted p-2 shrink-0"><FileText className="size-5" /></div>
            <div className="min-w-0">
              <p className="text-2xl font-bold tracking-tight">21</p>
              <p className="text-xs text-muted-foreground truncate">Aktive ADRs</p>
            </div>
          </CardContent>
        </Card>
        <Card key="backlog" className="py-4 gap-3">
          <CardContent className="px-4 flex items-center gap-3">
            <div className="rounded-lg bg-muted p-2 shrink-0"><ListTodo className="size-5" /></div>
            <div className="min-w-0">
              <p className="text-2xl font-bold tracking-tight">{backlogStats?.total ?? '—'}</p>
              <p className="text-xs text-muted-foreground truncate">
                {backlogStats ? `${backlogStats.done} erledigt · ${backlogStats.open} offen` : 'Backlog-Aufgaben'}
              </p>
            </div>
          </CardContent>
        </Card>
        <Card key="build" className="py-4 gap-3">
          <CardContent className="px-4 flex items-center gap-3">
            <div className="rounded-lg bg-muted p-2 shrink-0"><CheckCircle className="size-5 text-emerald-500" /></div>
            <div className="min-w-0">
              <p className="text-2xl font-bold tracking-tight">{activeModules}/{moduleCount}</p>
              <p className="text-xs text-muted-foreground truncate">Module mit Source</p>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Project Info & Architecture */}
      <div className="grid md:grid-cols-2 gap-6">
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
                ['Module', `${moduleCount} in ${categoryCount} Kategorien`],
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

        {/* Architecture Card — STATIC (upper-level, doesn't change) */}
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
              <div className="rounded-lg border border-emerald-300 dark:border-emerald-700 bg-emerald-50 dark:bg-emerald-950/30 p-4">
                <div className="flex items-center gap-2 mb-2">
                  <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800 text-xs">Core 1</Badge>
                  <span className="font-semibold text-sm">task_fast</span>
                  <Badge variant="outline" className="text-xs ml-auto">Priorität 5</Badge>
                </div>
                <p className="text-xs text-muted-foreground">GNSS → IMU → WAS → STEER → ACTUATOR</p>
                <p className="text-xs text-muted-foreground mt-1">Periodisch · 100 Hz · Sensor → Aktuator Pfad</p>
              </div>
              <div className="rounded-lg border border-amber-300 dark:border-amber-700 bg-amber-50 dark:bg-amber-950/30 p-4">
                <div className="flex items-center gap-2 mb-2">
                  <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800 text-xs">Core 0</Badge>
                  <span className="font-semibold text-sm">task_slow</span>
                  <Badge variant="outline" className="text-xs ml-auto">Priorität 2</Badge>
                </div>
                <p className="text-xs text-muted-foreground">HW-Monitor · WDT Feed · SD Flush · NTRIP Tick · ETH Monitor</p>
                <p className="text-xs text-muted-foreground">SharedSlot RTCM → UART · DBG.loop() · CLI Polling</p>
                <p className="text-xs text-muted-foreground mt-1">Eventgesteuert · Lifecycle-Owner · Kommunikation & Verwaltung</p>
              </div>
              <div className="rounded-lg border border-dashed border-muted-foreground/20 bg-muted/30 p-3">
                <div className="flex items-center gap-2 mb-1">
                  <Badge variant="outline" className="text-[10px] px-1.5 py-0 text-muted-foreground">ADR-007</Badge>
                  <span className="text-xs text-muted-foreground line-through">maintTask</span>
                  <span className="text-[10px] text-muted-foreground">→ task_slow integriert</span>
                </div>
                <p className="text-[10px] text-muted-foreground">SD Flush · NTRIP Tick · ETH Monitor · WDT Feed · SharedSlot RTCM</p>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Mode System Card — STATIC */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base">
            <Settings className="size-4 text-muted-foreground" />
            Betriebsmodi (OpMode)
          </CardTitle>
          <CardDescription>Zwei-Zustands-Automat: Konfiguration ←→ Arbeit</CardDescription>
        </CardHeader>
        <CardContent>
          <div className="flex flex-col sm:flex-row items-center gap-4 justify-center">
            <div className="rounded-lg border-2 border-dashed border-amber-400 dark:border-amber-600 p-4 flex-1 max-w-xs w-full">
              <div className="flex items-center gap-2 mb-2">
                <Badge className="bg-amber-100 text-amber-800 dark:bg-amber-900/50 dark:text-amber-300 border-amber-200 dark:border-amber-800">CONFIG</Badge>
                <Wrench className="size-4 text-amber-600 dark:text-amber-400" />
              </div>
              <p className="text-sm font-medium">Konfigurationsmodus</p>
              <p className="text-xs text-muted-foreground mt-1">Web-UI erreichbar · Parameter setzen · Kalibrierung · Firmware-Update</p>
            </div>
            <div className="flex flex-col items-center gap-1 text-muted-foreground">
              <ArrowRight className="size-5 rotate-90 sm:rotate-0" />
              <span className="text-xs">Übergang</span>
              <ArrowRight className="size-5 rotate-90 sm:rotate-0" />
            </div>
            <div className="rounded-lg border-2 border-solid border-emerald-400 dark:border-emerald-600 p-4 flex-1 max-w-xs w-full">
              <div className="flex items-center gap-2 mb-2">
                <Badge className="bg-emerald-100 text-emerald-800 dark:bg-emerald-900/50 dark:text-emerald-300 border-emerald-200 dark:border-emerald-800">WORK</Badge>
                <Zap className="size-4 text-emerald-600 dark:text-emerald-400" />
              </div>
              <p className="text-sm font-medium">Arbeitsmodus</p>
              <p className="text-xs text-muted-foreground mt-1">Autosteuerung aktiv · Echtzeit-Sensorpfad · AgOpenGPS-Datenstream</p>
            </div>
          </div>
          <div className="mt-4 text-center">
            <p className="text-xs text-muted-foreground">
              Übergang: CONFIG → WORK bei Safety-Pin HIGH · WORK → CONFIG bei Safety-Pin LOW + Geschwindigkeit &lt; Schwellwert oder CLI `mode setup`
            </p>
          </div>
        </CardContent>
      </Card>

      {/* Recent Activity — dynamic from git */}
      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <CardTitle className="flex items-center gap-2 text-base">
              <GitCommit className="size-4 text-muted-foreground" />
              Letzte Aktivitäten
            </CardTitle>
            <Badge variant="outline" className="text-[10px]">Source: git log</Badge>
          </div>
        </CardHeader>
        <CardContent>
          <div className="space-y-1">
            {commits.slice(0, 12).map((commit) => (
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

function ModuleTab({ modules, loading, error, refetch }: { modules: ModuleData[]; loading: boolean; error: string | null; refetch: () => void }) {
  const [search, setSearch] = useState('')
  const [filterCategory, setFilterCategory] = useState<ModuleCategory | 'Alle'>('Alle')
  const [expandedModule, setExpandedModule] = useState<string | null>(null)

  const filteredModules = useMemo(() => {
    return modules.filter((m) => {
      const matchSearch = search === '' ||
        m.name.toLowerCase().includes(search.toLowerCase()) ||
        m.fullName.toLowerCase().includes(search.toLowerCase()) ||
        m.description.toLowerCase().includes(search.toLowerCase()) ||
        m.id.toLowerCase().includes(search.toLowerCase())
      const matchCategory = filterCategory === 'Alle' || m.category === filterCategory
      return matchSearch && matchCategory
    })
  }, [modules, search, filterCategory])

  const categories: (ModuleCategory | 'Alle')[] = ['Alle', 'Core', 'Sensor', 'Kommunikation', 'Sicherheit', 'Werkzeug']

  if (error) return <DataErrorBanner message={error} onRetry={refetch} />
  if (loading) return <div className="grid grid-cols-2 md:grid-cols-3 gap-3">{[1,2,3,4,5,6].map(i => <LoadingCard key={i} />)}</div>

  return (
    <div className="space-y-4">
      <div className="flex items-center gap-2 text-xs text-muted-foreground">
        <Database className="size-3" />
        <span>Daten aus Quellcode geladen: module_interface.h, module_system.cpp, features.h</span>
      </div>

      <div className="flex flex-col sm:flex-row gap-3">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 size-4 text-muted-foreground" />
          <Input placeholder="Module durchsuchen..." className="pl-9" value={search} onChange={(e) => setSearch(e.target.value)} />
          {search && (
            <button onClick={() => setSearch('')} className="absolute right-3 top-1/2 -translate-y-1/2 text-muted-foreground hover:text-foreground">
              <X className="size-3.5" />
            </button>
          )}
        </div>
        <div className="flex gap-2 flex-wrap">
          {categories.map((cat) => (
            <Button key={cat} variant={filterCategory === cat ? 'default' : 'outline'} size="sm" onClick={() => setFilterCategory(cat)} className="text-xs">{cat}</Button>
          ))}
        </div>
      </div>

      <div className="grid sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-3">
        <AnimatePresence mode="popLayout">
          {filteredModules.map((mod) => (
            <motion.div key={mod.id} layout initial={{ opacity: 0, scale: 0.95 }} animate={{ opacity: 1, scale: 1 }} exit={{ opacity: 0, scale: 0.95 }} transition={{ duration: 0.2 }}>
              <Card
                className={`cursor-pointer transition-all hover:shadow-md border-l-4 ${CATEGORY_BORDER_COLORS[mod.category]} py-0 gap-0`}
                onClick={() => setExpandedModule(expandedModule === mod.id ? null : mod.id)}
              >
                <CardHeader className="px-4 py-3 gap-1">
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-2">
                      {MODULE_ICONS[mod.id] || <Box className="size-4" />}
                      <CardTitle className="text-sm font-semibold">{mod.name}</CardTitle>
                    </div>
                    <Badge variant="outline" className={`text-[10px] px-1.5 py-0 border ${CATEGORY_COLORS[mod.category]}`}>{mod.category}</Badge>
                  </div>
                  <CardDescription className="text-xs">{mod.fullName}</CardDescription>
                </CardHeader>
                <AnimatePresence>
                  {expandedModule === mod.id && (
                    <motion.div initial={{ height: 0, opacity: 0 }} animate={{ height: 'auto', opacity: 1 }} exit={{ height: 0, opacity: 0 }} transition={{ duration: 0.2 }} className="overflow-hidden">
                      <CardContent className="px-4 pb-3 pt-0 space-y-2">
                        <p className="text-xs text-muted-foreground">{mod.description}</p>
                        <div className="grid grid-cols-2 gap-x-4 gap-y-1 text-[10px]">
                          <div className="flex justify-between"><span className="text-muted-foreground">Freshness</span><code className="font-mono">{mod.freshness} ms</code></div>
                          <div className="flex justify-between"><span className="text-muted-foreground">Feature</span><code className="font-mono text-[9px]">{mod.featureFlag}</code></div>
                          <div className="flex justify-between col-span-2"><span className="text-muted-foreground">Abhängigkeiten</span>
                            <div className="flex gap-1">{mod.deps.length > 0 ? mod.deps.map((d) => <Badge key={d} variant="secondary" className="text-[10px] px-1.5 py-0">{d}</Badge>) : <span className="text-muted-foreground">Keine</span>}</div>
                          </div>
                        </div>
                        {mod.configKeys.length > 0 && (
                          <div className="text-[10px]">
                            <span className="text-muted-foreground">Config Keys: </span>
                            <span className="font-mono">{mod.configKeys.join(', ')}</span>
                          </div>
                        )}
                        {mod.errorCodes.length > 0 && (
                          <div className="text-[10px]">
                            <span className="text-muted-foreground">Error Codes: </span>
                            <span className="font-mono">{mod.errorCodes.join(', ')}</span>
                          </div>
                        )}
                        <div className="pt-2 border-t mt-1">
                          <Button
                            variant="outline" size="sm" className="w-full text-xs gap-1.5"
                            onClick={(e) => {
                              e.stopPropagation()
                              pendingAIRequest = { contextType: 'module', contextId: mod.id, contextLabel: `${mod.name} (${mod.fullName})` }
                              window.dispatchEvent(new CustomEvent('ai-request-trigger'))
                            }}
                          >
                            <Sparkles className="size-3" /> AI Request
                          </Button>
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
                        {mod.deps.map((d) => <span key={d} className="text-[10px] text-muted-foreground">← {d}</span>)}
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
          <Button variant="link" size="sm" onClick={() => { setSearch(''); setFilterCategory('Alle'); }}>Filter zurücksetzen</Button>
        </div>
      )}
    </div>
  )
}

// ─── BACKLOG TAB ──────────────────────────────────────────────────────────────

function BacklogTab({ tasks, epics, stats, loading, error, refetch }: {
  tasks: BacklogTaskData[]; epics: { id: string; title: string }[];
  stats: { total: number; done: number; open: number; inProgress: number } | null;
  loading: boolean; error: string | null; refetch: () => void
}) {
  const [filterPriority, setFilterPriority] = useState<string>('Alle')
  const [filterStatus, setFilterStatus] = useState<string>('Alle')
  const [filterEpic, setFilterEpic] = useState<string>('Alle')
  const [sortBy, setSortBy] = useState<'priority' | 'id'>('priority')

  const sortedTasks = useMemo(() => {
    return tasks
      .filter((t) => {
        if (filterPriority !== 'Alle' && t.priority !== filterPriority) return false
        if (filterStatus !== 'Alle' && t.status !== filterStatus) return false
        if (filterEpic !== 'Alle' && t.epic !== filterEpic) return false
        return true
      })
      .sort((a, b) => {
        if (sortBy === 'priority') {
          const diff = (PRIORITY_ORDER[a.priority as TaskPriority] ?? 99) - (PRIORITY_ORDER[b.priority as TaskPriority] ?? 99)
          return diff !== 0 ? diff : a.id.localeCompare(b.id)
        }
        return a.id.localeCompare(b.id)
      })
  }, [tasks, filterPriority, filterStatus, filterEpic, sortBy])

  const epicOptions = ['Alle', ...epics.map(e => e.id)]

  if (error) return <DataErrorBanner message={error} onRetry={refetch} />
  if (loading) return <div className="grid grid-cols-3 gap-3">{[1,2,3].map(i => <LoadingCard key={i} />)}</div>

  return (
    <div className="space-y-4">
      <div className="flex items-center gap-2 text-xs text-muted-foreground">
        <Database className="size-3" />
        <span>Daten geladen aus esp32_project/backlog/index.yaml ({tasks.length} Tasks)</span>
      </div>

      <div className="grid grid-cols-3 gap-3">
        {[
          { label: 'Erledigt', count: stats?.done ?? 0, badge: STATUS_BADGE['Erledigt'] },
          { label: 'Offen', count: stats?.open ?? 0, badge: STATUS_BADGE['Offen'] },
          { label: 'In Arbeit', count: stats?.inProgress ?? 0, badge: STATUS_BADGE['In Arbeit'] },
        ].map((s) => (
          <Card key={s.label} className="py-0 gap-0">
            <CardContent className="px-4 py-3 flex items-center gap-3">
              {s.badge.icon}
              <div>
                <p className="text-lg font-bold">{s.count}</p>
                <p className="text-xs text-muted-foreground">{s.label}</p>
              </div>
            </CardContent>
          </Card>
        ))}
      </div>

      <div className="flex flex-col sm:flex-row gap-3">
        <div className="flex items-center gap-2 text-xs text-muted-foreground shrink-0">
          <Filter className="size-3.5" /><span>Filter:</span>
        </div>
        <div className="flex gap-2 flex-wrap">
          <div className="flex gap-1">
            {(['Alle', 'Hoch', 'Mittel', 'Niedrig'] as const).map((p) => (
              <Button key={p} variant={filterPriority === p ? 'default' : 'outline'} size="sm" onClick={() => setFilterPriority(p)} className="text-xs h-7">{p === 'Alle' ? 'Priorität' : p}</Button>
            ))}
          </div>
          <Separator orientation="vertical" className="h-7" />
          <div className="flex gap-1">
            {(['Alle', 'Erledigt', 'Offen', 'In Arbeit'] as const).map((s) => (
              <Button key={s} variant={filterStatus === s ? 'default' : 'outline'} size="sm" onClick={() => setFilterStatus(s)} className="text-xs h-7">{s === 'Alle' ? 'Status' : s}</Button>
            ))}
          </div>
          <Separator orientation="vertical" className="h-7" />
          <div className="flex gap-1 flex-wrap">
            {epicOptions.map((e) => (
              <Button key={e} variant={filterEpic === e ? 'default' : 'outline'} size="sm" onClick={() => setFilterEpic(e)} className="text-xs h-7">{e === 'Alle' ? 'Epic' : e}</Button>
            ))}
          </div>
        </div>
        <div className="flex items-center gap-2 ml-auto">
          <span className="text-xs text-muted-foreground">Sortierung:</span>
          <Button variant={sortBy === 'priority' ? 'default' : 'outline'} size="sm" onClick={() => setSortBy('priority')} className="text-xs h-7">Priorität</Button>
          <Button variant={sortBy === 'id' ? 'default' : 'outline'} size="sm" onClick={() => setSortBy('id')} className="text-xs h-7">ID</Button>
        </div>
      </div>

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
                <TableHead className="w-[40px] text-xs font-semibold"></TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {sortedTasks.map((task) => (
                <TableRow key={task.id} className="cursor-pointer">
                  <TableCell><code className="text-xs font-mono text-muted-foreground">{task.id}</code></TableCell>
                  <TableCell><span className="text-sm">{task.title}</span></TableCell>
                  <TableCell className="hidden md:table-cell">
                    <div className="flex flex-col">
                      <code className="text-xs font-mono text-muted-foreground">{task.epic}</code>
                      <span className="text-xs text-muted-foreground">{task.epicName}</span>
                    </div>
                  </TableCell>
                  <TableCell>
                    <Badge className={`text-[10px] px-1.5 py-0 border ${(PRIORITY_BADGE[task.priority] || PRIORITY_BADGE['Mittel']).className}`}>
                      {(PRIORITY_BADGE[task.priority] || PRIORITY_BADGE['Mittel']).icon}{task.priority}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <Badge className={`text-[10px] px-1.5 py-0 border ${(STATUS_BADGE[task.status] || STATUS_BADGE['Offen']).className}`}>
                      {(STATUS_BADGE[task.status] || STATUS_BADGE['Offen']).icon}{task.status}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <Button variant="ghost" size="sm" className="size-7 text-xs gap-1"
                      onClick={(e) => {
                        e.stopPropagation()
                        pendingAIRequest = { contextType: 'backlog_task', contextId: task.id, contextLabel: `${task.id}: ${task.title}` }
                        window.dispatchEvent(new CustomEvent('ai-request-trigger'))
                      }}
                    >
                      <Sparkles className="size-3" />
                    </Button>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </div>
        {sortedTasks.length === 0 && <div className="text-center py-8 text-muted-foreground text-sm">Keine Aufgaben entsprechen den gewählten Filtern</div>}
        <div className="border-t px-4 py-2 flex justify-between items-center">
          <span className="text-xs text-muted-foreground">{sortedTasks.length} von {tasks.length} Aufgaben</span>
          <span className="text-xs text-muted-foreground">
            {tasks.length > 0 ? `${tasks[0]?.id} bis ${tasks[tasks.length - 1]?.id}` : ''} · {stats?.done ?? 0}/{stats?.total ?? 0} erledigt
          </span>
        </div>
      </Card>
    </div>
  )
}

// ─── AI REQUEST TAB ───────────────────────────────────────────────────────────

function AIRequestTab({ modules, tasks }: { modules: ModuleData[]; tasks: BacklogTaskData[] }) {
  const [generator, setGenerator] = useState<AIRequestGeneratorState>({
    type: 'issue_request', contextType: 'module', contextId: '', contextLabel: '', userNote: '',
  })
  const [previewMarkdown, setPreviewMarkdown] = useState('')
  const [collectedRequests, setCollectedRequests] = useState<AIRequestDraft[]>([])
  const [batchMarkdown, setBatchMarkdown] = useState('')
  const [copied, setCopied] = useState(false)

  useEffect(() => {
    const handler = () => {
      if (pendingAIRequest) {
        setGenerator(prev => ({
          ...prev,
          contextType: pendingAIRequest!.contextType,
          contextId: pendingAIRequest!.contextId,
          contextLabel: pendingAIRequest!.contextLabel,
        }))
        pendingAIRequest = null
      }
    }
    window.addEventListener('ai-request-trigger', handler)
    return () => window.removeEventListener('ai-request-trigger', handler)
  }, [])

  const handleGenerate = () => {
    const md = generateAIRequestMarkdown(generator, modules, tasks)
    setPreviewMarkdown(md)
  }

  const handleCopy = async () => {
    const text = previewMarkdown || batchMarkdown
    if (!text) return
    const ok = await copyToClipboard(text)
    if (ok) { setCopied(true); setTimeout(() => setCopied(false), 2000) }
  }

  const handleDownload = () => {
    const text = previewMarkdown || batchMarkdown
    if (!text) return
    const today = new Date().toISOString().split('T')[0]
    const contextSlug = generator.contextId.toLowerCase().replace(/[^a-z0-9]+/g, '_') || 'general'
    const typeSlug = generator.type.replace('_request', '')
    const filename = batchMarkdown ? `batch_${today}_ai_requests.md` : `${today}_${contextSlug}_${typeSlug}_request.md`
    downloadMarkdown(text, filename)
  }

  const handleAddToBatch = () => {
    if (!previewMarkdown) return
    const today = new Date().toISOString().split('T')[0]
    const contextSlug = generator.contextId.toLowerCase().replace(/[^a-z0-9]+/g, '_') || 'general'
    const typeSlug = generator.type.replace('_request', '')
    const draft: AIRequestDraft = {
      id: `AI-${today.replace(/-/g, '')}-${typeSlug}-${contextSlug}`,
      type: generator.type, contextType: generator.contextType,
      contextId: generator.contextId,
      contextLabel: generator.contextLabel || `${generator.contextType}: ${generator.contextId}`,
      userNote: generator.userNote, createdAt: new Date().toISOString(), markdown: previewMarkdown,
    }
    setCollectedRequests(prev => [...prev, draft])
    setPreviewMarkdown('')
  }

  const handleRemoveFromBatch = (index: number) => {
    setCollectedRequests(prev => prev.filter((_, i) => i !== index))
    setBatchMarkdown('')
  }

  const handleGenerateBatch = () => {
    if (collectedRequests.length === 0) return
    const md = generateBatchMarkdown(collectedRequests)
    setBatchMarkdown(md)
    setPreviewMarkdown('')
  }

  const contextOptions = useMemo(() => {
    if (generator.contextType === 'module') {
      return modules.map(m => ({ value: m.id, label: `${m.name} (${m.fullName})` }))
    } else if (generator.contextType === 'backlog_task') {
      return tasks.map(t => ({ value: t.id, label: `${t.id}: ${t.title}` }))
    }
    return []
  }, [generator.contextType, modules, tasks])

  const handleContextSelect = (value: string) => {
    const option = contextOptions.find(o => o.value === value)
    setGenerator(prev => ({ ...prev, contextId: value, contextLabel: option?.label || '' }))
  }

  return (
    <div className="space-y-6">
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base"><Sparkles className="size-4 text-amber-500" /> Neuen Request erstellen</CardTitle>
          <CardDescription>Erstelle strukturierte AI-Requests mit Dashboard-Kontext (Dynamisch geladen)</CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <div>
            <label className="text-xs font-medium text-muted-foreground mb-2 block">Request-Typ</label>
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-2">
              {AI_REQUEST_TYPES.map(rt => (
                <Button key={rt.value} variant={generator.type === rt.value ? 'default' : 'outline'} size="sm"
                  className="h-auto py-2 px-3 flex flex-col items-start gap-0.5 text-left"
                  onClick={() => setGenerator(prev => ({ ...prev, type: rt.value }))}
                >
                  <span className="text-xs font-medium">{rt.label}</span>
                  <span className="text-[10px] text-muted-foreground leading-tight">{rt.description}</span>
                </Button>
              ))}
            </div>
          </div>
          <Separator />
          <div className="grid sm:grid-cols-2 gap-3">
            <div>
              <label className="text-xs font-medium text-muted-foreground mb-1.5 block">Kontext-Typ</label>
              <div className="flex gap-1">
                {[
                  { value: 'module' as AIContextType, label: 'Modul' },
                  { value: 'backlog_task' as AIContextType, label: 'Backlog Task' },
                  { value: 'architecture' as AIContextType, label: 'Architektur' },
                ].map(ct => (
                  <Button key={ct.value} variant={generator.contextType === ct.value ? 'default' : 'outline'} size="sm"
                    className="text-xs flex-1"
                    onClick={() => setGenerator(prev => ({
                      ...prev, contextType: ct.value,
                      contextId: ct.value === 'architecture' ? 'global' : '',
                      contextLabel: ct.value === 'architecture' ? 'Gesamte Architektur' : '',
                    }))}
                  >
                    {ct.label}
                  </Button>
                ))}
              </div>
            </div>
            {generator.contextType !== 'architecture' && (
              <div>
                <label className="text-xs font-medium text-muted-foreground mb-1.5 block">
                  {generator.contextType === 'module' ? 'Modul wählen' : 'Task wählen'}
                </label>
                <select
                  className="w-full h-9 rounded-md border border-input bg-background px-3 py-1 text-sm shadow-xs transition-colors focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring"
                  value={generator.contextId} onChange={(e) => handleContextSelect(e.target.value)}
                >
                  <option value="">— Bitte wählen —</option>
                  {contextOptions.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
                </select>
              </div>
            )}
          </div>
          <div>
            <label className="text-xs font-medium text-muted-foreground mb-1.5 block">User Note (optional)</label>
            <textarea
              className="w-full min-h-[80px] rounded-md border border-input bg-background px-3 py-2 text-sm shadow-xs transition-colors placeholder:text-muted-foreground focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring resize-y"
              placeholder="Zusätzliche Hinweise, Anforderungen oder Randbedingungen..."
              value={generator.userNote} onChange={(e) => setGenerator(prev => ({ ...prev, userNote: e.target.value }))}
            />
          </div>
          <div className="flex gap-2">
            <Button onClick={handleGenerate} className="gap-1.5"><Send className="size-3.5" /> Markdown erzeugen</Button>
            {previewMarkdown && (
              <>
                <Button variant="outline" size="sm" onClick={handleCopy} className="gap-1.5">
                  {copied ? <CheckCircle className="size-3.5 text-emerald-500" /> : <Copy className="size-3.5" />}
                  {copied ? 'Kopiert!' : 'Kopieren'}
                </Button>
                <Button variant="outline" size="sm" onClick={handleDownload} className="gap-1.5"><Download className="size-3.5" /> Herunterladen</Button>
                <Button variant="outline" size="sm" onClick={handleAddToBatch} className="gap-1.5"><ClipboardList className="size-3.5" /> Zum Batch</Button>
              </>
            )}
          </div>
        </CardContent>
      </Card>

      {previewMarkdown && (
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2 text-base"><Eye className="size-4 text-muted-foreground" /> Markdown-Vorschau</CardTitle>
          </CardHeader>
          <CardContent>
            <ScrollArea className="h-[400px] w-full rounded-md border">
              <pre className="p-4 text-xs font-mono whitespace-pre-wrap break-words leading-relaxed">{previewMarkdown}</pre>
            </ScrollArea>
          </CardContent>
        </Card>
      )}

      <Card>
        <CardHeader>
          <div className="flex items-center justify-between">
            <div>
              <CardTitle className="flex items-center gap-2 text-base"><ClipboardCopy className="size-4 text-muted-foreground" /> Gesammelte Requests (Batch)</CardTitle>
              <CardDescription className="mt-1">{collectedRequests.length === 0 ? 'Sammle mehrere Requests und erzeuge eine kombinierte Batch-Analyse' : `${collectedRequests.length} Request(s) gesammelt`}</CardDescription>
            </div>
            {collectedRequests.length > 0 && (
              <div className="flex gap-2">
                <Button size="sm" onClick={handleGenerateBatch} className="gap-1.5"><Sparkles className="size-3.5" /> Batch erzeugen</Button>
                {batchMarkdown && (
                  <>
                    <Button variant="outline" size="sm" onClick={handleCopy} className="gap-1.5">{copied ? <CheckCircle className="size-3.5 text-emerald-500" /> : <Copy className="size-3.5" />}{copied ? 'Kopiert!' : 'Kopieren'}</Button>
                    <Button variant="outline" size="sm" onClick={handleDownload} className="gap-1.5"><Download className="size-3.5" /> Download</Button>
                  </>
                )}
              </div>
            )}
          </div>
        </CardHeader>
        {collectedRequests.length > 0 && (
          <CardContent>
            <div className="space-y-2">
              {collectedRequests.map((r, i) => (
                <div key={`${r.id}-${i}`} className="flex items-center justify-between rounded-md border px-3 py-2 gap-2">
                  <div className="min-w-0 flex-1">
                    <div className="flex items-center gap-2">
                      <code className="text-xs font-mono text-muted-foreground">{r.id}</code>
                      <Badge variant="outline" className="text-[10px] px-1.5 py-0">{AI_REQUEST_TYPES.find(t => t.value === r.type)?.label}</Badge>
                    </div>
                    <p className="text-xs text-muted-foreground mt-0.5 truncate">{r.contextLabel}</p>
                  </div>
                  <Button variant="ghost" size="sm" className="size-7 shrink-0 text-muted-foreground hover:text-destructive" onClick={() => handleRemoveFromBatch(i)}><Trash2 className="size-3.5" /></Button>
                </div>
              ))}
            </div>
          </CardContent>
        )}
        {batchMarkdown && (
          <CardContent className="pt-0">
            <ScrollArea className="h-[350px] w-full rounded-md border">
              <pre className="p-4 text-xs font-mono whitespace-pre-wrap break-words leading-relaxed">{batchMarkdown}</pre>
            </ScrollArea>
          </CardContent>
        )}
      </Card>
    </div>
  )
}

// ─── ARCHITECTURE TAB ─────────────────────────────────────────────────────────

const ARCH_FADE = { initial: { opacity: 0, y: 20 }, animate: { opacity: 1, y: 0 }, transition: { duration: 0.4 } } as const

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

function ArchitectureTab({ modules }: { modules: ModuleData[] }) {
  // Group modules by category dynamically
  const categories = useMemo(() => {
    const groups = new Map<string, string[]>()
    for (const m of modules) {
      if (!groups.has(m.category)) groups.set(m.category, [])
      groups.get(m.category)!.push(m.id)
    }
    return Array.from(groups.entries()).map(([cat, mods]) => ({
      cat: cat as ModuleCategory,
      modules: mods,
      color: CATEGORY_COLORS[cat as ModuleCategory],
    }))
  }, [modules])

  return (
    <div className="space-y-6">
      <motion.div {...ARCH_FADE}>
        <Card>
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="flex items-center gap-2 text-base"><Layers className="size-4 text-muted-foreground" /> System-Übersicht — Schichtenarchitektur</CardTitle>
              <Badge variant="outline" className="text-[10px]">Dynamisch aus Source</Badge>
            </div>
            <CardDescription>Top-to-Bottom Layered Architecture des ESP32-S3 Firmware-Stacks</CardDescription>
          </CardHeader>
          <CardContent className="space-y-0">
            <ArchLayer title="Externe Schnittstellen" color="border-rose-300 dark:border-rose-700 bg-rose-50 dark:bg-rose-950/20">
              <div className="flex flex-wrap gap-2 justify-center">
                {[
                  { label: 'AgOpenGPS', icon: <Radio className="size-3.5" /> },
                  { label: 'Web-UI', icon: <MonitorSmartphone className="size-3.5" /> },
                  { label: 'NTRIP Caster', icon: <RadioTower className="size-3.5" /> },
                  { label: 'SD-Karte', icon: <Database className="size-3.5" /> },
                  { label: 'Remote Console', icon: <Cable className="size-3.5" /> },
                ].map((item) => (
                  <span key={item.label} className="inline-flex items-center gap-1.5 rounded-md border border-rose-200 dark:border-rose-800 bg-rose-100 dark:bg-rose-900/30 px-2.5 py-1 text-[11px] font-medium text-rose-800 dark:text-rose-300">
                    {item.icon}{item.label}
                  </span>
                ))}
              </div>
            </ArchLayer>
            <ArchConnector />
            <ArchLayer title="Protokoll-Schicht" color="border-amber-300 dark:border-amber-700 bg-amber-50 dark:bg-amber-950/20">
              <div className="flex flex-wrap gap-2 justify-center">
                {['PGN-Codec', 'NMEA-Parser', 'AOG-UDP', 'NTRIP-Client'].map((p) => (
                  <span key={p} className="rounded-md border border-amber-200 dark:border-amber-800 bg-amber-100 dark:bg-amber-900/30 px-2.5 py-1 text-[11px] font-mono font-medium text-amber-800 dark:text-amber-300">{p}</span>
                ))}
              </div>
            </ArchLayer>
            <ArchConnector />
            <ArchLayer title={`Modul-System — ${modules.length} Module`} color="border-primary/40 bg-primary/[0.03] dark:bg-primary/[0.06]">
              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
                {categories.map(({ cat, modules: mods, color }) => (
                  <div key={cat} className="space-y-1.5">
                    <Badge className={`text-[10px] border ${color}`}>{cat} ({mods.length})</Badge>
                    <div className="flex flex-wrap gap-1">
                      {mods.map((m) => (
                        <span key={m} className="rounded border border-border bg-background px-2 py-0.5 text-[10px] font-mono font-semibold">{m}</span>
                      ))}
                    </div>
                  </div>
                ))}
              </div>
            </ArchLayer>
            <ArchConnector />
            <ArchLayer title="Hardware-Abstraktion (HAL)" color="border-sky-300 dark:border-sky-700 bg-sky-50 dark:bg-sky-950/20">
              <div className="flex flex-wrap gap-2 justify-center">
                {['hal_esp32', 'SPI', 'UART', 'I2C', 'GPIO', 'NVS', 'ETH (RMII)'].map((p) => (
                  <span key={p} className="rounded-md border border-sky-200 dark:border-sky-800 bg-sky-100 dark:bg-sky-900/30 px-2.5 py-1 text-[11px] font-mono font-medium text-sky-800 dark:text-sky-300">{p}</span>
                ))}
              </div>
            </ArchLayer>
          </CardContent>
        </Card>
      </motion.div>

      {/* Dependency Graph — dynamic */}
      <motion.div {...ARCH_FADE}>
        <Card>
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="flex items-center gap-2 text-base"><CircuitBoard className="size-4 text-muted-foreground" /> Modul-Abhängigkeitsgraph</CardTitle>
              <Badge variant="outline" className="text-[10px]">Source: mod_*.cpp s_deps[]</Badge>
            </div>
            <CardDescription>Modulabhängigkeiten aus den s_deps Arrays der Quellcode-Module</CardDescription>
          </CardHeader>
          <CardContent>
            <div className="grid sm:grid-cols-2 lg:grid-cols-3 gap-3">
              {modules.filter(m => m.deps.length > 0).map((m) => (
                <div key={m.id} className="rounded-lg border p-3">
                  <div className="flex items-center gap-2 mb-2">
                    {MODULE_ICONS[m.id] || <Box className="size-3.5" />}
                    <code className="text-xs font-bold">{m.id}</code>
                  </div>
                  <div className="space-y-1">
                    {m.deps.map(d => (
                      <div key={d} className="flex items-center gap-2 text-[10px]">
                        <ArrowDown className="size-3 text-muted-foreground" />
                        <code className="font-mono text-muted-foreground">{d}</code>
                        {modules.find(mod => mod.id === d) ? (
                          <Badge variant="outline" className="text-[9px] px-1 py-0 text-emerald-600">{CATEGORY_COLORS[modules.find(mod => mod.id === d)!.category]?.split(' ')[0] || ''}</Badge>
                        ) : (
                          <Badge variant="outline" className="text-[9px] px-1 py-0 text-destructive">missing</Badge>
                        )}
                      </div>
                    ))}
                  </div>
                </div>
              ))}
            </div>
            {modules.filter(m => m.deps.length === 0).length > 0 && (
              <p className="text-xs text-muted-foreground mt-3">
                Leaf-Module (keine Abhängigkeiten): {modules.filter(m => m.deps.length === 0).map(m => m.id).join(', ')}
              </p>
            )}
          </CardContent>
        </Card>
      </motion.div>

      {/* Freshness Config — dynamic */}
      <motion.div {...ARCH_FADE}>
        <Card>
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="flex items-center gap-2 text-base"><Activity className="size-4 text-muted-foreground" /> Freshness Timeouts</CardTitle>
              <Badge variant="outline" className="text-[10px]">Source: kDefaultFreshness[]</Badge>
            </div>
            <CardDescription>Modulspezifische Freshness-Timeouts aus module_system.cpp</CardDescription>
          </CardHeader>
          <CardContent>
            <Table>
              <TableHeader>
                <TableRow className="bg-muted/50 hover:bg-muted/50">
                  <TableHead className="text-xs font-semibold">Modul</TableHead>
                  <TableHead className="text-xs font-semibold">Freshness (ms)</TableHead>
                  <TableHead className="text-xs font-semibold hidden sm:table-cell">Kategorie</TableHead>
                  <TableHead className="text-xs font-semibold hidden sm:table-cell">Feature Flag</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {modules.sort((a, b) => a.freshness - b.freshness).map((m) => (
                  <TableRow key={m.id}>
                    <TableCell>
                      <div className="flex items-center gap-2">
                        {MODULE_ICONS[m.id] || <Box className="size-3" />}
                        <code className="text-xs font-mono font-semibold">{m.id}</code>
                      </div>
                    </TableCell>
                    <TableCell>
                      <code className="text-xs font-mono">{m.freshness === 0 ? '∞ (Infrastruktur)' : `${m.freshness} ms`}</code>
                      {m.freshness > 0 && (
                        <Progress value={Math.min(100, (m.freshness / 10000) * 100)} className="h-1 mt-1 w-24" />
                      )}
                    </TableCell>
                    <TableCell className="hidden sm:table-cell">
                      <Badge variant="outline" className={`text-[10px] px-1.5 py-0 border ${CATEGORY_COLORS[m.category]}`}>{m.category}</Badge>
                    </TableCell>
                    <TableCell className="hidden sm:table-cell">
                      <code className="text-[10px] font-mono text-muted-foreground">{m.featureFlag}</code>
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </CardContent>
        </Card>
      </motion.div>
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
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base"><Archive className="size-4 text-muted-foreground" /> Firmware-Architektur PDF</CardTitle>
          <CardDescription>Umfassende Dokumentation der Firmware-Architektur, Module und Schnittstellen</CardDescription>
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
          <Button onClick={handleDownload} disabled={downloading} className="w-full sm:w-auto">
            {downloading ? <><Loader2 className="size-4 animate-spin" /> Wird heruntergeladen...</> : downloaded ? <><CheckCircle2 className="size-4" /> Erneut herunterladen</> : <><Download className="size-4" /> PDF herunterladen</>}
          </Button>
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base"><LayoutDashboard className="size-4 text-muted-foreground" /> Architektur-Diagramm</CardTitle>
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
              <ArrowRight className="size-3" /><span className="text-[10px]">Datenfluss von links nach rechts</span><ArrowRight className="size-3" />
            </div>
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2 text-base"><FolderDown className="size-4 text-muted-foreground" /> Dokumentation & Ressourcen</CardTitle>
        </CardHeader>
        <CardContent className="space-y-2">
          {[
            { name: 'GitHub Repository', desc: 'Quellcode, Issues & Pull Requests', href: 'https://github.com/Keule/Z.AI_WORK', icon: <Github className="size-4" /> },
            { name: 'ADR-007: Zwei-Task-Architektur', desc: 'Aktuelle Task-Aufteilung (ersetzt ADR-002)', href: 'https://github.com/Keule/Z.AI_WORK/blob/main/esp32_project/docs/adr/ADR-007-two-task-architecture.md', icon: <FileText className="size-4" /> },
            { name: 'ADR-ZAI-001: Git/GitHub Workflow', desc: 'Agenten-Workflow Regeln & Backup-Strategie', href: 'https://github.com/Keule/Z.AI_WORK/blob/main/esp32_project/docs/adr/ADR-ZAI-001-git-github-workflow.md', icon: <FileText className="size-4" /> },
            { name: 'AgOpenGPS Dokumentation', desc: 'Externe API-Referenz und Protokollspezifikation', href: 'https://github.com/AgOpenGPS', icon: <ExternalLink className="size-4" /> },
          ].map((link) => (
            <a key={link.name} href={link.href} target="_blank" rel="noopener noreferrer" className="flex items-center gap-3 rounded-lg border p-3 hover:bg-muted/50 transition-colors group">
              <div className="rounded-md bg-muted p-2 group-hover:bg-background transition-colors">{link.icon}</div>
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

// ─── MAIN PAGE ───────────────────────────────────────────────────────────────

export default function DashboardPage() {
  const { modules, loading: modLoading, error: modError, refetch: modRefetch } = useModules()
  const { tasks, epics, stats, loading: blLoading, error: blError, refetch: blRefetch } = useBacklog()
  const { commits, loading: cmtLoading } = useCommits()
  const [activeTab, setActiveTab] = useState('overview')

  const anyLoading = modLoading || blLoading || cmtLoading

  return (
    <div className="min-h-screen flex flex-col bg-background">
      {/* Header */}
      <header className="sticky top-0 z-50 border-b bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
        <div className="container mx-auto max-w-7xl flex items-center justify-between px-4 py-3">
          <div className="flex items-center gap-3">
            <Cpu className="size-6 text-primary" />
            <div>
              <h1 className="text-lg font-bold tracking-tight">Z.AI ESP32 Dashboard</h1>
              <p className="text-[10px] text-muted-foreground hidden sm:block">AG OpenGPS · Autosteuerung · Firmware Refactoring</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {anyLoading && <Loader2 className="size-4 animate-spin text-muted-foreground" />}
            <a href="https://github.com/Keule/Z.AI_WORK" target="_blank" rel="noopener noreferrer">
              <Button variant="ghost" size="icon" className="size-9" aria-label="GitHub">
                <Github className="size-4" />
              </Button>
            </a>
            <ThemeToggle />
          </div>
        </div>
      </header>

      {/* Content */}
      <main className="flex-1 container mx-auto max-w-7xl px-4 py-6">
        <Tabs value={activeTab} onValueChange={setActiveTab}>
          <TabsList className="mb-6 flex-wrap h-auto gap-1">
            <TabsTrigger value="overview" className="text-xs gap-1.5"><LayoutDashboard className="size-3.5" /> Overview</TabsTrigger>
            <TabsTrigger value="modules" className="text-xs gap-1.5"><Box className="size-3.5" /> Module</TabsTrigger>
            <TabsTrigger value="backlog" className="text-xs gap-1.5"><ListTodo className="size-3.5" /> Backlog</TabsTrigger>
            <TabsTrigger value="architecture" className="text-xs gap-1.5"><Layers className="size-3.5" /> Architektur</TabsTrigger>
            <TabsTrigger value="ai-requests" className="text-xs gap-1.5"><Sparkles className="size-3.5" /> AI Requests</TabsTrigger>
            <TabsTrigger value="downloads" className="text-xs gap-1.5"><FolderDown className="size-3.5" /> Downloads</TabsTrigger>
          </TabsList>

          <TabsContent value="overview">
            {anyLoading ? (
              <div className="grid grid-cols-2 md:grid-cols-4 gap-4">{[1,2,3,4].map(i => <LoadingCard key={i} />)}</div>
            ) : (
              <OverviewTab modules={modules} backlogStats={stats} commits={commits} />
            )}
          </TabsContent>

          <TabsContent value="modules">
            <ModuleTab modules={modules} loading={modLoading} error={modError} refetch={modRefetch} />
          </TabsContent>

          <TabsContent value="backlog">
            <BacklogTab tasks={tasks} epics={epics} stats={stats} loading={blLoading} error={blError} refetch={blRefetch} />
          </TabsContent>

          <TabsContent value="architecture">
            {modLoading ? <div className="grid grid-cols-3 gap-3">{[1,2,3].map(i => <LoadingCard key={i} />)}</div> : <ArchitectureTab modules={modules} />}
          </TabsContent>

          <TabsContent value="ai-requests">
            {anyLoading ? <div className="flex items-center justify-center py-20"><Loader2 className="size-6 animate-spin" /></div> : <AIRequestTab modules={modules} tasks={tasks} />}
          </TabsContent>

          <TabsContent value="downloads">
            <DownloadsTab />
          </TabsContent>
        </Tabs>
      </main>

      {/* Footer */}
      <footer className="border-t mt-auto">
        <div className="container mx-auto max-w-7xl px-4 py-3 flex items-center justify-between text-xs text-muted-foreground">
          <span>Z.AI ESP32 Dashboard — Daten dynamisch aus Quellcode geladen</span>
          <span className="hidden sm:inline">{modules.length} Module · {tasks.length} Tasks · {commits.length} Commits</span>
        </div>
      </footer>
    </div>
  )
}
