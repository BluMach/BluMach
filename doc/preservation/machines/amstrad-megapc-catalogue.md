# Amstrad Mega PC catalogue evidence

Spanish is the source text for the English, French, Italian and Portuguese
translations.

| Claim | Source and scope |
| --- | --- |
| PC7386SX main unit and Sega-licensed games card | Amstrad, *MEGAPC3H40 / PC14DSM Service Manual*, PDF p. 3, preserved in the local library. The service note refers repairs of the PC section to PC7386SX manual 273930. [ACME archive](https://acpc.me/). |
| 386SX at 25 MHz, 68000 at 8 MHz, 1 MB RAM expandable to 16 MB, 40 MB disk, 1.44 MB floppy, AdLib audio in PC mode and stereo in Mega Drive mode | Same service manual, PDF p. 2. These specifications describe the documented MEGAPC3H40 configuration. |
| Front slider, shared 15/31 kHz dual-sync monitor and physical specimen | [Centre for Computing History, CH2534](https://www.computinghistory.org.uk/det/2534/Amstrad-Mega-PC-386SX/). |
| Complete UK system advertised in January 1993 at £699 excluding VAT | Admincure in *Personal Computer World*, January 1993, p. 493. A dealer offer anchors the date and price. [Magazine archive](https://worldradiohistory.com/UK/Personal-Computer-World/). |
| Two 64 KiB PC ROM files. The interleaved 32 KiB VGA and 64 KiB system regions each have a zero 8-bit sum. | 86Box ROM collection and static byte comparison in the local library. System region starts at chip offset `0x8000` and contains `AMSTRAD PC7386SX`. [ROM collection](https://github.com/86Box/roms/tree/c4cd8da9034a8557896ff6a975ae58499a83bb80/machines/megapc). |
| Existing PC profile and limits | [BluMach loader](https://github.com/BluMach/BluMach/blob/88f74ba30693e14b0003c656fa3ac578c3d85eed/src/machine/m_at_386sx.c#L788) and the WD90C11 video loader. [PCem’s README](https://github.com/sarah-walker-pcem/pcem/blob/dev/README.md) explicitly scopes its Mega PC support to the PC side. BluMach boot and peripheral validation are pending. The games card and video/audio switching need separate work. |

The catalogue photograph is Staffan Vilcans’s photograph from Retrogathering
2010, retouched by Ubcule. A catalogue-size copy is included
under [CC BY-SA 2.0](https://creativecommons.org/licenses/by-sa/2.0/).
[Photograph and modification history](https://commons.wikimedia.org/wiki/File:Amstrad_Mega_PC_(5058669424)_(retouched).jpg).

The Mega Plus remains a separate research lead. MAME set names and a secondary
price list establish leads for the 486SLC variant. Physical configuration and
retail availability still need primary evidence.
