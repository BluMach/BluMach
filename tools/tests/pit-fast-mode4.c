/* BluMach modifications: rtzor, Project BluMach, 2026. */
/* Exercise the complete FAST PIT and real timer scheduler without firmware.
 * Only I/O registration and the unrelated RIVA timer are test doubles.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/pit_fast.c"
#include "../../src/timer.c"

cpu_state_t cpu_state;
uint64_t tsc, PITCONST;
double isa_timing;
int isa_cycles;
pit_intf_t pit_devs[2];

void rivatimer_init(void) { }
void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}

/* Native devices register their real callbacks; no port dispatcher is needed. */
#define IO_ARGUMENTS uint16_t base, uint16_t size, \
    uint8_t (*read8)(uint16_t, void *), uint16_t (*read16)(uint16_t, void *), \
    uint32_t (*read32)(uint16_t, void *), void (*write8)(uint16_t, uint8_t, void *), \
    void (*write16)(uint16_t, uint16_t, void *), void (*write32)(uint16_t, uint32_t, void *), void *priv
void io_sethandler(IO_ARGUMENTS)
{
    (void)base; (void)size; (void)read8; (void)read16; (void)read32;
    (void)write8; (void)write16; (void)write32; (void)priv;
}
void io_handler(uint8_t set, IO_ARGUMENTS)
{
    (void)set;
    io_sethandler(base, size, read8, read16, read32, write8, write16, write32, priv);
}
#undef IO_ARGUMENTS

