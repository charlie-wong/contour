#! /bin/bash

clear
for i in `seq 1 30`; do
    printf "%02d: " $i
    for k in `seq 1 30`; do
        printf " %02d" $k
    done
    printf "\n"
done

SIXEL_IMAGE="$HOME/projects/contour/docs/sixel/snake.sixel"

echo -ne "\033[2;6H"
cat "${SIXEL_IMAGE}"
echo -ne "****"
echo -ne "\033[31;1H"
