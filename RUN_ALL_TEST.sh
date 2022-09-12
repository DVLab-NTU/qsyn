
bold=$(tput bold)
normal=$(tput sgr0)
red='\033[0;31m'
green='\033[0;32m'
white='\033[0m'
pass_text="${green}${bold}Passed${normal}${white}"
fail_text="${red}${bold}Failed${normal}${white}"

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
            printf "    - Testing %-20s %s" $test_name
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

echo "\n> Testing functions..."
./tests/bin/tests -r compact

exit ${return_code}