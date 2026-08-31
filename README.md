# Erebus

Ein objektbasiertes, rechtegebundenes Betriebssystem für x86_64-UEFI.
Kein Unix-Nachbau — eigener Startlader, eigener Kernel, eigene Konzepte.

## Grundidee

| Unix | Erebus |
|---|---|
| Alles ist eine Datei | Alles ist ein typisiertes Objekt |
| Pfade in einem globalen Namensraum | Nur Referenzen; kein globaler Namensraum |
| Rechte werden geprüft | Die Referenz *ist* das Recht (Capability) |
| Speichern in Dateien | Versionierte Snapshots des Objektgraphen |
| Byteströme (stdin/stdout, Pipes) | Typisierte Nachrichten |
| Anwendung öffnet Datei | Fenster ist eine Sicht auf ein Objekt |

Sicherheitsfolge: Ein Programm kann nichts benennen, was ihm nicht
übergeben wurde. Es gibt keinen Allmachtszustand („root"), keine
setuid-Kette und keinen Weg, sich Rechte zu *erwerben* — sie werden nur
weitergegeben, und dabei höchstens abgeschwächt.

## Bauen

Vorausgesetzt wird eine Linux-Umgebung (hier: WSL2 / Ubuntu) mit

    apt install clang lld nasm make qemu-system-x86 ovmf mtools \
                dosfstools xorriso gdb unifont python3-pil

Dann:

    make          # Lader, Kernel und startfähiges Abbild
    make run      # in QEMU starten, serielle Ausgabe im Terminal
    make shot     # kopflos starten, build/screen.png + build/serial.log
    make debug    # angehalten starten, wartet auf gdb an Port 1234
    make clean

Unter Windows aus dem Projektverzeichnis heraus:

    wsl -d Ubuntu -- bash -lc "cd /mnt/c/erebus && make run"

## Aufbau

    boot/            UEFI-Startlader (PE/COFF, MS-ABI)
      efi.h          von Hand geschriebene UEFI-Schnittstelle, kein gnu-efi
      boot.c         GOP, Dateisystem, ELF laden, ExitBootServices
    common/          von Lader UND Kernel benutzt
      bootinfo.h     Übergabestruktur
      elf64.h        ELF-Kopfstrukturen
    kernel/
      arch/x86_64/   start.S, linker.ld
      hw/            Gerätetreiber (bisher: serielle Schnittstelle)
      gfx/           Bildpuffer, Zeichensatz, Bildschirmkonsole
      lib/           kprintf, Speicherfunktionen, panic, Härtung
      include/eb/    Kopfdateien
    tools/           mkfont.py (Zeichensatz), ppm2png.py (Abzüge)

## Entscheidungen und ihre Gründe

**Eigener Startlader statt GRUB oder Limine.** Der Bootvorgang gehört
zum System. Kein Fremdcode zwischen Firmware und Kernel bedeutet auch:
keine fremde Vertrauensbasis in einem sicherheitsorientierten System.

**Kernel fest bei 2 MiB, identisch abgebildet.** UEFI bildet vor
`ExitBootServices` alles eins zu eins ab, also stimmen virtuelle und
physische Adresse überein. Das hält den Start nachvollziehbar. Der Umzug
in die obere Adresshälfte kommt, wenn eigene Seitentabellen stehen.

**Zeichensatz aus GNU Unifont, zur Bauzeit eingebettet.** Genau 8×16, kein
Rasterizer zur Laufzeit, keine Schriftdatei die gelesen werden müsste.
Deckt Latein inkl. Umlauten, Rahmen- und Blockzeichen ab.

**Serielle Schnittstelle als erste Ausgabe.** Sie funktioniert, bevor
irgendetwas anderes steht, und noch dann, wenn der Bildpuffer zerschossen
ist. QEMU leitet sie direkt ins Terminal.

**Kein SSE/MMX im Kernel.** Vektorregister müssten erst eingeschaltet und
bei jedem Kontextwechsel gesichert werden. Ohne sie ist der Wechsel
billiger und der Startpfad kürzer.

## Stand

* **M1 — erledigt.** Eigener UEFI-Lader, Kernel übernimmt Bildschirm und
  serielle Schnittstelle, Speicherkarte gelesen und geprüft (122 Bereiche,
  505 MiB frei bei 512 MiB Maschine).
* M2 — GDT, IDT, Ausnahmebehandlung, Zeitgeber, Panik-Bildschirm
* M3 — Seitenrahmenverwaltung, eigene Seitentabellen, Kernel-Heap, NX/SMEP/SMAP
* M4 — Objektspeicher und Capability-Tabelle
* M5 — Threads, Ablaufsteuerung, Nachrichten
* M6 — Nutzermodus (Ring 3), Prozesse, getrennte Adressräume
* M7 — Compositor, Fenster, Eingabe (PS/2)
* M8 — Desktop: Objektbrowser und Typ-Betrachter
* M9 — Beständigkeit: NVMe/AHCI, Snapshot und Wiederherstellung
* M10 — Start von echter Hardware per USB-Stick

## Hinweis zu Secure Boot

Der Lader ist nicht signiert. Auf echter Hardware muss Secure Boot
abgeschaltet sein. Eine eigene Signaturkette wäre ein Thema für später.
