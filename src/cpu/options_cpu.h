/*
  Hatari - options_cpu.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef OPTIONS_CPU_H
#define OPTIONS_CPU_H

#define MAX_CUSTOM_MEMORY_ADDRS 2

struct uae_prefs {
    int cpu_model;
    int cpu_level;
    int mmu_model;
    int fpu_model;
    int fpu_revision;
    bool fpu_strict;
    bool cpu_compatible;
    int cachesize;
    bool compfpu;
    int cpu_idle;
    int cpu060_revision;
};

extern struct uae_prefs currprefs, changed_prefs;

void check_prefs_changed_cpu (void);

#endif /* OPTIONS_CPU_H */
