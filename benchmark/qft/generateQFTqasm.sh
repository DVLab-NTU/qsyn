for file in 2 3 4 5 6 7 8 9 10 16 27 65 127 433; do
    echo "Generating size = $file"
    python3 generateQFTqasm.py --qnum ${file}                                    
    echo "Generating size = $file decomposition"
    python3 generateQFTqasm.py --qnum ${file} --decom
    echo "Generating size = $file for PyZX"
    python3 generateQFTqasm.py --qnum ${file} --forpyzx                                          
done