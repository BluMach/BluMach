/* Conventional Memories 3inONEder OPL3-section contract tests. GPL-2.0-or-later. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <86box/device.h>
#include <86box/gameport.h>
#include <86box/hdc_ide.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/pic.h>
#include <86box/snd_opl.h>
#include <86box/sound.h>
#include <86box/toshiba_aform.h>

static void *common_priv;
static uint16_t io_base[4], io_size[4];
static uint32_t mapping_base, mapping_size;
static int io_count, io_remove_count, mapping_disable_count, music_handler_count;
static int gameport_add_count, ethernet_add_count;
static int xtide_board;

void *device_get_common_priv(void) { return common_priv; }
const device_t gameport_201_device = { 0 };
const device_t conventional_3inoneder_ne2000_8bit_device = { 0 };
void *gameport_add(const device_t *device)
{
    assert(device == &gameport_201_device);
    gameport_add_count++;
    return NULL;
}
void *device_add(const device_t *device)
{
    assert(device == &conventional_3inoneder_ne2000_8bit_device);
    ethernet_add_count++;
    return NULL;
}
void io_sethandler(uint16_t base, uint16_t size,
                   uint8_t (*inb)(uint16_t, void *),
                   uint16_t (*inw)(uint16_t, void *),
                   uint32_t (*inl)(uint16_t, void *),
                   void (*outb)(uint16_t, uint8_t, void *),
                   void (*outw)(uint16_t, uint16_t, void *),
                   void (*outl)(uint16_t, uint32_t, void *), void *priv)
{
    (void) inb; (void) inw; (void) inl; (void) outb; (void) outw; (void) outl; (void) priv;
    io_base[io_count] = base;
    io_size[io_count++] = size;
}
void io_removehandler(uint16_t base, uint16_t size,
                      uint8_t (*inb)(uint16_t, void *),
                      uint16_t (*inw)(uint16_t, void *),
                      uint32_t (*inl)(uint16_t, void *),
                      void (*outb)(uint16_t, uint8_t, void *),
                      void (*outw)(uint16_t, uint16_t, void *),
                      void (*outl)(uint16_t, uint32_t, void *), void *priv)
{
    (void) base; (void) size; (void) inb; (void) inw; (void) inl;
    (void) outb; (void) outw; (void) outl; (void) priv;
    io_remove_count++;
}
void mem_mapping_add(mem_mapping_t *mapping, uint32_t base, uint32_t size,
                     uint8_t (*read_b)(uint32_t, void *),
                     uint16_t (*read_w)(uint32_t, void *),
                     uint32_t (*read_l)(uint32_t, void *),
                     void (*write_b)(uint32_t, uint8_t, void *),
                     void (*write_w)(uint32_t, uint16_t, void *),
                     void (*write_l)(uint32_t, uint32_t, void *), uint8_t *exec,
                     uint32_t flags, void *priv)
{
    (void) mapping; (void) read_b; (void) read_w;
    (void) read_l; (void) write_b; (void) write_w; (void) write_l;
    (void) exec; (void) flags; (void) priv;
    mapping_base = base;
    mapping_size = size;
}
void mem_mapping_disable(mem_mapping_t *mapping)
{
    (void) mapping;
    mapping_disable_count++;
}
void picint_common(uint16_t num, int level, int set, uint8_t *irq_state)
{
    (void) num; (void) level; (void) set; (void) irq_state;
}
static uint8_t opl_read(uint16_t port, void *priv)
{
    (void) port; (void) priv;
    return 0xff;
}
static void opl_write(uint16_t port, uint8_t value, void *priv)
{
    (void) port; (void) value; (void) priv;
}
static int32_t opl_buffer[2];
static int32_t *opl_update(void *priv) { (void) priv; return opl_buffer; }
static void opl_reset(void *priv) { (void) priv; }
uint8_t fm_driver_get_ex(int chip_id, fm_drv_t *drv, int is_48k)
{
    assert(chip_id == FM_YMF262 && is_48k == 3);
    drv->read = opl_read;
    drv->write = opl_write;
    drv->update = opl_update;
    drv->reset_buffer = opl_reset;
    return 1;
}
void music_add_handler(void (*get_buffer)(int32_t *, uint16_t, void *), void *priv)
{
    (void) get_buffer; (void) priv;
    music_handler_count++;
}
static uint8_t fake_rom[0x2800];
FILE *rom_fopen(const char *path, char *mode)
{
    FILE *file;
    (void) path; (void) mode;
    fake_rom[0] = 0x55;
    fake_rom[1] = 0xaa;
    fake_rom[2] = 0x14;
    file = tmpfile();
    assert(file);
    assert(fwrite(fake_rom, 1, sizeof(fake_rom), file) == sizeof(fake_rom));
    rewind(file);
    return file;
}
void *ide_xtide_init_board(int board) { xtide_board = board; return fake_rom; }
void ide_xtide_close_board(int board) { assert(board == xtide_board); }
void ide_writew(uint16_t address, uint16_t value, void *priv)
{ (void) address; (void) value; (void) priv; }
void ide_write_devctl(uint16_t address, uint8_t value, void *priv)
{ (void) address; (void) value; (void) priv; }
void ide_writeb(uint16_t address, uint8_t value, void *priv)
{ (void) address; (void) value; (void) priv; }
uint8_t ide_readb(uint16_t address, void *priv)
{ (void) address; (void) priv; return 0xff; }
uint8_t ide_read_alt_status(uint16_t address, void *priv)
{ (void) address; (void) priv; return 0xff; }
uint16_t ide_readw(uint16_t address, void *priv)
{ (void) address; (void) priv; return 0xffff; }

#include "../src/device/toshiba_aform.c"
#include "../src/device/conventional_3inoneder.c"

static void
test_mode(int mode, int expected_count, uint16_t first, uint16_t second)
{
    toshiba_aform_slot_params_t slot_params = {
        .machine = "t5200", .signals = TOSHIBA_AFORM_T5200_SIGNALS
    };
    conventional_3inoneder_params_t card_params = {
        .machine = "t5200", .opl_io = mode
    };
    void *slot, *card;

    io_count = io_remove_count = mapping_disable_count = music_handler_count = 0;
    gameport_add_count = ethernet_add_count = 0;
    common_priv = &slot_params;
    slot = toshiba_aform_slot_device.init(&toshiba_aform_slot_device);
    assert(slot);
    common_priv = &card_params;
    card = conventional_3inoneder_device.init(&conventional_3inoneder_device);
    assert(card);
    assert(io_count == expected_count && music_handler_count == 1);
    assert(io_base[0] == first && io_size[0] == 4);
    if (expected_count == 2)
        assert(io_base[1] == second && io_size[1] == 4);
    conventional_3inoneder_device.close(card);
    assert(io_remove_count == expected_count);
    toshiba_aform_slot_device.close(slot);
}

static void
test_xtide(int variant, uint16_t expected_base)
{
    toshiba_aform_slot_params_t slot_params = {
        .machine = "t5200", .signals = TOSHIBA_AFORM_T5200_SIGNALS
    };
    conventional_3inoneder_params_t card_params = {
        .machine = "t5200", .opl_io = CONVENTIONAL_3INONEDER_OPL_388,
        .xtide = variant
    };
    void *slot, *card;

    io_count = io_remove_count = mapping_disable_count = music_handler_count = 0;
    gameport_add_count = ethernet_add_count = 0;
    common_priv = &slot_params;
    slot = toshiba_aform_slot_device.init(&toshiba_aform_slot_device);
    assert(slot);
    common_priv = &card_params;
    card = conventional_3inoneder_device.init(&conventional_3inoneder_device);
    assert(card && music_handler_count == 1);
    assert(io_count == 2 && io_base[0] == 0x388 && io_base[1] == expected_base);
    assert(xtide_board == 1);
    assert(io_size[1] == 16);
    assert(mapping_base == 0xc8000 && mapping_size == 0x2800);
    conventional_3inoneder_device.close(card);
    assert(io_remove_count == 2 && mapping_disable_count == 1);
    toshiba_aform_slot_device.close(slot);
}

static void
test_version_a_cf_only(void)
{
    toshiba_aform_slot_params_t slot_params = {
        .machine = "t5200", .signals = TOSHIBA_AFORM_T5200_SIGNALS
    };
    conventional_3inoneder_params_t card_params = {
        .machine = "t5200", .opl_io = CONVENTIONAL_3INONEDER_OPL_DISABLED,
        .xtide = CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300
    };
    void *slot, *card;

    io_count = io_remove_count = mapping_disable_count = music_handler_count = 0;
    common_priv = &slot_params;
    slot = toshiba_aform_slot_device.init(&toshiba_aform_slot_device);
    assert(slot);
    common_priv = &card_params;
    card = conventional_3inoneder_device.init(&conventional_3inoneder_device);
    assert(card);
    assert(io_count == 1 && io_base[0] == 0x300);
    assert(music_handler_count == 0);
    conventional_3inoneder_device.close(card);
    assert(io_remove_count == 1 && mapping_disable_count == 1);
    toshiba_aform_slot_device.close(slot);
}

static void
test_version_c_resources(void)
{
    toshiba_aform_slot_params_t slot_params = {
        .machine = "t5200", .signals = TOSHIBA_AFORM_T5200_SIGNALS
    };
    conventional_3inoneder_params_t card_params = {
        .machine = "t5200", .opl_io = CONVENTIONAL_3INONEDER_OPL_388,
        .xtide = CONVENTIONAL_3INONEDER_XTIDE_AT320INT_320,
        .joystick = 1, .ethernet = 1
    };
    void *slot, *card;

    io_count = io_remove_count = mapping_disable_count = music_handler_count = 0;
    gameport_add_count = ethernet_add_count = 0;
    common_priv = &slot_params;
    slot = toshiba_aform_slot_device.init(&toshiba_aform_slot_device);
    assert(slot);
    common_priv = &card_params;
    card = conventional_3inoneder_device.init(&conventional_3inoneder_device);
    assert(card && gameport_add_count == 1 && ethernet_add_count == 1);
    conventional_3inoneder_device.close(card);
    toshiba_aform_slot_device.close(slot);

    card_params.xtide = CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300;
    common_priv = &slot_params;
    slot = toshiba_aform_slot_device.init(&toshiba_aform_slot_device);
    assert(slot);
    common_priv = &card_params;
    assert(!conventional_3inoneder_device.init(&conventional_3inoneder_device));
    toshiba_aform_slot_device.close(slot);
}

int
main(void)
{
    test_mode(CONVENTIONAL_3INONEDER_OPL_388, 1, 0x388, 0);
    test_mode(CONVENTIONAL_3INONEDER_OPL_220, 1, 0x220, 0);
    test_mode(CONVENTIONAL_3INONEDER_OPL_240, 1, 0x240, 0);
    test_mode(CONVENTIONAL_3INONEDER_OPL_388_AND_220, 2, 0x388, 0x220);
    test_xtide(CONVENTIONAL_3INONEDER_XTIDE_AT_INT_300, 0x300);
    test_xtide(CONVENTIONAL_3INONEDER_XTIDE_AT320INT_320, 0x320);
    test_version_a_cf_only();
    test_version_c_resources();
    return 0;
}