static pitf_t *pit;
static pc_timer_t sentinel;
static unsigned checks;
#define CHECK(expression) do { \
    ++checks; \
    if (!(expression)) { \
        fprintf(stderr, "line %u, TSC %llu: %s\n", __LINE__, \
                (unsigned long long)tsc, #expression); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void sentinel_callback(void *priv)
{
    (void)priv;
    timer_advance_u64(&sentinel, 1000000000ULL << 32);
}

static void reset_device(uint64_t period, const device_t *device)
{
    /* The application closes the scheduler before freeing its devices. */
    timer_close();
    if (pit != NULL)
        pitf_close(pit);
    timer_init();
    timer_add(&sentinel, sentinel_callback, NULL, 0);
    timer_set_delay_u64(&sentinel, 1000000000ULL << 32);
    PITCONST = period;
    pit = pitf_init(device);
    CHECK(pit != NULL);
    pit->counters[2].count = 0x55; /* Deterministic pre-load CE for comparison. */
}

static void reset_test(uint64_t period)
{
    reset_device(period, &i8253_fast_device);
}

static void advance_to(uint64_t target)
{
    while (tsc < target) {
        ++tsc;
        timer_process();
    }
}

static void write_port(unsigned port, unsigned value) { pitf_write(port, value, pit); }
static unsigned read_counter(void) { return pitf_read(0x42, pit); }
static uint128_t now_fixed(void) { return (uint128_t)tsc << 32; }
static uint128_t event_fixed(void)
{
    const pc_timer_t *timer = &pit->counters[2].timer;
    return ((uint128_t)timer->ts_integer << 32) | timer->ts_frac;
}
static uint64_t next_delta(uint64_t period)
{
    return period - (uint64_t)(now_fixed() % period);
}
static void set_period(uint64_t period)
{
    PITCONST = period;
    pitf_speed_changed(pit);
}

static void test_fractional_periods(void)
{
    const uint64_t periods[] = {
        (1ULL << 32) + (1ULL << 31), (2ULL << 32) + (1ULL << 30),
        (7ULL << 32) + 3006477107ULL, (25ULL << 32) + (1ULL << 30),
        (uint64_t)(24000000.0 / 1193182.0 * 4294967296.0),
        (uint64_t)(30000000.0 / 1193182.0 * 4294967296.0)
    };
    const unsigned offsets[] = {0, 1, 2, 3, 11, 37};
    const unsigned values[] = {0, 1, 2, 3, 127, 255};
    for (unsigned pi = 0; pi < 6; ++pi) {
        for (unsigned oi = 0; oi < 6; ++oi) {
            for (unsigned vi = 0; vi < 6; ++vi) {
                const uint64_t period = periods[pi];
                const unsigned value = values[vi], count = value ? value : 65536;
                reset_test(period);
                advance_to(offsets[oi]);
                const uint128_t load = now_fixed() + next_delta(period);
                write_port(0x43, 0x98); /* Counter2, LSB, mode4, binary. */
                write_port(0x42, value);
                pitf_ctr_set_gate(pit, 2, 1);
                CHECK(event_fixed() == load);
                const uint64_t when = (uint64_t)(load >> 32);
                if (when > tsc)
                    advance_to(when - 1);
                CHECK(pit->counters[2].newcount == PITF_M4_LOAD_NEXT);
                /* Real timer callbacks have integer TSC resolution. */
                advance_to(when > tsc ? when : tsc + 1);
                CHECK(pit->counters[2].newcount == 0);
                CHECK(pitf_read_timer(&pit->counters[2]) == (int)count);
                CHECK(pit->counters[2].out == 1);
                if (value != 0) {
                    const uint128_t terminal = load + (uint128_t)count * period;
                    CHECK(event_fixed() == terminal);
                    advance_to((uint64_t)(terminal >> 32));
                    CHECK(pit->counters[2].out == 0);
                    CHECK(read_counter() == 0);
                    CHECK(event_fixed() == terminal + period);
                    advance_to((uint64_t)((terminal + period) >> 32));
                    CHECK(pit->counters[2].out == 1);
                }
            }
        }
    }
}

static void test_period_changes(void)
{
    const uint64_t old_periods[] = {
        10ULL << 32, (25ULL << 32) + (1ULL << 30),
        (uint64_t)(30000000.0 / 1193182.0 * 4294967296.0)
    };
    const uint64_t new_periods[] = {
        20ULL << 32, (20ULL << 32) + (1ULL << 29),
        (uint64_t)(24000000.0 / 1193182.0 * 4294967296.0)
    };
    for (unsigned i = 0; i < 3; ++i) {
        const uint64_t old_period = old_periods[i], new_period = new_periods[i];
        reset_test(old_period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 10);
        pitf_ctr_set_gate(pit, 2, 1);
        advance_to((uint64_t)(event_fixed() >> 32) + 3);
        const int before = pitf_read_timer(&pit->counters[2]);
        uint128_t remaining = timer_get_remaining_u64(&pit->counters[2].timer);
        uint128_t expected = now_fixed() + remaining * new_period / old_period;
        set_period(new_period);
        CHECK(event_fixed() == expected);
        CHECK(pitf_read_timer(&pit->counters[2]) == before);

        reset_test(old_period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 7); advance_to(5);
        remaining = timer_get_remaining_u64(&pit->counters[2].timer);
        expected = now_fixed() + remaining * new_period / old_period;
        set_period(new_period);
        CHECK(event_fixed() == expected);
        advance_to((uint64_t)(expected >> 32));
        CHECK(pitf_read_timer(&pit->counters[2]) == 7);

        /* A speed change scales the remaining part of a low strobe. */
        reset_test(old_period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 1);
        pitf_ctr_set_gate(pit, 2, 1);
        advance_to((uint64_t)(event_fixed() >> 32));
        advance_to((uint64_t)(event_fixed() >> 32));
        CHECK(pit->counters[2].out == 0);
        advance_to(tsc + 3);
        remaining = timer_get_remaining_u64(&pit->counters[2].timer);
        expected = now_fixed() + remaining * new_period / old_period;
        set_period(new_period);
        CHECK(event_fixed() == expected);
        advance_to((uint64_t)(expected >> 32));
        CHECK(pit->counters[2].out == 1);

        /* The oscillator continues while GATE0 has stopped the CE timer. */
        reset_test(old_period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 5);
        advance_to((uint64_t)(event_fixed() >> 32)); advance_to(tsc + 3);
        CHECK(!timer_is_enabled(&pit->counters[2].timer));
        expected = now_fixed() + (uint128_t)next_delta(old_period) * new_period / old_period;
        set_period(new_period); write_port(0x42, 8);
        CHECK(event_fixed() == expected);
    }
}

static void test_rebases(void)
{
    const uint64_t periods[] = {10ULL << 32, (25ULL << 32) + (1ULL << 30)};
    for (unsigned i = 0; i < 2; ++i) {
        const uint64_t period = periods[i];
        reset_test(period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 1); advance_to(5);
        uint128_t expected = event_fixed() + ((uint128_t)101 << 32);
        timer_set_new_tsc(tsc + 101);
        CHECK(event_fixed() == expected);
        write_port(0x42, 2);
        CHECK(event_fixed() == expected);

        /* Rebase before the first mode4 operation still preserves CLK phase. */
        reset_test(period); advance_to(5);
        uint64_t delta = next_delta(period);
        timer_set_new_tsc(tsc + 101);
        expected = now_fixed() + delta;
        write_port(0x43, 0x98); write_port(0x42, 3);
        CHECK(event_fixed() == expected);

        reset_test(period);
        advance_to(3); write_port(0x43, 0x98); write_port(0x42, 5);
        advance_to((uint64_t)(event_fixed() >> 32)); advance_to(tsc + 3);
        delta = next_delta(period); timer_set_new_tsc(tsc + 101);
        expected = now_fixed() + delta; write_port(0x42, 4);
        CHECK(event_fixed() == expected);
    }
}

static void test_manual_clock(void)
{
    reset_test(10ULL << 32);
    pitf_ctr_set_using_timer(pit, 2, 0);
    write_port(0x43, 0x98);
    for (unsigned i = 0; i < 256; ++i) {
        const int previous = pit->counters[2].count;
        write_port(0x42, i);
        CHECK(pitf_read_timer(&pit->counters[2]) == previous);
        pitf_ctr_clock(pit, 2);
        CHECK(read_counter() == i);
        pitf_ctr_clock(pit, 2);
        CHECK(read_counter() == i); /* GATE0 holds CE after transfer. */
    }
    /* An incomplete LSB/MSB pair must not trigger another load. */
    write_port(0x43, 0xb8); write_port(0x42, 0x34); write_port(0x42, 0x12);
    pitf_ctr_set_gate(pit, 2, 1); pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].count == 0x1234);
    write_port(0x42, 0x56); pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].count == 0x1233);
    write_port(0x42, 0x01); pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].count == 0x0156);
}

