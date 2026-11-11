/* BluMach modifications: rtzor, Project BluMach, 2026. */
/* BluMach, a preservation-focused fork of 86Box.
 * Author: rtzor
 * Project: BluMach
 * Register I/O regression: production OGC, CGA and timing implementations.
 * Palette output and the external light-pen button are test doubles. This
 * harness does not validate rendering, device initialization or physical timing.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <86box/timer.h>
#include <86box/mem.h>
#include <86box/video.h>
#include <86box/vid_cga.h>
#include <86box/vid_ogc.h>

uint64_t CGACONST = UINT64_C(1000);
double cpuclock = 8000000.0;
monitor_t monitors[MONITORS_NUM];
int monitor_index_global;

void
update_cga16_color(uint8_t mode, uint8_t color)
{
    (void) mode;
    (void) color;
}

int
tablet_get_buttons_ex(void)
{
    return 0;
}

static ogc_t
profile(bool go380, uint16_t control)
{
    ogc_t ogc = { 0 };
    ogc.go380_profile = go380;
    ogc.ctrl_addr = control;
    return ogc;
}

static void
crtc_write(ogc_t *ogc, uint16_t index, uint16_t data, uint8_t reg, uint8_t value)
{
    ogc_out(index, reg, ogc);
    ogc_out(data, value, ogc);
}

static void
aliases(void)
{
    /* Four index aliases crossed with four data aliases. These are actual
       calls to the OGC handler and its production CGA delegate. */
    for (uint16_t index = 0x3d0; index <= 0x3d6; index += 2) {
        for (uint16_t data = 0x3d1; data <= 0x3d7; data += 2) {
            ogc_t ogc = profile(true, 0x3de);
            for (unsigned value = 0; value <= 0xff; value++) {
                crtc_write(&ogc, index, data, 0x2f, value);
                assert(ogc.cga.crtcreg == 0x0f);
                assert(ogc.cga.crtc[0x0f] == value);
                for (uint16_t read_index = 0x3d0; read_index <= 0x3d6; read_index += 2) {
                    assert(ogc_in(read_index, &ogc) == 0x0f);
                    assert(ogc_in(read_index + 1, &ogc) == value);
                }
            }
            /* Other CRTC masks must still come from CGA. */
            crtc_write(&ogc, index, data, CGA_CRTC_CURSOR_ADDR_HIGH, 0xff);
            assert(ogc_in(data, &ogc) == 0x3f);
            crtc_write(&ogc, index, data, CGA_CRTC_CURSOR_ADDR_LOW, 0xff);
            assert(ogc_in(data, &ogc) == 0xff);
        }
    }
}

static void
other_profiles(void)
{
    /* A shared handler is not evidence that M19/GO708/M240 decode GO380's
       extra ports. Preserve their prior canonical-only OGC routing. */
    const uint16_t controls[] = { 0x3db, 0x3de };
    const uint16_t aliases[] = { 0x3d0, 0x3d1, 0x3d2, 0x3d3, 0x3d6, 0x3d7 };
    for (unsigned c = 0; c < 2; c++) {
        ogc_t ogc = profile(false, controls[c]);
        crtc_write(&ogc, 0x3d4, 0x3d5, 0x0f, 0x5a);
        assert(ogc_in(0x3d4, &ogc) == 0x0f);
        assert(ogc_in(0x3d5, &ogc) == 0x5a);
        for (unsigned i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
            ogc_out(aliases[i], 0xaa, &ogc);
            assert(ogc_in(aliases[i], &ogc) == 0xff);
            assert(ogc.cga.crtcreg == 0x0f);
            assert(ogc.cga.crtc[0x0f] == 0x5a);
        }
    }

    /* The standalone CGA delegate retains its own established aliases. */
    cga_t cga = { 0 };
    for (uint16_t index = 0x3d0; index <= 0x3d6; index += 2) {
        cga_out(index, 0x0f, &cga);
        cga_out(index + 1, 0xff, &cga);
        assert(cga_in(index, &cga) == 0x0f);
        assert(cga_in(index + 1, &cga) == 0xff);
    }
}

