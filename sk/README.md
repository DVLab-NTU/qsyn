# Sk 
this is a SK algorithm code implementation
    create_gate_list.cpp
        - type in maximum gate length of the gate list 
        - create two files to save the gate list and random unitaries for testing
    sk.cpp
        - rely on Eigen and qpp
        - type in max length of gate in gate list, number of iteration in sk algorithm

g++-11 -O3 -Wall -Wextra -pedantic -isystem /home/ferayer/eigen/ -I /home/ferayer/skcode sk_eigen.cpp -o sk
