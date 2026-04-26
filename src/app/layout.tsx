import type { Metadata } from "next";
import { Geist, Geist_Mono } from "next/font/google";
import "./globals.css";
import { Toaster } from "@/components/ui/toaster";
import { Providers } from "@/components/providers";

const geistSans = Geist({
  variable: "--font-geist-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

export const metadata: Metadata = {
  title: "ZAI_GPS Dashboard — ESP32-S3 Firmware",
  description: "Übersichtsdashboard für das ZAI_GPS ESP32-S3 Firmware-Projekt. AgOpenGPS kompatible Steuerung auf LilyGO T-ETH-Lite-S3 mit FreeRTOS.",
  keywords: ["ZAI_GPS", "ESP32-S3", "AgOpenGPS", "LilyGO", "FreeRTOS", "GNSS", "NTRIP", "Firmware"],
  authors: [{ name: "ZAI_GPS Team" }],
  icons: {
    icon: "https://z-cdn.chatglm.cn/z-ai/static/logo.svg",
  },
  openGraph: {
    title: "ZAI_GPS Dashboard — ESP32-S3 Firmware",
    description: "Übersichtsdashboard für das ZAI_GPS ESP32-S3 Firmware-Projekt",
    siteName: "ZAI_GPS",
    type: "website",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="de" suppressHydrationWarning>
      <body
        className={`${geistSans.variable} ${geistMono.variable} antialiased bg-background text-foreground`}
      >
        <Providers>
          {children}
          <Toaster />
        </Providers>
      </body>
    </html>
  );
}
