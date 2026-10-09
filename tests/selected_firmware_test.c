/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <86box/device.h>
#include <86box/ini.h>
static char *selection;
static int low_present, high_present;
ini_t config_get_ini(void) { return NULL; }
ini_section_t ini_find_section(ini_t ini, const char *name)
{ (void)ini; (void)name; return NULL; }
char *ini_section_get_string(ini_section_t section, const char *key, char *def)
{ (void)section; (void)key; return selection ? selection : def; }
void device_get_name(const device_t *dev, int bus, char *name)
{ (void)bus; strcpy(name, dev->name); }
int rom_present(const char *file)
{ return !strcmp(file,"low") ? low_present : !strcmp(file,"high") ? high_present : 1; }
static const device_config_t config[] = {
    { .name="bios", .type=CONFIG_BIOS, .default_string="pair", .bios={
        { .name="Paired", .internal_name="pair", .files_no=2, .files={"low","high"} },
        { .name="Other", .internal_name="other", .files_no=1, .files={"other"} }, {0} } },
    { .type=CONFIG_END }
};
int main(void)
{
    const device_t dev={ .name="Test", .config=config };
    assert(device_configured_bios_available(NULL)==0);
    assert(device_configured_bios_available(&dev)==-1); /* Other is present: no fallback. */
    low_present=1;
    assert(device_configured_bios_available(&dev)==-1); /* Half of a pair is insufficient. */
    high_present=1;
    assert(device_configured_bios_available(&dev)==1);
    selection="unknown";
    assert(device_configured_bios(&dev)==NULL);
    assert(device_configured_bios_available(&dev)==-1);
    selection="other";
    assert(device_configured_bios_available(&dev)==1);
    return 0;
}
