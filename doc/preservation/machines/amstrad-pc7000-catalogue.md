# Amstrad PC7000 catalogue evidence — 2026-11-07

Scope: four visible historical cards (PC7286, PC7386SX, PC7486SLC-33,
PC7486SLC80) and a PC7000 family in Spanish, English, French, Italian and
Portuguese. Spanish is the editorial original. Existing schema, cards,
visibility, translation notices and section order remain intact. No emulator
implementation, creation profile, firmware execution or FPGA-derived validation.
Base reviewed: BluMach 88f74ba30693e14b0003c656fa3ac578c3d85eed.

## Preserved bibliography

Canonical source IDs below belong to `amstrad/_shared` unless otherwise stated.
Originals and full SHA-256 metadata remain in the official local library; no
unknown-rights original or firmware is included in Git. Pages cited were read;
scanned specification pages and the magazine cover/advertisements were checked
visually. The complete magazines/manuals have not been exhaustively analysed.

| ID | Source and consulted location |
| --- | --- |
| `amstrad-pc7000-editorial-pcreview` | [PC Review 13, November 1992](https://archive.org/details/pc-review-13), cover, Amstrad advertisement PDF p. 23, news “Amstrad makes it seven”, printed/PDF p. 24. Archive OCR PDF preserved. The advertisement page has no visible printed number; a secondary index calls it p. 22. Use the verified PDF locator. |
| `amstrad-pc6486-editorial-pcw199301` | [Personal Computer World, January 1993](https://worldradiohistory.com/UK/Personal-Computer-World/), Computers by Post advertisement, PDF p. 119. Reused existing full PDF, hash verified. |
| `amstrad-pc7000-editorial-pc7386-service` | [Amstrad PC7386SX Service Manual](https://archive.org/details/manual_PC7386SX_SM_AMSTRAD_EN), first publication October 1992, pp. 2–3 (specifications and board layout). Full 19-page PDF preserved. |
| `megapc:megapc-doc-4` | [Amstrad MegaPC3H40 / PC14DSM Service Manual](https://acpc.me/ACME/AMSTRAD_PRO/AMSTRAD_PC/DOCUMENTS_TECHNIQUES/SERVICE_MANUALS/), PDF p. 3, note for engineers. Existing 33-page PDF reused and hash checked; PC7386SX service reference 273930. |
| `amstrad-pc7000-editorial-slc33-technical` | [Amstrad PC7486SLC-33 Technical Reference Manual](https://banjosmods.wordpress.com/2021/04/18/amstrad-pc7486-slc33-manuals/), printed pp. 3–5, 11, 13 (PDF pp. 3–4, 7–8). Scans credited to Phipscube, reposted by Banjo on 18 April 2021. Full 31-page scan preserved from the linked ZIP. |
| `amstrad-pc7000-editorial-slc33-software` | Same landing page: Amstrad PC7486SLC-33 Software Installation Guide, printed pp. 1, 3 (PDF pp. 3–4). Full 16-page scan preserved. |
| `amstrad-pc7000-editorial-banjo-bios` | [Banjo, Amstrad PC7486-SLC33 BIOS Dump](https://banjosmods.wordpress.com/2021/04/17/amstrad-pc7486-slc33-bios-dump/), 17 April 2021. Owner report and uncertainty about dumping; HTML preserved. |
| `amstrad-pc7000-editorial-vogons` | [VOGONS, Amstrad Mega PC](https://www.vogons.org/viewtopic.php?p=887671#p887671), Dioxaz reply 67, 26 August 2020, corrected 27 August; keropi reply 71, 30 August. Browser-search rendered text extract preserved, including these replies. Direct HTTP acquisition returned 403; no raw HTML acquisition is claimed. |
| `amstrad-pc7000-editorial-gamopat` | [Dioxaz, Gamopat, 22 March 2023](https://www.gamopat-forum.com/t103701p30-toujours-a-la-recherche-des-logiciels-preinstalles-de-l-amstrad-mega-pc), owner's account distinguishes SLC33 acquisition from earlier SLC80 and reports French Counterpoint recovered from SLC80. Full HTML preserved. |
| `pc7286:pc7286-rom-1` | [86Box ROM collection c4cd8da9, pc7286](https://github.com/86Box/roms/tree/c4cd8da9034a8557896ff6a975ae58499a83bb80/machines/pc7286). Existing 128 KiB original, unchanged. Collection attribution is not an independently verified physical dump. |
| `amstrad-pc2000-blumach-machine-table-base` and `amstrad-pc7000-editorial-code-*` | [BluMach source at 88f74ba3](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src): machine_table.c (existing preserved snapshot), m_at_286.c, m_at_386sx.c, vid_cl54xx.c (new preserved snapshots). Implementation evidence only. |

ZIP, acquisition pages/metadata and the byte-observation report are additionally
registered. The intake is a research package, not a count of distinct manuals.

## Claims and editorial decisions

| Claim | Evidence / locator | Decision |
| --- | --- | --- |
| PC7000 aimed at beginners with preinstalled software and tutorial | Documented: PC Review PDF p. 23, Amstrad advertisement | Family history and both 1992 cards. No claim that marketing promises demonstrate ease of use. |
| PC7286 286/16 MHz, 1 MB, 40 MB, VGA, 1.44 MB floppy | Documented: same advertisement and p. 24 news | Hardware table. No inferred chipset or RAM maximum from emulator code. |
| Family Pack £799.99 including VAT, sound/speakers/joystick/six games; follows 5286/4386SX bundles | Documented: PC Review p. 24 | History, attributed to this offer in November 1992. Not a precise launch day or universal base-unit price. |
| Series starting price £529.99 excluding / £622.74 including VAT, valid 1 September 1992 | Documented: advertisement footer | Dossier only: not explicitly allocated by model. |
| PC7386SX 25 MHz; 1 MB/40 MB or 2 MB/80 MB; 16 MB maximum, four SIMM sockets; 256–512 KB video; two ISA16, two RS232, LPT, PS/2 | Documented: service manual p. 2; configurations corroborated by advertisement | Hardware and technical text. No named external OEM established. |
| PC7386SX £609 excluding VAT with 1 MB/40 MB and colour monitor | Documented: Computers by Post, PCW PDF p. 119 | History, explicitly January 1993 dealer price, not launch price. |
| Desktop versus Counterpoint software attribution | Amstrad advertisement/service manual name Desktop; PCW dealer advertisement names DOS 5 and Counterpoint for 7286 and 7386SX | Both retained in technical.market in all five languages. No invented factory revision or country explanation. |
| MegaPC main CPU unit is the same as PC7386SX | Documented: MegaPC manual PDF p. 3 refers servicing to PC7386SX manual 273930 | History states main-unit relationship only. No claim of identical ROMs or whole-product equivalence; no external OEM inferred. |
| PC7386SX two BIOS 27C512, LOW/HIGH | Documented: service board drawing p. 3 | Firmware entry. No PC7386SX-attributable dump registered here; MegaPC bytes remain MegaPC assets. |
| SLC33 33 MHz, 16-bit external bus, 1 KB internal cache, 16/64 KB external cache, 2/4 MB RAM to 16 MB, paired 30-pin SIMMs; 130/214 MB disks | Documented: technical manual pp. 3, 11, 13 | Card values; do not transfer to SLC80. CPU supplier not established by these pages. |
| SLC33 monitor/KB7(A)/PS2 mouse/DOS6/Windows3.1 bundle | Documented: software guide p. 3 | History. Exact commercial date/price left unknown; no date derived from firmware copyright. Period field uses an em dash for unknown. |
| SLC33 physical CL-GD5420-75QC-B | Owner observation: Banjo article | Attributed explicitly; not universalised across SLC variants. |
| SLC80 commercial identity and French Counterpoint | Owner observation: Dioxaz's 2023 Gamopat account and 2020 VOGONS physical dump | Separate research card; no CPU clock/disk capacity inferred from “80”. No precise sales chronology or named OEM invented. Period unknown. |
| SLC80 chip identification correction | Dioxaz VOGONS reply 67 corrects AM27C512 to ST M27C512 after repeat reads with ID checking | Preserve original archive README unchanged; add correction rather than silently replacing historical source. |
| SLC80 BIOS not interchangeable with a 386SX board as tested by keropi | Owner observation: reply 71 | Attributed failed test, not a universal proof of incompatibility or a BluMach test. No A20 hypothesis adopted as fact. |

## Read-only firmware observations

All offsets below refer to files, not runtime memory. Existing asset bytes,
rights, security flags and validation states are unchanged. No firmware was
executed, patched, antivirus-scanned, uploaded or committed.

- PC7286: 131072 bytes, SHA-256
  `4b74e46ef6e21d874af12239a8299ca114548884826e747b080302c781e14af7`.
  AMSTRAD PC7286 at 0x16964; PhoenixVIEW VGA string at 0x2D. The inherited
  loader maps 128 KiB at 0xE0000; GD5401 onboard device independently selects
  `BIOS_GD5401_ONBOARD_PATH` from the drsm35286 collection. A VGA string in the
  system file does not establish a correct video mapping or complete ROM set.
- SLC33: 64 KiB system image with AMSTRAD PC7486SLC V1.0 at 0x81A0.
  E000.ROM and E800.ROM each have 32768 bytes and identical SHA-256
  `66762871779dfa1968ab95c81630b405e2b717e4001b9a3baefa518fdf2731ed`.
  Count one distinct video image; physical chip coverage remains pending.
- SLC80: exact even=LO, odd=HI interleave reconstructs the existing 128 KiB
  combined image, SHA-256
  `edc0bb7e6b24e309768abaffe756019a6edb1d16b6a96695be4ad0317e0a67e7`.
  WD7600/486SLC version string at 0x15B7C; Paradise103226-414 at 0x2D.
  Older 64 KiB image matches neither half nor lane. No revision equivalence
  inferred. This differs from the preserved SLC33 firmware.

## Implementation boundary and viability

This is catalogue work, not a full fidelity audit. In the reviewed base,
pc7286 exists; PC7386 is an alias of megapc with MegaPC ROMs. No dedicated
pc7386sx, pc7486slc33 or pc7486slc80 entry was found. Source inspection is not
boot validation. No external emulator's current support is inferred from an
old closed feature request, and no FPGA core result feeds these cards.

| Component | PC7286 | PC7386SX | SLC33 | SLC80 |
| --- | --- | --- | --- | --- |
| Firmware | Preserved system file; video mapping unknown/blocker | Attributable dump unknown/blocker | System/video memory dumps; chip coverage unknown | Physical pair preserved; mapping pending |
| CPU / bus | 286 documented; existing model reusable subject to testing | 386SX documented; reusable CPU, board comparison pending | 486SLC/16-bit documented; configuration work needed | ROM identifies 486SLC; clock/board unknown |
| Chipset / RAM | SCAMP implementation only; physical comparison pending | WD board diagram available; full fidelity audit pending | SIMM topology documented; controller map pending | WD7600 firmware identifier only; map pending |
| IRQ / DMA / timers / reset | Inherited model, unvalidated | MegaPC reuse candidate, unvalidated | Unknown/blocker | Unknown/blocker |
| Keyboard / RTC | VL82C113 in code, unvalidated physically | PS/2 documented; implementation comparison pending | PS/2 documented; detailed map pending | Unknown/blocker |
| Video | Cross-machine ROM path unresolved | Manual and related MegaPC device; ROM attribution unresolved | Owner Cirrus observation; mapping work needed | Paradise ROM identifier; physical match pending |
| Floppy / disk / ports | Advertised drives; controller evidence incomplete | Manual and schematics; controller comparison pending | Manual capacities/ports; controller mapping pending | Configuration/controller mapping unknown |

All four: viable after named firmware/board research and validation, not
validated now. PC7386SX is presently documentation-only for its own attributable
firmware. Highest-value follow-ups: PC7286 physical ROM/video layout; PC7386SX
chip-label photographs and independent dumps; SLC33 board/chip mapping; SLC80
dated sales material and complete specimen photographs. Detailed address maps
and revision comparisons belong in a future Labbook article rather than longer
catalogue entries.

No publication-cleared machine photograph was established. New cards have no
invented media. Family and card visibility is enabled under the owner's later
instruction; existing visibility is unchanged. The official library remains
on G:, and synchronization is not declared complete.
