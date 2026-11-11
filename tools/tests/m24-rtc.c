/* BluMach modifications: rtzor, Project BluMach, 2026. */
/* MM58174 counter and real NVR persistence contracts. No firmware execution.
 * Compile with -O2 -fwhole-program, -Isrc/include and -Isrc/cpu.
 * argv[1] must name a disposable, empty directory.
 */
#include <assert.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

/* Keep the device clock distinct from the generic NVR clock, as in the app. */
#define intclk m24_device_clock
#include "../../src/machine/m_xt_olivetti.c"
#undef intclk

static struct tm host_fixture = {
    .tm_year = 88, .tm_mon = 0, .tm_mday = 2, .tm_wday = 6,
    .tm_hour = 12, .tm_min = 34, .tm_sec = 37
};
#ifdef _WIN32
static int fixture_time(struct tm *out, const time_t *unused)
{ (void)unused; *out = host_fixture; return 0; }
#define localtime_s fixture_time
#define gmtime_s fixture_time
#else
static struct tm *fixture_time(const time_t *unused, struct tm *out)
{ (void)unused; *out = host_fixture; return out; }
#define localtime_r fixture_time
#define gmtime_r fixture_time
#endif
#undef IS_AT
#define IS_AT(x) 0
#include "../../src/nvr.c"

char usr_path[1024];
uint64_t tsc, TIMER_USEC = 1ULL << 32;
int time_sync, machine;
static const char *fixture_name;
static unsigned checks, warnings;
#define CHECK(c) do { checks++; assert(c); } while (0)

void pclog(const char *format, ...)
{
    if (strstr(format, "persistence disabled"))
        warnings++;
}
FILE *plat_fopen(const char *path, const char *mode) { return fopen(path, mode); }
int plat_dir_check(char *path)
{
    struct stat st;
    return !stat(path, &st) && (st.st_mode & S_IFDIR);
}
int plat_dir_create(char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}
void path_slash(char *path)
{
    const size_t length = strlen(path);
    if (length && path[length - 1] != '/' && path[length - 1] != '\\')
        strcat(path, "/");
}
const char *machine_get_nvr_name(void) { return fixture_name; }
void timer_enable(pc_timer_t *timer) { timer->flags |= TIMER_ENABLED; }
void timer_disable(pc_timer_t *timer) { timer->flags &= ~TIMER_ENABLED; }
void timer_add(pc_timer_t *timer, void (*callback)(void *), void *priv, int start)
{
    memset(timer, 0, sizeof(*timer));
    timer->callback = callback;
    timer->priv = priv;
    if (start)
        timer_set_delay_u64(timer, 0);
}