static void
upper_ports(void)
{
    const uint16_t controls[] = { 0x3db, 0x3de };
    for (unsigned go380 = 0; go380 < 2; go380++) {
        for (unsigned c = 0; c < 2; c++) {
            for (uint16_t port = 0x3d8; port <= 0x3df; port++) {
                ogc_t ogc = profile(go380, controls[c]);
                ogc.cga.crtcreg = 0x0f;
                ogc.cga.crtc[0x0f] = 0xa5;
                ogc_out(port, 0x3f, &ogc);
                assert(ogc.cga.crtcreg == 0x0f);
                assert(ogc.cga.crtc[0x0f] == 0xa5);
                assert(ogc.cga.cgamode ==
                       (port == 0x3d8 ? (go380 ? 0x3f & ~CGA_MODE_FLAG_BW : 0x3f) : 0));
                assert(ogc.cga.cgacol == (port == 0x3d9 ? 0x3f : 0));
                assert(ogc.ctrl_3de == (port == controls[c] ? 0x3f : 0));
                assert(ogc.base == (port == controls[c] ? 0x4000 : 0));
                assert(ogc_in(port, &ogc) == (port == 0x3da ? 0xe4 : 0xff));
            }
            ogc_t ogc = profile(go380, controls[c]);
            ogc.cga.cgastat = 0x09;
            assert(ogc_in(0x3da, &ogc) == 0xed);
            ogc.mono_display = 1;
            assert(ogc_in(0x3da, &ogc) == 0xfd);
            ogc.cga.lp_strobe = 1;
            (void) ogc_in(0x3dc, &ogc);
            ogc_out(0x3dc, 0, &ogc);
            assert(ogc.cga.lp_strobe == 1);
            ogc.cga.lp_strobe = 0;
            (void) ogc_in(0x3dd, &ogc);
            ogc_out(0x3dd, 0, &ogc);
            assert(ogc.cga.lp_strobe == 0);
        }
    }
}

static void
standard_40h(ogc_t *ogc, uint16_t index, uint16_t data)
{
    static const uint8_t crtc[] = { 0x38, 0x28, 0x2d, 0x06, 0x7f, 0x06, 0x64, 0x70, 0x02, 0x01 };
    for (unsigned reg = 0; reg < sizeof(crtc); reg++)
        crtc_write(ogc, index, data, reg, crtc[reg]);
    ogc_out(0x3d8, CGA_MODE_FLAG_GRAPHICS | CGA_MODE_FLAG_HIGHRES_GRAPHICS, ogc);
    ogc_out(ogc->ctrl_addr, 1, ogc);
}

static void
timings(void)
{
    for (uint16_t index = 0x3d0; index <= 0x3d6; index += 2) {
        for (uint16_t data = 0x3d1; data <= 0x3d7; data += 2) {
            ogc_t ogc = profile(true, 0x3de);
            const double character = 16.0 * (cpuclock / 24000000.0) * (double) (UINT64_C(1) << 32);
            standard_40h(&ogc, index, data);
            assert(ogc.cga.dispontime == (uint64_t) (40 * character));
            assert(ogc.cga.dispofftime == (uint64_t) (18 * character));
            const uint64_t on = ogc.cga.dispontime, off = ogc.cga.dispofftime;
            crtc_write(&ogc, index, data, 0x0f, 0xff);
            assert(ogc.cga.dispontime == on && ogc.cga.dispofftime == off);
            /* Leaving the special mode through an alias must restore the
               production CGA timing path, including its normal masks. */
            crtc_write(&ogc, index, data, CGA_CRTC_VTOTAL, 0x7e);
            cga_t expected = ogc.cga;
            cga_recalctimings(&expected);
            assert(ogc.cga.dispontime == expected.dispontime);
            assert(ogc.cga.dispofftime == expected.dispofftime);
        }
    }

    ogc_t m19 = profile(false, 0x3db);
    const double character = 16.0 * (cpuclock / 14318180.0) * (double) (UINT64_C(1) << 32);
    standard_40h(&m19, 0x3d4, 0x3d5);
    assert(m19.cga.dispontime == (uint64_t) (40 * character));
    assert(m19.cga.dispofftime == (uint64_t) (8 * character));
    ogc_out(0x3de, 0, &m19);
    assert(m19.ctrl_3de == 1);
    assert(m19.cga.dispofftime == (uint64_t) (8 * character));
    ogc_out(0x3db, 0, &m19);
    cga_t expected = m19.cga;
    cga_recalctimings(&expected);
    assert(m19.cga.dispontime == expected.dispontime);
    assert(m19.cga.dispofftime == expected.dispofftime);

    ogc_t go708 = profile(false, 0x3de);
    standard_40h(&go708, 0x3d4, 0x3d5);
    ogc_recalctimings(&go708);
    assert(go708.cga.dispontime == 40 * CGACONST);
    assert(go708.cga.dispofftime == 17 * CGACONST);
}

int
main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "aliases"))
        aliases();
    else if (!strcmp(argv[1], "other-profiles"))
        other_profiles();
    else if (!strcmp(argv[1], "upper-ports"))
        upper_ports();
    else if (!strcmp(argv[1], "timings"))
        timings();
    else
        return 2;
    puts("OGC I/O case passed");
    return 0;
}