static void test_gate_and_retrigger(void)
{
    reset_test(10ULL << 32);
    pitf_ctr_set_using_timer(pit, 2, 0);
    write_port(0x43, 0x98); pitf_ctr_set_gate(pit, 2, 1);
    write_port(0x42, 5); pitf_ctr_clock(pit, 2);
    pitf_ctr_clock(pit, 2); pitf_ctr_clock(pit, 2);
    CHECK(read_counter() == 3);
    pitf_ctr_set_gate(pit, 2, 0);
    for (unsigned i = 0; i < 10; ++i) pitf_ctr_clock(pit, 2);
    CHECK(read_counter() == 3);
    pitf_ctr_set_gate(pit, 2, 1); pitf_ctr_clock(pit, 2);
    CHECK(read_counter() == 2);
    write_port(0x42, 4); CHECK(read_counter() == 2);
    pitf_ctr_clock(pit, 2); CHECK(read_counter() == 4);
    for (unsigned i = 0; i < 4; ++i) pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].out == 0);
    pitf_ctr_set_gate(pit, 2, 0); pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].out == 1);

    reset_test(10ULL << 32);
    write_port(0x43, 0x98); write_port(0x42, 5); pitf_ctr_set_gate(pit, 2, 1);
    advance_to(23); CHECK(read_counter() == 4);
    pitf_ctr_set_gate(pit, 2, 0); advance_to(99); CHECK(read_counter() == 4);
    pitf_ctr_set_gate(pit, 2, 1);
    CHECK(event_fixed() == (uint128_t)130 << 32);
    advance_to(100); CHECK(read_counter() == 3);
    advance_to(130); CHECK(pit->counters[2].out == 0);
}

