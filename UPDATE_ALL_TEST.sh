
bold=$(tput bold)
normal=$(tput sgr0)
red='\033[0;31m'
green='\033[0;32m'
white='\033[0m'
pass_text="${green}${bold}Passed${normal}${white}"
fail_text="${red}${bold}Failed${normal}${white}"

echo "> Updating all testcases ref..."
for test_date in tests/*/; do
    tmp=$(basename $test_date)
    if [ "$tmp" = "bin" ]; then continue; fi

    for test_pkg in ${test_date}*/; do
        # Print the package under test
        relPath=$(realpath --relative-to=${PWD}/tests/ ${PWD}/${test_pkg})
        printf "> Updating %s...\n" $relPath
        for test_file in ${test_pkg}testcases/*; do
            test_name=$(basename $test_file)
            dofile="${test_pkg}testcases/$test_name"
            ref_file="${test_pkg}reference/$test_name"
            # Test the dofile
            printf "    - Updating %-20s %s" $test_name
            ./qsyn < ${dofile} > ${ref_file}
            # print the result
            # grep returns 0 if found matching strings
            # status=$?
            # [ $status -eq 0 ] && echo ${pass_text} || echo ${fail_text}
        done
    done
done
