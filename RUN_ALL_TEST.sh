
bold=$(tput bold)
normal=$(tput sgr0)
red='\033[0;31m'
green='\033[0;32m'
white='\033[0m'

echo "> Testing commands..."
cd tests
thisDir=$PWD
# touch tmp.log;
for test in */; do
    tmp=$(basename $test)
    if [ "$tmp" = "bin" ]; then continue; fi

    cd "$test"
    for subtest in */; do
        for subsubtest in */; do
            cd "$subsubtest"
            relPath=$(realpath --relative-to=$thisDir $PWD)
            printf "> Checking %s...\n" $relPath
                for testfile in testcases/*; do
                    testname=$(basename $testfile)
                    printf "    - Testing %-20s %s" $testname
                    ../../../qsyn -f "testcases/$testname" 2>/dev/null | \
                    diff "reference/$testname" - -rs | \
                    grep -q "identical"
                    status=$?
                    [ $status -eq 0 ] && echo "${green}${bold}Passed${normal}${white}" || echo "${red}${bold}Failed${normal}${white}"
                done
            cd ..
        done
    done
    cd ..
done
# echo $PWD
echo "\n> Testing functions..."
cd bin
./tests -r compact
cd ..
# rm -r tmp.log
cd ..