static void test_clock_source_changes(void)
{
    reset_test(10ULL << 32);
    write_port(0x43, 0x98); write_port(0x42, 3);
    pitf_ctr_set_gate(pit, 2, 1); advance_to(13);
    pitf_ctr_set_using_timer(pit, 2, 0);
    CHECK(!timer_is_enabled(&pit->counters[2].timer));
    CHECK(read_counter() == 3);
    pitf_ctr_clock(pit, 2); CHECK(read_counter() == 2);
    advance_to(100); CHECK(read_counter() == 2);
    CHECK(pit->counters[2].out == 1);
    pitf_ctr_set_using_timer(pit, 2, 1);
    CHECK(event_fixed() == (uint128_t)120 << 32);
    advance_to(110); CHECK(read_counter() == 1);
    advance_to(120); CHECK(pit->counters[2].out == 0);
    /* Recovering OUT also follows the selected clock source. */
    pitf_ctr_set_using_timer(pit, 2, 0);
    CHECK(!timer_is_enabled(&pit->counters[2].timer));
    pitf_ctr_set_gate(pit, 2, 0); pitf_ctr_clock(pit, 2);
    CHECK(pit->counters[2].out == 1);
    advance_to(200); CHECK(read_counter() == 255);

    reset_test(10ULL << 32); advance_to(3);
    write_port(0x43, 0x98); write_port(0x42, 7);
    pitf_ctr_set_using_timer(pit, 2, 0); pitf_ctr_clock(pit, 2);
    CHECK(read_counter() == 7);
    advance_to(20); CHECK(read_counter() == 7);
    write_port(0x42, 9); pitf_ctr_set_using_timer(pit, 2, 1);
    CHECK(event_fixed() == (uint128_t)30 << 32);
    advance_to(30); CHECK(read_counter() == 9);
    CHECK(!timer_is_enabled(&pit->counters[2].timer));
}

static void test_custom_clock_devices(void)
{
    device_t custom = i8253_fast_device;
    custom.local |= PIT_CUSTOM_CLOCK;
    const device_t *devices[] = {&custom, &i8254_ps2_fast_device};
    for (unsigned i = 0; i < 2; ++i) {
        reset_device(10ULL << 32, devices[i]);
        CHECK(pit->dev_priv == NULL); /* No internal phase is running initially. */
        pitf_ctr_set_using_timer(pit, 2, 0);
        write_port(0x43, 0x98); write_port(0x42, 7);
        pitf_ctr_set_gate(pit, 2, 1); pitf_ctr_clock(pit, 2);
        CHECK(read_counter() == 7);
        pitf_ctr_clock(pit, 2); CHECK(read_counter() == 6);
        advance_to(100); CHECK(read_counter() == 6);
        pitf_ctr_set_using_timer(pit, 2, 1);
        CHECK(event_fixed() == (uint128_t)160 << 32);
        advance_to(110); CHECK(read_counter() == 5);
        advance_to(160); CHECK(pit->counters[2].out == 0);
        advance_to(170); CHECK(pit->counters[2].out == 1);
    }
}

static void test_phase_stress(void)
{
    const uint64_t periods[] = {
        10ULL << 32, (20ULL << 32) + (1ULL << 29), (25ULL << 32) + (1ULL << 30),
        (uint64_t)(24000000.0 / 1193182.0 * 4294967296.0),
        (uint64_t)(30000000.0 / 1193182.0 * 4294967296.0)
    };
    uint64_t period = periods[0];
    uint128_t next = period;
    unsigned rng = 0x4d3234;
    reset_test(period); write_port(0x43, 0x98);
    for (unsigned i = 0; i < 100; ++i) {
        rng = rng * 1664525u + 1013904223u;
        advance_to(tsc + rng % 200u);
        if (next <= now_fixed())
            next += ((now_fixed() - next) / period + 1u) * period;
        if (i % 3 == 0) {
            const uint64_t new_period = periods[(rng >> 16) % 5];
            uint128_t delta = (next - now_fixed()) * new_period / period;
            if (delta == 0) delta = 1;
            next = now_fixed() + delta;
            set_period(new_period); period = new_period;
        }
        if (i % 5 == 0) { timer_set_new_tsc(tsc + 101); next += (uint128_t)101 << 32; }
        if (i % 7 == 0 && tsc > 100) { timer_set_new_tsc(tsc - 100); next -= (uint128_t)100 << 32; }
        write_port(0x42, 15); CHECK(event_fixed() == next);
    }
    tsc += 3000000; timer_process();
    if (next <= now_fixed())
        next += ((now_fixed() - next) / period + 1u) * period;
    write_port(0x42, 17); CHECK(event_fixed() == next);
}

static uint64_t fingerprint = 1469598103934665603ULL;
static unsigned observations;
static void observe(unsigned value)
{
    fingerprint ^= value;
    fingerprint *= 1099511628211ULL;
    ++observations;
}

