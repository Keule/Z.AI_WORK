import { NextResponse } from 'next/server'
import { readFileSync, existsSync } from 'fs'
import { join } from 'path'
import YAML from 'js-yaml'

const BACKLOG_DIR = join(process.cwd(), 'esp32_project', 'backlog')
const INDEX_YAML = join(BACKLOG_DIR, 'index.yaml')

interface YamlTask {
  id: string
  title: string
  file: string
  epic_id: string
  status: 'open' | 'in_progress' | 'blocked' | 'done'
  priority: 'high' | 'medium' | 'low'
  delivery_mode: 'hardware_required' | 'firmware_only' | 'mixed'
  task_category: string
  dependencies: string[]
}

interface YamlEpic {
  id: string
  title: string
  file: string
  task_ids: string[]
}

const STATUS_MAP: Record<string, string> = {
  open: 'Offen',
  in_progress: 'In Arbeit',
  blocked: 'Offen',
  done: 'Erledigt',
}

const PRIORITY_MAP: Record<string, string> = {
  high: 'Hoch',
  medium: 'Mittel',
  low: 'Niedrig',
}

const TASK_CATEGORY_MAP: Record<string, string> = {
  runtime_stability: 'Laufzeitstabilität',
  sensor_safety: 'Sensor & Sicherheit',
  platform_reuse: 'Plattform & Wiederverwendung',
  feature_expansion: 'Funktionserweiterung',
}

export async function GET() {
  try {
    if (!existsSync(INDEX_YAML)) {
      return NextResponse.json({ error: 'Backlog index.yaml not found' }, { status: 404 })
    }

    const raw = readFileSync(INDEX_YAML, 'utf-8')
    const data = YAML.load(raw) as { epics: YamlEpic[]; tasks: YamlTask[] }

    if (!data || !data.epics || !data.tasks) {
      return NextResponse.json({ error: 'Invalid YAML structure' }, { status: 500 })
    }

    // Build epic lookup
    const epicMap = new Map(data.epics.map(e => [e.id, e]))

    // Transform tasks
    const tasks = data.tasks.map(t => ({
      id: t.id,
      title: t.title,
      file: t.file,
      epic: t.epic_id,
      epicName: epicMap.get(t.epic_id)?.title || t.epic_id,
      epicNameDe: TASK_CATEGORY_MAP[epicMap.get(t.epic_id)?.title?.toLowerCase().replace(/\s+/g, '_') || ''] || epicMap.get(t.epic_id)?.title || t.epic_id,
      priority: PRIORITY_MAP[t.priority] || t.priority,
      priorityRaw: t.priority,
      status: STATUS_MAP[t.status] || t.status,
      statusRaw: t.status,
      deliveryMode: t.delivery_mode,
      taskCategory: t.task_category,
      taskCategoryDe: TASK_CATEGORY_MAP[t.task_category] || t.task_category,
      dependencies: t.dependencies || [],
    }))

    // Transform epics
    const epics = data.epics.map(e => ({
      id: e.id,
      title: e.title,
      file: e.file,
      taskIds: e.task_ids,
      taskCount: e.task_ids.length,
      taskCountDone: data.tasks.filter(t => t.epic_id === e.id && t.status === 'done').length,
      taskCountOpen: data.tasks.filter(t => t.epic_id === e.id && t.status !== 'done').length,
    }))

    // Stats
    const stats = {
      total: tasks.length,
      done: tasks.filter(t => t.statusRaw === 'done').length,
      open: tasks.filter(t => t.statusRaw === 'open').length,
      inProgress: tasks.filter(t => t.statusRaw === 'in_progress').length,
      blocked: tasks.filter(t => t.statusRaw === 'blocked').length,
      byPriority: {
        hoch: tasks.filter(t => t.priorityRaw === 'high').length,
        mittel: tasks.filter(t => t.priorityRaw === 'medium').length,
        niedrig: tasks.filter(t => t.priorityRaw === 'low').length,
      },
    }

    return NextResponse.json({ tasks, epics, stats })
  } catch (err) {
    console.error('Backlog API error:', err)
    return NextResponse.json({ error: 'Failed to parse backlog' }, { status: 500 })
  }
}
