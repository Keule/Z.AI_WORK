'use client'

import { useState } from 'react'
import { FileText, Download, Loader2, CheckCircle2, ExternalLink, Github } from 'lucide-react'

export default function Home() {
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
    <div className="min-h-screen flex flex-col bg-background">
      <main className="flex-1 flex items-center justify-center p-4">
        <div className="w-full max-w-md">
          <div className="rounded-2xl border bg-card p-8 shadow-lg">
            {/* Header */}
            <div className="flex items-center gap-3 mb-6">
              <div className="rounded-xl bg-primary/10 p-3">
                <FileText className="h-8 w-8 text-primary" />
              </div>
              <div>
                <h1 className="text-xl font-bold text-foreground">
                  ZAI_GPS Firmware
                </h1>
                <p className="text-sm text-muted-foreground">
                  Architektur-Übersicht
                </p>
              </div>
            </div>

            {/* Info */}
            <div className="rounded-lg bg-muted/50 p-4 mb-6 space-y-2">
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">Datei</span>
                <span className="font-mono text-xs text-foreground">ZAI_GPS_Firmware_Architecture.pdf</span>
              </div>
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">Größe</span>
                <span className="text-foreground">149 KB</span>
              </div>
              <div className="flex justify-between text-sm">
                <span className="text-muted-foreground">Typ</span>
                <span className="text-foreground">PDF Dokument</span>
              </div>
            </div>

            {/* Download Button */}
            <button
              onClick={handleDownload}
              disabled={downloading}
              className="w-full flex items-center justify-center gap-2 rounded-lg bg-primary px-4 py-3 text-sm font-semibold text-primary-foreground transition-all hover:bg-primary/90 disabled:opacity-50 cursor-pointer"
            >
              {downloading ? (
                <>
                  <Loader2 className="h-5 w-5 animate-spin" />
                  Wird heruntergeladen...
                </>
              ) : downloaded ? (
                <>
                  <CheckCircle2 className="h-5 w-5" />
                  Erneut herunterladen
                </>
              ) : (
                <>
                  <Download className="h-5 w-5" />
                  PDF herunterladen
                </>
              )}
            </button>

            {downloaded && (
              <p className="mt-3 text-center text-xs text-green-600 dark:text-green-400">
                ✓ Download erfolgreich gestartet
              </p>
            )}

            {/* Links */}
            <div className="mt-6 pt-4 border-t space-y-2">
              <a
                href="https://github.com/Keule/Z.AI_WORK"
                target="_blank"
                rel="noopener noreferrer"
                className="flex items-center gap-2 text-sm text-muted-foreground hover:text-foreground transition-colors"
              >
                <Github className="h-4 w-4" />
                GitHub Repository
                <ExternalLink className="h-3 w-3 ml-auto" />
              </a>
            </div>
          </div>

          <p className="mt-4 text-center text-xs text-muted-foreground">
            ESP32-S3 &middot; AgOpenGPS &middot; LilyGO T-ETH-Lite-S3
          </p>
        </div>
      </main>

      <footer className="border-t py-4 text-center text-xs text-muted-foreground">
        ZAI_GPS Firmware Architecture PDF Download
      </footer>
    </div>
  )
}
