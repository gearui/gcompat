/* GearOS AArch64 LP64 loader query bridge. SPDX-License-Identifier: NCSA
 * Reports actual mapped PT_LOAD spans and PT_GNU_EH_FRAME using musl's loader.
 * Not a signal-safe replacement for glibc's lock-free loader implementation.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#if !defined(__aarch64__)
#error This bridge requires independent ABI validation on other architectures
#endif
struct gear_dl_find_object {
    unsigned long long flags;
    void *start, *end;
    struct link_map *map;
    void *eh_frame;
    void *sframe;
    unsigned long long reserved[6];
};
struct search { uintptr_t address; struct gear_dl_find_object *out; int found; int index; };
static int find(struct dl_phdr_info *info, size_t size, void *opaque)
{
    (void)size;
    struct search *s = opaque;
    uintptr_t low = UINTPTR_MAX, high = 0;
    void *eh = NULL;
    int contains = 0;
    for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) *p = &info->dlpi_phdr[i];
        uintptr_t begin = info->dlpi_addr + p->p_vaddr;
        if (p->p_type == PT_LOAD && p->p_memsz) {
            uintptr_t end = begin + p->p_memsz;
            if (begin < low) low = begin;
            if (end > high) high = end;
            if (s->address >= begin && s->address < end) contains = 1;
        }
        if (p->p_type == PT_GNU_EH_FRAME) eh = (void *)begin;
    }
    int main_program = s->index++ == 0;
    if (!contains) return 0;
    /* musl reports the main program first, named by its path (glibc: ""); a NOLOAD
     * dlopen of that path does not find the already-mapped executable, so unwinding
     * through the executable (C++/Kotlin exceptions) found no FDE. Use the
     * program handle for it. */
    void *handle = main_program ? dlopen(NULL, RTLD_NOW)
        : dlopen(info->dlpi_name && *info->dlpi_name ? info->dlpi_name : NULL, RTLD_NOW | RTLD_NOLOAD);
    struct link_map *map = NULL;
    if (!handle || dlinfo(handle, RTLD_DI_LINKMAP, &map)) {
        if (handle) dlclose(handle);
        return 1;
    }
    memset(s->out, 0, sizeof *s->out);
    s->out->start = (void *)low; s->out->end = (void *)high;
    s->out->map = map; s->out->eh_frame = eh;
    dlclose(handle); s->found = 1;
    return 1;
}
int _dl_find_object(void *address, struct gear_dl_find_object *out)
{
    struct search s = {(uintptr_t)address, out, 0, 0};
    dl_iterate_phdr(find, &s);
    return s.found ? 0 : -1;
}
