/*
 * Conventional Memories 3inONEder for Toshiba portables.
 *
 * The documented sound section uses a Yamaha YMF262-M/YAC512-M pair.  This
 * independent implementation also models the publicly documented XTIDE
 * option-ROM interface and Lo-tech-compatible 8-bit IDE transport. Firmware
 * is supplied separately by the user. The optional joystick
 * follows the documented IBM Game Control Adapter schematic and Ethernet
 * uses the documented 8-bit, partially NE2000-compatible default setup.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <86box/conventional_3inoneder.h>
#include <86box/device.h>
#include <86box/hdc_ide.h>
#include <86box/gameport.h>
#include <86box/mem.h>
#include <86box/net_ne2000.h>
#include <86box/rom.h>
#include <86box/snd_opl.h>
#include <86box/sound.h>
#include <86box/toshiba_aform.h>

typedef struct {
    toshiba_aform_slot_t *slot;
    fm_drv_t              opl;
    uint16_t              ports[2];
    uint8_t               port_count;
    uint8_t               opl_enabled;

    void                 *ide_board;
    int                   ide_board_index;
    uint8_t               ide_data_high;
    uint8_t              *xtide_rom;
    mem_mapping_t         xtide_mapping;
    uint16_t              xtide_base;
    uint8_t               xtide_mapped;
    uint8_t               xtide_io_registered;
} conventional_3inoneder_t;

static void conventional_3inoneder_close(void *priv);

#define CONVENTIONAL_3INONEDER_XTIDE_ROM_SIZE 0x2800
#define CONVENTIONAL_3INONEDER_XTIDE_ROM_BASE 0xc8000
#define CONVENTIONAL_3INONEDER_IDE_BOARD       1
#define CONVENTIONAL_3INONEDER_AT_INT_ROM \
    "roms/machines/t5200/3inoneder/3inoneder-at-int.bin"
#define CONVENTIONAL_3INONEDER_AT320INT_ROM \
    "roms/machines/t5200/3inoneder/3inoneder-at320int.bin"

static void
conventional_3inoneder_get_buffer(int32_t *buffer, uint16_t len, void *priv)
{
    conventional_3inoneder_t *card = (conventional_3inoneder_t *) priv;
    const int32_t *opl_buffer = card->opl.update(card->opl.priv);

    for (uint16_t i = 0; i < len * 2; i++)
        buffer[i] += opl_buffer[i];
    card->opl.reset_buffer(card->opl.priv);
}

static int
conventional_3inoneder_add_opl_port(conventional_3inoneder_t *card,
                                    uint16_t port)
{
    if (!toshiba_aform_slot_set_io_handler(card->slot, port, 4,
                                            card->opl.read, card->opl.write,
                                            card->opl.priv))
        return 0;

    card->ports[card->port_count++] = port;
    return 1;
}

static uint8_t
conventional_3inoneder_xtide_read(uint16_t port, void *priv)
{
    conventional_3inoneder_t *card = (conventional_3inoneder_t *) priv;
    const uint8_t reg = port & 0x0f;
    uint16_t value = 0xffff;

    switch (reg) {
        case 0x00:
            value = ide_readw(0, card->ide_board);
            card->ide_data_high = value >> 8;
            break;
        case 0x01 ... 0x07:
            value = ide_readb(reg, card->ide_board);
            break;
        case 0x08:
            value = card->ide_data_high;
            break;
        case 0x0e:
            value = ide_read_alt_status(0, card->ide_board);
            break;
        default:
            break;
    }
    return value & 0xff;
}

static void
conventional_3inoneder_xtide_write(uint16_t port, uint8_t value, void *priv)
{
    conventional_3inoneder_t *card = (conventional_3inoneder_t *) priv;
    const uint8_t reg = port & 0x0f;

    switch (reg) {
        case 0x00:
            ide_writew(0, value | (card->ide_data_high << 8), card->ide_board);
            break;
        case 0x01 ... 0x07:
            ide_writeb(reg, value, card->ide_board);
            break;
        case 0x08:
            card->ide_data_high = value;
            break;
        case 0x0e:
            ide_write_devctl(0, value, card->ide_board);
            break;
        default:
            break;
    }
}

static uint8_t
conventional_3inoneder_xtide_rom_read(uint32_t address, void *priv)
{
    conventional_3inoneder_t *card = (conventional_3inoneder_t *) priv;
    return card->xtide_rom[address - CONVENTIONAL_3INONEDER_XTIDE_ROM_BASE];
}

static int
conventional_3inoneder_init_xtide(conventional_3inoneder_t *card, int variant)
{
    const char *rom_path;
    FILE *rom_file;

    switch (variant) {
        case CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300:
            rom_path = CONVENTIONAL_3INONEDER_AT_INT_ROM;
            card->xtide_base = 0x300;
            break;
        case CONVENTIONAL_3INONEDER_XTIDE_AT320INT_320:
            rom_path = CONVENTIONAL_3INONEDER_AT320INT_ROM;
            card->xtide_base = 0x320;
            break;
        default:
            return 1;
    }

    if (!(rom_file = rom_fopen(rom_path, "rb")) ||
        !(card->xtide_rom = malloc(CONVENTIONAL_3INONEDER_XTIDE_ROM_SIZE))) {
        if (rom_file)
            fclose(rom_file);
        return 0;
    }
    if (fread(card->xtide_rom, 1, CONVENTIONAL_3INONEDER_XTIDE_ROM_SIZE,
              rom_file) != CONVENTIONAL_3INONEDER_XTIDE_ROM_SIZE ||
        card->xtide_rom[0] != 0x55 || card->xtide_rom[1] != 0xaa ||
        card->xtide_rom[2] != 0x14) {
        fclose(rom_file);
        free(card->xtide_rom);
        card->xtide_rom = NULL;
        return 0;
    }
    fclose(rom_file);

    card->ide_board = ide_xtide_init_board(CONVENTIONAL_3INONEDER_IDE_BOARD);
    card->ide_board_index = CONVENTIONAL_3INONEDER_IDE_BOARD;
    if (!card->ide_board ||
        !toshiba_aform_slot_add_mapping(card->slot, &card->xtide_mapping,
                                        CONVENTIONAL_3INONEDER_XTIDE_ROM_BASE,
                                        CONVENTIONAL_3INONEDER_XTIDE_ROM_SIZE,
                                        conventional_3inoneder_xtide_rom_read,
                                        NULL, card->xtide_rom,
                                        MEM_MAPPING_IS_ROM, card))
        return 0;

    card->xtide_mapped = 1;
    if (!toshiba_aform_slot_set_io_handler(card->slot, card->xtide_base, 16,
                                            conventional_3inoneder_xtide_read,
                                            conventional_3inoneder_xtide_write,
                                            card))
        return 0;
    card->xtide_io_registered = 1;
    return 1;
}

static void *
conventional_3inoneder_init(const device_t *info)
{
    const conventional_3inoneder_params_t *params = device_get_common_priv();
    conventional_3inoneder_t *card;

    (void) info;
    if (!params || !params->machine ||
        !(card = calloc(1, sizeof(*card))))
        return NULL;

    card->slot = toshiba_aform_slot_get(params->machine);
    if (!toshiba_aform_slot_is_for_machine(card->slot, "t5200") ||
        !toshiba_aform_slot_has_signals(card->slot, TOSHIBA_AFORM_SIGNAL_IO)) {
        free(card);
        return NULL;
    }

    /* Ethernet occupies 300h on version C cards, so its documented XTIDE
       configuration must be the 320h image. */
    if (params->ethernet && params->xtide == CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300) {
        free(card);
        return NULL;
    }

    if (params->opl_io != CONVENTIONAL_3INONEDER_OPL_DISABLED) {
        if (!fm_driver_get_ex(FM_YMF262, &card->opl, 3)) {
            free(card);
            return NULL;
        }
        card->opl_enabled = 1;
        switch (params->opl_io) {
            case CONVENTIONAL_3INONEDER_OPL_220:
                conventional_3inoneder_add_opl_port(card, 0x220);
                break;
            case CONVENTIONAL_3INONEDER_OPL_240:
                conventional_3inoneder_add_opl_port(card, 0x240);
                break;
            case CONVENTIONAL_3INONEDER_OPL_388_AND_220:
                conventional_3inoneder_add_opl_port(card, 0x388);
                conventional_3inoneder_add_opl_port(card, 0x220);
                break;
            case CONVENTIONAL_3INONEDER_OPL_388:
            default:
                conventional_3inoneder_add_opl_port(card, 0x388);
                break;
        }
    }

    if ((!card->port_count && params->xtide == CONVENTIONAL_3INONEDER_XTIDE_NONE) ||
        !conventional_3inoneder_init_xtide(card, params->xtide)) {
        conventional_3inoneder_close(card);
        return NULL;
    }

    if (params->joystick)
        gameport_add(&gameport_201_device);
    if (params->ethernet)
        device_add(&conventional_3inoneder_ne2000_8bit_device);

    if (card->opl_enabled)
        music_add_handler(conventional_3inoneder_get_buffer, card);
    return card;
}

