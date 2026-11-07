# PC5386SX catalogue evidence

7 November 2026. Adds a visible historical research card to the existing Series 5
PR, with Spanish originals and equivalent English, French, Italian and Portuguese.
No emulator changes, FPGA results, firmware execution or public images.

## Claims and source locators

| Claim | Source and locator | Evidence / decision |
| --- | --- | --- |
| PC5386 proposed as the higher-end 386SX member of the 5000 family, without a launch date | [L. C., Science & Vie Micro 86, September 1991, p. 76](https://archive.org/details/science-et-vie-micro-086), final column, PC5386 paragraph | Contemporary announcement; not a first-sale date. Preserved scan leaf 77 and cover, visually read |
| Announced clock: 16 MHz | Same paragraph | Documented announcement; retained alongside secondary 20 MHz |
| 5386 listed as 80386SX, VGA, AT | [Cliff Lawson, General notes on Amstrad PCs](https://www.freetimeweb.nl/home/computer/alt/lawsons-amstrad-computer_site/web.ukonline.co.uk/cliff.lawson/general.htm), model table | Manufacturer-associated author; does not specify clock or RAM |
| A PC5386SX used as a Windows 3.11 network workstation, reporting BIOS AM53004 and integrated Chips & Technologies video | [Peter Fitzpatrick, 17 January 1999](https://www.freetimeweb.nl/home/computer/alt/lawsons-amstrad-computer_site/web.ukonline.co.uk/cliff.lawson/questd.htm), dated question and reply | Direct owner testimony, not inspected board or ROM bytes |
| A PC5386SXHD with failed floppy drive | [Keith, 19 August 2000](https://www.freetimeweb.nl/home/computer/alt/lawsons-amstrad-computer_site/web.ukonline.co.uk/cliff.lawson/questg.htm), dated question | Independent owner testimony. Reported name retained as search alias, not proof of a separate board/firmware variant |
| 20 MHz, 2 MB, VGA, 1.44 MB 3.5-inch floppy, unspecified hard disk, two 16-bit ISA slots, serial/parallel | [Fabien Neck / system-cfg, Amstrad PC5386 sheet preserved by ACPC](https://acpc.me/), one-page PDF | Secondary record explicitly says the machine is not in the author's collection. Clock conflicts with contemporary announcement; other values remain secondary pending physical/manual corroboration |
| No dedicated BluMach registration | [BluMach machine sources at 88f74ba3](https://github.com/BluMach/BluMach/tree/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine), machine_table.c and m_amstrad.c | Source inspection, no boot test |

The owner reports support commercial machines in use in Australia by their
respective dates. They do not establish a 1991 sale, exact launch date, price,
named OEM, predecessor/successor relationship or ROM interchangeability.
The catalogue period 1991 refers to the announcement; the family table retains
its unknown sale-date cell. No price or named OEM is added.

## Preservation references

Existing machine assets in `amstrad/pc5386sx`: `pc5386sx-round4-1` (Fitzpatrick),
`pc5386sx-round4-2` (Keith), `pc5386sx-doc-3` (system-cfg PDF). Original bytes,
hashes, rights and provenance remain intact.

New shared assets: `amstrad-pc5386-editorial-svm-page76`,
`amstrad-pc5386-editorial-svm-cover`, `amstrad-pc5386-editorial-svm-scandata`,
`amstrad-pc5386-editorial-svm-ocr`, `amstrad-pc5386-editorial-svm-metadata`,
`amstrad-pc5386-editorial-lawson-general`, and the claim dossier. The two images
are Internet Archive-rendered JPEGs of the scan, not locally edited pictures or
original JP2 files. Only the cover and p. 76 were visually reviewed; the full PDF
was not acquired. OCR is a navigation aid. The article photograph depicts a
PC5286 Games Pack and is not used as a PC5386SX illustration.

The wiki source index is preserved as a lead, not factual corroboration.
Source-code snapshots are reused from `amstrad/_shared`, IDs
`amstrad-pc2000-blumach-machine-table-base` and
`amstrad-pc2000-blumach-machine-code-base`, after hash comparison.

## Corrections and unresolved details

The 16/20 MHz discrepancy is stated in every language; it does not demonstrate
two production variants. RAM/slots/floppy figures are explicitly attributed to
the secondary sheet in the technical section. Its 1991 date is not adopted as
a verified sale date. The earlier record's CMOS type 17 claim lacks support in
these sources and is withdrawn, retaining the earlier metadata snapshot.

AM53004 remains an owner's reported name. No dump has been acquired in this
record, and no size/hash can be assigned. A different ROM revision need not
contain this literal text. The earlier worklog's demand for that string is
superseded, with the original worklog preserved unchanged.

Lawson's response to the VGA query cites LK1 from his PC5286 manual. It is not
proof that PC5386SX board jumpers match, or that a PC5286 BIOS can substitute.
The C&T video chip and memory controller remain unidentified. MS-DOS 3.3 and
Counterpoint are not asserted as this model's original bundle without evidence.
No claim of historical nonexistence is made from missing local material.

## Implementation outlook, not validation

| Area | Outcome | Missing evidence |
| --- | --- | --- |
| Firmware | unknown/blocker | Attributable system and video dumps, layout, revisions |
| CPU/bus | compatible approximation | Generic 386SX/AT devices exist; clock and board mapping unresolved |
| Chipset/memory, interrupt/DMA/timers, reset | unknown/blocker | Board/chip identification, maps and reset behaviour |
| Keyboard, RTC/NVRAM | unknown/blocker | Controller, wiring and setup/storage layout |
| VGA | unknown/blocker | Exact C&T controller and video ROM |
| Floppy, hard disk, ports | unknown/blocker | Controllers, wiring and configuration; reported capacities do not define an implementation |

Current outcome: documentation only; implementation requires named hardware
research and firmware acquisition. No claim about an unreviewed current external
emulator is added. No FPGA code or validation is a BluMach evidence source.

The catalogue uses the existing schema and block order, retains existing machine
visibility, and adds no creation profile. Images await clear publication rights.
Builder, catalogue/source/family tests, website export, preservation hashes and
machine audits are required; results are recorded in the PR and local receipt.
Native GUI layout and firmware boot are not reviewed in this editorial task.
