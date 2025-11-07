#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

void wrapper(int64_t idx, void *env)
{
    if (!env)
    {
        fprintf(stderr, "wrapper: env is NULL\n");
        return;
    }

    int32_t *a = (int32_t *)env;

    if (idx < 0)
    {
        fprintf(stderr, "wrapper: suspicious index %" PRId64 "\n", idx);
        return;
    }

    a[(int)idx] = (int32_t)idx;

    fprintf(stderr, "wrapper: wrote a[%" PRId64 "] = %d\n", idx, (int)a[idx]);
}