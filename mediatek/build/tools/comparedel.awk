#! /usr/bin/awk

BEGIN {ARGV[x]
    getline type<tf
    split(type,array,",")
}

{
    num = 0
    for (j = 4; j <= cl; j++)
    {
        for (k = 4; k <= NF; k++)
        {
            if ($k == array[j])
            {
                num = 1
                break
            }
        }
        if (num == 1)
            break
    }

    if (num == 1)
    {
        for (i = 1; i <= 3; i++)
        {
            printf("%s,", $i)
        }
        for (j = 4; j <= cl; j++)
        {
            res = 0
            for (k = 4; k <= NF; k++)
            {
                if ($k == array[j])
                {
                    res = 1
                    printf("%s,", "x")
                    break
                }
            }
            if (res == 0)
                printf(" ,")
        }
        printf("\n")
    }
}
