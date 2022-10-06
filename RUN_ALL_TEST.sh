#!/bin/bash
bold=$(tput bold)
normal=$(tput sgr0)
red=$(tput setaf 1)
green=$(tput setaf 2)
white=$(tput sgr0)
pass_text="${bold}${green}Passed${white}${normal}"
fail_text="${bold}${red}Failed${white}${normal}"

return_code=0

echo "> Testing commands..."
for test_date in tests/*/; do
    tmp=$(basename $test_date)
    if [ "$tmp" = "bin" ]; then continue; fi

    for test_pkg in ${test_date}*/; do
        # Print the package under test
        relPath=$(realpath --relative-to=${PWD}/tests/ ${PWD}/${test_pkg})
        printf "> Checking %s...\n" $relPath
        for test_file in ${test_pkg}testcases/*; do
            test_name=$(basename $test_file)
            dofile="${test_pkg}testcases/$test_name"
            ref_file="${test_pkg}reference/$test_name"
            # Test the dofile
            printf "    - Testing %-30s %s" $test_name
            ./qsyn -f ${dofile} 2>/dev/null | diff ${ref_file} - -rs | grep -q "identical"
            # print the result
            # grep returns 0 if found matching strings
            status=$?
            if [ $status -eq 0 ]; then 
                echo ${pass_text};
            else 
                echo ${fail_text}; return_code=1;
            fi
        done
    done
done

printf "\n> Testing functions...\n"
./tests/bin/tests -r compact

exit ${return_code}