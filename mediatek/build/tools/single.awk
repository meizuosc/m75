#! /usr/bin/awk -f

{
    for (i = 1; i <= NF; i++)
    {
        ++word[$i]
        if (word[$i] == 1)
            printf("%s ", $i)
    }
}
# chmod u+x t.awk
# ./t.awk file1

