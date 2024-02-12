    OPENQASM 2.0;
    include "qelib1.inc";
    
    qreg q[3];
    
    rz(pi/2) q[1];
    ry(pi/4) q[2];
    rz(pi/2) q[1];
    cx q[2], q[1];
    rz(pi/7) q[0];
    ry(pi/9) q[0];
    rz(pi/7) q[0];
    cx q[1], q[0];