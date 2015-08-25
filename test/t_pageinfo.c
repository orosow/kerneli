#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>       /* open() and friends */
#include <fcntl.h>
#include <sys/mman.h>       /* mmap() */

#include "macros.h"

#include "pageinfo.h"
#include "pageinfo_ioctl.h"

#define DEVNAME "/dev/pageinfo"
#define MAX_PATH 256
#define TRACING_PATH "/sys/kernel/debug/tracing"

#define PFLAG_PRESENT 0
#define PFLAG_GLOBAL 1
#define PFLAG_DIRTY 2
#define PFLAG_ACCESSED 3
#define PFLAG_WRITE 4
#define PFLAG_EXEC 5
#define PFLAG_HUGE 6
#define PFLAG_MAX 7

int data;
char *page_size[] = { "N/A ", "1 GB", "2 MB", "4 KB" };

void print_usage(char *name)
{
    printf("Usage: %s [cd]\n", name);
    printf("\t-c: code segment page info\n\t-d: data segment page info\n"); 
}

static char *pflag_val(int flag, int value) {

    static char *page_flags[PFLAG_MAX][2] = {
        {"pres ", "PRES "},
        {"glob ", "GLOB "},
        {"dirt ", "DIRT "},
        {"acc  ", "ACC  "},
        {"writ ", "WRIT "},
        {"exec ", "EXEC "},
        {"huge ", "HUGE "}
    };

    return page_flags[flag][value];
}

void parse_va(struct pageinfo *info)
{
    unsigned long va =  va;
    unsigned long flags = info->flags[info->level];

    printf("virtual address: %lx -- size: %s", info->va, page_size[info->level]);
    if (info->none) {
        printf(":: NO MAPPING\n");
        return;
    }

    printf("   ");

    printf(pflag_val(PFLAG_PRESENT, !!flags));

    /* zero flags serve as an indicator that a page is not present since
     * if page is not present, the remaining flags have no meaning. 
     * (note that we perform an explicit check for the NONE condition
     * (no mapping) above).
     */
    if (!flags) {
        return;
    }

    printf(pflag_val(PFLAG_GLOBAL, !!(flags & _PAGE_GLOBAL)));
    printf(pflag_val(PFLAG_DIRTY, !!(flags & _PAGE_DIRTY)));
    printf(pflag_val(PFLAG_ACCESSED, !!(flags & _PAGE_ACCESSED)));
    printf(pflag_val(PFLAG_WRITE, !!(flags & _PAGE_RW)));
    printf(pflag_val(PFLAG_EXEC, !(flags & _PAGE_NX)));     /* NOTE: _PAGE_NX stands for NON-executable! */
    printf(pflag_val(PFLAG_HUGE, !!(flags & _PAGE_PSE)));

    printf(":: flags: 0x%lx :: ", flags);

    printf("physical page address: %lx\n", info->pa_dir[info->level]); 
}

void print_faults(struct pageinfo *info)
{
    printf("<<<<< FAULTS START >>>>>\n");

    printf("FAULTS: prev_minor: %d\n", info->prev_minflt);
    printf("FAULTS: prev_major: %d\n", info->prev_majflt);
    printf("FAULTS: cur_minor:  %d\n", info->cur_minflt);
    printf("FAULTS: cur_major: %d\n", info->cur_majflt);

    printf("<<<< FAULTS END >>>>>\n");
}

void test_codeseg(int fd)
{
    struct pageinfo info;
 
    /* ------------- CODE -------------- */

    info.va = (unsigned long ) &parse_va;   /* point to code segment */

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- code");

    printf("======== CODE SEGMENT ===========\n");
    parse_va(&info);
}

void test_dataseg(int fd)
{
    struct pageinfo info;

    info.va = (unsigned long ) &data;       /* point to data segment */

    if (ioctl(fd, PINFO_GET_ADDR, &info) == -1)
        serr_exit("ioctl() failed -- data");

    printf("====== DATA SEGMENT =========\n");
    parse_va(&info);
    print_faults(&info);
}

int main(int argc, char *argv[])
{
    int fd;
    int opt;

    if (argc == 1)
        print_usage(argv[0]);

    if ((fd = open(DEVNAME, O_RDWR)) == -1)
        serr_exit("open() failed");

    while ((opt = getopt(argc, argv, "cd")) != -1) {
        switch (opt) {
            case 'd':
                test_dataseg(fd);
                break;
            
            case 'c':
                test_codeseg(fd);
                break;
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