static void
conventional_3inoneder_close(void *priv)
{
    conventional_3inoneder_t *card = (conventional_3inoneder_t *) priv;

    for (uint8_t i = 0; i < card->port_count; i++)
        toshiba_aform_slot_remove_io_handler(card->slot, card->ports[i], 4,
                                              card->opl.read, card->opl.write,
                                              card->opl.priv);
    if (card->xtide_io_registered)
        toshiba_aform_slot_remove_io_handler(card->slot, card->xtide_base, 16,
                                              conventional_3inoneder_xtide_read,
                                              conventional_3inoneder_xtide_write,
                                              card);
    if (card->xtide_mapped)
        toshiba_aform_slot_remove_mapping(card->slot, &card->xtide_mapping);
    if (card->ide_board)
        ide_xtide_close_board(card->ide_board_index);
    free(card->xtide_rom);
    free(card);
}

const device_t conventional_3inoneder_device = {
    .name          = "Conventional Memories 3inONEder",
    .internal_name = "conventional_3inoneder",
    .flags         = 0,
    .local         = 0,
    .init          = conventional_3inoneder_init,
    .close         = conventional_3inoneder_close,
    .reset         = NULL,
    .available     = NULL,
    .speed_changed = NULL,
    .force_redraw  = NULL,
    .config        = NULL
};
