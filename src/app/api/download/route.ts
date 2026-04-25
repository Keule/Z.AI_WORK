import { NextResponse } from 'next/server'
import { readFile } from 'fs/promises'
import { join } from 'path'

export async function GET() {
  try {
    const filePath = join(process.cwd(), 'public', 'ZAI_GPS_Firmware_Architecture.pdf')
    const buffer = await readFile(filePath)

    return new NextResponse(buffer, {
      headers: {
        'Content-Type': 'application/pdf',
        'Content-Disposition': 'attachment; filename="ZAI_GPS_Firmware_Architecture.pdf"',
        'Content-Length': buffer.length.toString(),
      },
    })
  } catch {
    return NextResponse.json({ error: 'PDF nicht gefunden' }, { status: 404 })
  }
}