static void open_clock(mm58174_t *rtc, const char *name, int sync)
{
    memset(rtc, 0, sizeof(*rtc));
    fixture_name = name;
    time_sync = sync;
    rtc->nvr.size = MM58174_NVR_REGS;
    rtc->nvr.reset = mm58174_reset;
    rtc->nvr.start = mm58174_start;
    rtc->nvr.tick = mm58174_tick;
    nvr_init(&rtc->nvr);
}
static void close_clock(mm58174_t *rtc)
{
    nvr_close();
    free(rtc->nvr.fn);
    rtc->nvr.fn = NULL;
}
static void next10(nvr_t *nvr)
{
    tsc = nvr->onesec_time.ts_integer;
    timer_disable(&nvr->onesec_time);
    onesec_timer(nvr);
}
static void write_fixture(const char *name, const uint8_t *bytes, size_t length)
{
    char filename[128];
    snprintf(filename, sizeof(filename), "%s.nvr", name);
    FILE *fp = fopen(nvr_path(filename), "rb");
    CHECK(fp == NULL); /* Never overwrite a pre-existing fixture. */
    fp = fopen(nvr_path(filename), "wb");
    CHECK(fp && fwrite(bytes, 1, length, fp) == length);
    CHECK(fclose(fp) == 0);
}
static size_t read_fixture(const char *name, uint8_t *bytes, size_t capacity)
{
    char filename[128];
    snprintf(filename, sizeof(filename), "%s.nvr", name);
    FILE *fp = fopen(nvr_path(filename), "rb");
    CHECK(fp != NULL);
    const size_t count = fread(bytes, 1, capacity, fp);
    CHECK(fgetc(fp) == EOF);
    CHECK(fclose(fp) == 0);
    return count;
}
static void seed_registers(uint8_t *bytes, int running)
{
    memset(bytes, 0, 32);
    mm58174_time_set(bytes, &host_fixture);
    bytes[MM58174_RESET] = running;
}
static void persistence_contracts(void)
{
    mm58174_t first, second;
    uint8_t legacy[32], saved[64], expected[16];

    seed_registers(legacy, 0);
    memcpy(expected, legacy, 16);
    expected[MM58174_RESET] = 1;
    write_fixture("legacy", legacy, 16);
    open_clock(&first, "legacy", 0);
    CHECK(!first.nvr.is_new && first.nvr.size == 32);
    CHECK(!memcmp(expected, first.nvr.regs, 16));
    CHECK(nvr_load() && !memcmp(expected, first.nvr.regs, 16));
    CHECK(nvr_save());
    CHECK(read_fixture("legacy", saved, sizeof(saved)) == 32);
    CHECK(!memcmp(saved, expected, 16));
    CHECK(!memcmp(saved + 16, mm58174_nvr_footer, 16));

    mm58174_write(0x7e, 0, &first.nvr);
    CHECK(nvr_save());
    CHECK(nvr_load() && !first.nvr.is_new && first.nvr.size == 32);
    CHECK(first.nvr.regs[MM58174_RESET] == 0);
    CHECK(first.nvr.regs[MM58174_SECOND1] == 0);
    close_clock(&first);
    open_clock(&second, "legacy", 0); /* Cold load, different instance. */
    CHECK(!second.nvr.is_new && second.nvr.regs[MM58174_RESET] == 0);
    CHECK(second.nvr.regs[MM58174_MINUTE1] == 4 && second.nvr.regs[MM58174_HOUR10] == 1);
    mm58174_write(0x7e, 1, &second.nvr);
    for (int i = 0; i < 100; i++) next10(&second.nvr);
    CHECK(second.nvr.regs[MM58174_SECOND1] == 1);
    CHECK(nvr_save() && nvr_load());
    CHECK(second.nvr.regs[MM58174_RESET] == 1 && second.nvr.regs[MM58174_SECOND1] == 1);
    close_clock(&second);

    for (int sync = 0; sync <= 1; sync++) for (int run = 0; run <= 1; run++) {
        char name[64];
        snprintf(name, sizeof(name), "modern-sync%d-run%d", sync, run);
        seed_registers(legacy, run);
        memcpy(legacy + 16, mm58174_nvr_footer, 16);
        write_fixture(name, legacy, 32);
        open_clock(&first, name, sync);
        CHECK(!first.nvr.is_new && first.nvr.size == 32);
        CHECK(first.nvr.regs[MM58174_RESET] == run);
        CHECK(first.nvr.regs[MM58174_SECOND1] == (run ? 7 : 0));
        CHECK(first.nvr.regs[MM58174_SECOND10] == (run ? 3 : 0));
        CHECK(first.nvr.onesec_cnt == 0 && mm58174_read(0x71, &first.nvr) == 0);
        struct tm global;
        nvr_time_get(&global);
        if (!sync) CHECK(global.tm_sec == (run ? 37 : 0));
        CHECK(nvr_save() && nvr_load());
        CHECK(first.nvr.regs[MM58174_RESET] == run);
        CHECK(first.nvr.regs[MM58174_SECOND1] == (run ? 7 : 0));
        close_clock(&first);
        open_clock(&second, name, sync);
        CHECK(!second.nvr.is_new && second.nvr.regs[MM58174_RESET] == run);
        CHECK(second.nvr.regs[MM58174_SECOND1] == (run ? 7 : 0));
        close_clock(&second);
    }
    for (int sync = 0; sync <= 1; sync++) {
        char name[64];
        snprintf(name, sizeof(name), "fresh-sync%d", sync);
        open_clock(&first, name, sync);
        CHECK(first.nvr.is_new && first.nvr.regs[MM58174_RESET] == 1);
        CHECK(first.nvr.size == 32);
        CHECK(nvr_save() && nvr_load() && !first.nvr.is_new);
        CHECK(first.nvr.regs[MM58174_RESET] == 1);
        close_clock(&first);
    }

    /* Unsupported and incomplete formats cannot silently become legacy RUN or
     * be truncated by the generic saver. Repeated load warns once per instance.
     */
    const size_t lengths[] = { 0, 7, 15, 17, 31, 32 };
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        char name[64];
        snprintf(name, sizeof(name), "unknown-%zu", lengths[i]);
        seed_registers(legacy, 0);
        memcpy(legacy + 16, mm58174_nvr_footer, 16);
        legacy[24] = 2; /* A future format version, not a legacy 16-byte save. */
        write_fixture(name, legacy, lengths[i]);
        const unsigned before = warnings;
        open_clock(&first, name, 0);
        CHECK(first.nvr.size == 0 && first.nvr.regs[MM58174_RESET] == 0);
        CHECK(warnings == before + 1);
        CHECK(nvr_save() && nvr_load());
        CHECK(first.nvr.size == 0 && warnings == before + 1);
        CHECK(nvr_save());
        CHECK(read_fixture(name, saved, sizeof(saved)) == lengths[i]);
        CHECK(!memcmp(saved, legacy, lengths[i]));
        close_clock(&first);
    }
}
static void divider_contracts(void)
{
    for (int sync = 0; sync <= 1; sync++) for (int phase = 0; phase < 100; phase++) {
        mm58174_t rtc = { 0 };
        nvr_t *nvr = &rtc.nvr;
        mm58174_time_set(nvr->regs, &host_fixture);
        mm58174_time_get(nvr->regs, &m24_device_clock);
        nvr_time_set(&host_fixture);
        nvr->tick = mm58174_tick;
        nvr->regs[MM58174_RESET] = 1;
        nvr->onesec_cnt = phase;
        tsc = 123456;
        time_sync = sync;
        timer_set_delay_u64(&nvr->onesec_time, 7000 * TIMER_USEC);
        mm58174_write(0x7e, 0, nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 0 && nvr->regs[MM58174_SECOND10] == 0);
        CHECK(mm58174_read(0x71, nvr) == 0);
        for (int i = 0; i < 150; i++) next10(nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 0 && mm58174_read(0x71, nvr) == 0);
        mm58174_write(0x7e, 1, nvr);
        CHECK(nvr->onesec_cnt == 0);
        CHECK(nvr->onesec_time.ts_integer == tsc + 10000);
        for (int i = 0; i < 9; i++) next10(nvr);
        CHECK(mm58174_read(0x71, nvr) == 0 && nvr->regs[MM58174_SECOND1] == 0);
        next10(nvr);
        CHECK(mm58174_read(0x71, nvr) == 1);
        for (int i = 0; i < 25; i++) next10(nvr);
        const uint64_t deadline = nvr->onesec_time.ts_integer;
        mm58174_write(0x7e, 1, nvr);
        CHECK(nvr->onesec_cnt == 35 && nvr->onesec_time.ts_integer == deadline);
        CHECK(mm58174_read(0x71, nvr) == 3);
        for (int i = 0; i < 64; i++) next10(nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 0 && mm58174_read(0x71, nvr) == 9);
        next10(nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 1 && mm58174_read(0x71, nvr) == 0);
        mm58174_write(0x71, 9, nvr);
        mm58174_write(0x72, 8, nvr);
        mm58174_write(0x73, 7, nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 1 && nvr->regs[MM58174_SECOND10] == 0);
        mm58174_write(0x7e, 0, nvr);
        CHECK(nvr->regs[MM58174_SECOND1] == 0);
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < 900);
    strcpy(usr_path, argv[1]);
    path_slash(usr_path);
    persistence_contracts();
    divider_contracts();
    printf("MM58174: PASS %u checks; real NVR load/save, legacy migration, modern STOP/RUN, fresh/unknown files, sync, divider phases\n", checks);
    return 0;
}
