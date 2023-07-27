#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    #ifdef PBS
    if (argc != 3) 
    {
        fprintf(2, "usage: setpriority new_priority pid\n");
        exit(1);
    }

    int res = setpriority(atoi(argv[1]), atoi(argv[2]));

    if (res == -1)
    {
        fprintf(2, "setpriority: failed\n");
        exit(1);
    }

    #else
    fprintf(2, "Not using Priority Based Scheduling\n");

    #endif

    exit(0);
}