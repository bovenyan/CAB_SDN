#!/bin/bash
if [ "$1" = "-l" ]
then
    echo "H:Maximum split level  Using:8k_${2}" >> ./CPLX_test_results
    for i in {1..10}
    do
        echo "Running: 8k_${2} level:$i"
        ./build/CPLX_test -r ./ruleset/8k_${2} $i
    done
elif [ "$1" = "-s" ]
then
    echo "H:Ruleset size  Using:*k_${2}" >> ./CPLX_test_results
    for i in {1..20}
    do
        echo "Running: ${i}k_${2}"
        ./build/CPLX_test -r ./ruleset/${i}k_${2} 5
    done
else
    echo "usage: -l|-s smoothness"
fi

