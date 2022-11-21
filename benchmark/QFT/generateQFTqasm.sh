for file in 5 7 16 27 65 127 433 1121 2129 3457 5105 11969; do
    echo "Generating size = $file"
    python3 generateQFTqasm.py --qnum ${file}                                    
    echo "Generating size = $file decomposition"
    python3 generateQFTqasm.py --qnum ${file} --decom                                    
done