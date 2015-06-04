#!/bin/bash
for j in 4 8 16 32
do
    echo "H:Maximum split level  Using:8k_${j}" >> ./CPLX_test_results
    for i in {1..10}
    do
        echo "Running: 8k_${j} level:$i"
        ./build/CPLX_test -r ./ruleset/8k_${j} $i
    done
done

for j in 4 8 16 32
do
    echo "H:Ruleset size  Using:*k_${j}" >> ./CPLX_test_results
    for i in {1..20}
    do
        echo "Running: ${i}k_${j}"
        ./build/CPLX_test -r ./ruleset/${i}k_${j} 5
    done
done
