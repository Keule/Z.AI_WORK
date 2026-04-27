import { NextResponse } from 'next/server'
import { execSync } from 'child_process'

const ESP32_DIR = 'esp32_project'

export async function GET() {
  try {
    // Get recent commits from the esp32_project subdirectory
    const gitLog = execSync(
      `git -C ${ESP32_DIR} log --format="%H|%h|%s|%ad" --date=short -20`,
      { encoding: 'utf-8', maxBuffer: 1024 * 1024 }
    )

    const commits = gitLog
      .trim()
      .split('\n')
      .filter(Boolean)
      .map(line => {
        const [fullHash, shortHash, message, date] = line.split('|')
        // Extract phase from commit message
        let phase = 'Sonstiges'
        const msg = message || ''
        if (msg.toLowerCase().includes('phase')) {
          const phaseMatch = msg.match(/Phase\s*[\d+]/i)
          if (phaseMatch) phase = `Phase ${phaseMatch[0].replace(/Phase\s*/i, '')}`
        }
        if (msg.toLowerCase().includes('adr-')) phase = 'ADR'
        if (msg.toLowerCase().includes('dashboard')) phase = 'Dashboard'
        if (msg.toLowerCase().includes('pr #')) phase = 'PR'
        if (msg.toLowerCase().includes('module')) phase = 'Modul'
        if (msg.toLowerCase().includes('refactor')) phase = 'Refactor'
        if (msg.toLowerCase().includes('debug')) phase = 'Debug'

        return {
          hash: shortHash,
          fullHash,
          message: msg.replace(/"/g, '&quot;'),
          date,
          phase,
        }
      })

    return NextResponse.json({ commits })
  } catch (err) {
    console.error('Commits API error:', err)
    return NextResponse.json({ error: 'Failed to get commits' }, { status: 500 })
  }
}