static void test_other_modes(void)
{
    /* Preserved pre-fix sequences, not a claim that all legacy modes are exact. */
    const unsigned modes[] = {0, 1, 2, 3, 5}, counts[] = {1, 2, 3, 127, 255};
    const uint64_t periods[] = {
        10ULL << 32, (25ULL << 32) + (1ULL << 30),
        (uint64_t)(30000000.0 / 1193182.0 * 4294967296.0)
    };
    for (unsigned pi = 0; pi < 3; ++pi) {
        for (unsigned m = 0; m < 5; ++m) {
            for (unsigned n = 0; n < 5; ++n) {
                for (unsigned rw = 1; rw <= 3; ++rw) {
                    reset_test(periods[pi]);
                    write_port(0x43, 0x80 | (rw << 4) | (modes[m] << 1));
                    if (rw != 2) write_port(0x42, counts[n] & 255);
                    if (rw != 1) write_port(0x42, counts[n] >> 8);
                    for (unsigned k = 1; k <= 256; ++k) {
                        advance_to(k);
                        if (k == 1 || k == 31 || k == 101) pitf_ctr_set_gate(pit, 2, 1);
                        if (k == 15 || k == 47 || k == 149) pitf_ctr_set_gate(pit, 2, 0);
                        if (k == 63) { write_port(0x42, 7); if (rw == 3) write_port(0x42, 0); }
                        if (k == 100) set_period(periods[pi] * 2);
                        const ctrf_t *counter = &pit->counters[2];
                        observe(read_counter()); observe(counter->out); observe(counter->count);
                        observe(counter->wm); observe(counter->rm); observe(counter->newcount);
                        observe(counter->thit); observe(counter->gate); observe(counter->enabled);
                    }
                }
            }
        }
    }
    CHECK(observations == 518400);
    CHECK(fingerprint == 0xf174c2b85cc48301ULL);
}

static unsigned timer_count(void)
{
    unsigned count = 0;
    for (pc_timer_t *timer = timer_head; timer != NULL; timer = timer->next) {
        CHECK(count < 30);
        ++count;
    }
    return count;
}

static void test_initial_clock_configuration(void)
{
    reset_test(0);
    CHECK(pit->dev_priv == NULL); /* Do not schedule an anchor with no period. */
    advance_to(5);
    set_period(10ULL << 32);
    CHECK(pit->dev_priv != NULL);
    timer_set_new_tsc(106);
    write_port(0x43, 0x98); write_port(0x42, 1);
    CHECK(event_fixed() == (uint128_t)111 << 32);
}

static void test_close_ownership(void)
{
    reset_test(10ULL << 32);
    const unsigned before = timer_count();
    pitf_t *other = pitf_init(&i8253_fast_device);
    CHECK(timer_count() == before + 3);
    pitf_write(0x43, 0x98, other); pitf_write(0x42, 7, other);
    CHECK(timer_count() == before + 4); /* Includes an active pending CE load. */
    pitf_close(other);
    CHECK(timer_count() == before);
    advance_to(100); /* No freed counter callback remains registered. */
}

int main(int argc, char **argv)
{
    const char *group = argc == 2 ? argv[1] : "all";
    if (strcmp(group, "counts") == 0 || strcmp(group, "all") == 0) {
        test_fractional_periods(); test_manual_clock(); test_gate_and_retrigger();
    }
    if (strcmp(group, "timebase") == 0 || strcmp(group, "all") == 0) {
        test_period_changes(); test_rebases(); test_phase_stress();
    }
    if (strcmp(group, "external") == 0 || strcmp(group, "all") == 0) {
        test_clock_source_changes(); test_custom_clock_devices();
    }
    if (strcmp(group, "other-modes") == 0 || strcmp(group, "all") == 0)
        test_other_modes();
    if (strcmp(group, "ownership") == 0 || strcmp(group, "all") == 0) {
        test_initial_clock_configuration(); test_close_ownership();
    }
    CHECK(checks > 1);
    timer_close();
    if (pit != NULL) pitf_close(pit);
    printf("FAST PIT: %u checks; %u other-mode observations PASS\n", checks, observations);
    return 0;
}
