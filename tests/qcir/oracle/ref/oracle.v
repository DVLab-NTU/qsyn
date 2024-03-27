module oracle (in_0, in_1, in_2, out);
    input in_0, in_1, in_2;
    output out;
    assign out = (in_0 & in_1) ^ (in_0 & in_2);
endmodule 
