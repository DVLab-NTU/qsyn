#include "bits/stdc++.h"
using namespace std;

bool isUnitaryMatrix(const vector<vector<complex<double>>>& matrix) {
    // 檢查矩陣乘以其共軛轉置是否為單位矩陣
    for (int i = 0; i < matrix.size(); ++i) {
        for (int j = 0; j < matrix.size(); ++j) {
            complex<double> sum = 0.0;
            for (int k = 0; k < matrix.size(); ++k) {
                    
                sum += matrix[i][k] * conj(matrix[j][k]);
            }

            //for debug
            //cout<<i<<" "<<j<<" : "<<sum<<endl;
            //cout.flush();

            if (i == j && abs(sum - 1.0) > 1e-6) {
                return false;
            } else if (i != j && abs(sum) > 1e-6) {
                return false;
            }
        }
    }
    return true;
}

void conjugateMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    for (int i = 0; i < matrix.size(); i++) {
        for (int j = 0; j < matrix[0].size(); j++) {
            matrix[i][j] = conj(matrix[i][j]); // 對每个元素共軛
        }
    }
}

vector<vector<complex<double>>> transposeMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    vector<vector<complex<double>>> result(matrix[0].size(), vector<complex<double>>(matrix.size(), 0.0));

    for (int i = 0; i < matrix[0].size(); i++) {
        for (int j = 0; j < matrix.size(); j++) {
            result[i][j] = matrix[j][i]; // 對每個元素轉置
        }
    }
    return result;
}

vector<vector<complex<double>>> matrixMultiply(const vector<vector<complex<double>>>& a, const vector<vector<complex<double>>>& b) {
    int m = a.size();    // a 的行
    int n = a[0].size(); // a 的列
    int p = b[0].size(); // b 的列

    // init to 0
    vector<vector<complex<double>>> result(m, std::vector<complex<double>>(p, 0.0));

    // multiply
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < p; ++j) {
            for (int k = 0; k < n; ++k) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    return result;
}

string intToBinary(int num, int n) {
    string binary = "";
    for (int i = n; i >= 0; --i) {
        binary += (num & (1 << i)) ? '1' : '0';
    }
    return binary;
}

vector<string> cnu_decompose(vector<vector<complex<double>>> U, int target_bits, int qubit) {
    vector<vector<complex<double>>> U_dag;
    vector<string> result;
    string temp, temp2;
    int n = qubit - 1;
    for(int i = 0; i < qubit; i++){
        assert(n > 0);
        if(i != target_bits){
            //if n = 1;
            if(n == 1){
                //result.push_back(cu_decompose(i, target_bits, U));
                n--;
                break;
            }

            //first CV
            complex<double> s = sqrt(U[0][0]*U[1][1] - U[0][1]*U[1][0]); 
            complex<double> t = sqrt(U[0][0] + U[1][1] + (2.0 * s));
            U[0][0] = (U[0][0] + s)/t;
            U[0][1] = (U[0][1])/t;
            U[1][0] = (U[1][0])/t;
            U[1][1] = (U[1][1] + s)/t;
            //result.push_back(cu_decompose(i, target_bits, U));

            //second Cn-1X
            temp = "c" + to_string(n-1) + "x";
            for(int j = i+1; j < qubit; j++){
                if(qubit-1 != target_bits && (j != target_bits && j != qubit-1)){
                    temp = temp + "q[" +to_string(j) + "], ";
                }
                else if(qubit-1 != target_bits && (j != target_bits && j == qubit-1)){
                    temp = temp + "q[" +to_string(j) + "];\n";
                }
                else if(qubit-1 == target_bits && (j != target_bits && j != qubit-2)){
                    temp = temp + "q[" +to_string(j) + "], ";
                }
                else if(qubit-1 == target_bits && (j != target_bits && j == qubit-2)){
                    temp = temp + "q[" +to_string(j) + "];\n";
                }
            }
            result.push_back(temp);

            //third CV_dag
            U_dag = transposeMatrix(U);
            conjugateMatrix(U_dag);
            //result.push_back(cu_decompose(i, target_bits, U));


            // fourth Cn-1X
            result.push_back(temp);
            n--;
        }
    }
    return result;
}

    
vector<vector<complex<double>>> to_2level(vector<vector<complex<double>>>& U, int& i, int& j) {
    complex<double> one;
    one.real(1);
    one.imag(0);
    vector<vector<complex<double>>> U2(2, vector<complex<double>>(2, 0.0));
    i = U.size() - 1;
    while (i > -1){
        // cout << U[i][i] << endl;
        if (abs(U[i][i] - one) > 1e-6) break;
        --i;
    }
    if (i == -1) {
        cerr << "incorrect matrix" << endl;
        return U2;
    }
    complex<double> uii = U[i][i];
    j = 0;
    while (j < i){
        if (abs(U[j][i]) > 1e-6) break;
        ++j;
    }
    if (j == i) {
        cerr << "incorrect matrix" << endl;
        return U2;
    }

    U2[0][0] = U[j][j];
    U2[0][1] = U[j][i];
    U2[1][0] = U[i][j];
    U2[1][1] = U[i][i];
    return U2;
}

string str_q(int b) {
    return "q[" + to_string(b) + "]";
}

vector<string> vecstr_Ctrl(int b, int n, string U, vector<bool>& i_state) {
    vector<string> half_ckt;
    string cnU = "c" + to_string(n-1) + U + " ";
    if((n-1) == 1){
        cnU = "c" + U + " ";
    }
    for (size_t ctrl_b = 0; ctrl_b < n; ++ctrl_b) {
        if (ctrl_b == b) continue;

        if (ctrl_b >= i_state.size() || i_state[ctrl_b] == 0) {
            half_ckt.push_back("rx(pi) " + str_q(ctrl_b) + ";\n");
        }
        cnU += str_q(ctrl_b) + ", ";
    }
    cnU += str_q(b) + ";\n";
    vector<string> full_ckt = half_ckt;
    full_ckt.push_back(cnU);
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

vector<string> gray_code(int i, int j, int n, string U2_name, vector<vector<complex<double>>>& U2){
    //cout << i << " " << j << endl;
    assert(i != j);
    vector<string> half_ckt;
    vector<bool> i_state, j_state;
    while (i != 0) {
        i_state.push_back(false);
        if (i % 2) i_state[i_state.size() - 1] = true;
        i = i / 2;
    }
    while (j != 0) {
        j_state.push_back(false);
        if (j % 2) j_state[j_state.size() - 1] = true;
        j = j / 2;
    }
    while (i_state.size() != j_state.size()) {
        if (i_state.size() < j_state.size()) i_state.push_back(false);
        else j_state.push_back(false);
    }

    int U_b = -1;
    for (size_t b = 0; b < i_state.size(); ++b) {
        if (i_state[b] != j_state[b]) { // q[b] is flip
            if (U_b < 0) {
                U_b = b;
                continue;
            }
            vector<string> cnx = vecstr_Ctrl(b, n, "x", i_state);
            half_ckt.insert(half_ckt.end(), cnx.begin(), cnx.end());
            i_state[b] = !i_state[b];
        }
    }
    assert(U_b != -1);
    vector<string> cnU = vecstr_Ctrl(U_b, n, U2_name, i_state);
    vector<string> full_ckt = half_ckt;
    full_ckt.insert(full_ckt.end(),cnU.begin(), cnU.end());
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

int main(int argc, char *argv[]){
    //輸入matrix大小
    ifstream fin(argv[1]);
    int n;
    fin>>n;

    //輸入matrix
    vector<vector<complex<double>>> input_matrix(n, vector<complex<double>>(n, 0.0));
    string str;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            fin>>str;
            str = str.substr(1, str.size()-2);
            input_matrix[i][j].real(stod(str.substr(0, str.find(","))));
            input_matrix[i][j].imag(stod(str.substr(str.find(",")+1)));
            //cout<<"("<<input_matrix[n][n].real<<","<<input_matrix[n][n].imag<<") ";
        }
        //cout<<endl;
    }
    
    //check unitary
    assert(isUnitaryMatrix(input_matrix));
    
    //2-level decomposition
    vector<vector<vector<complex<double>>>> two_level_matrices;
    bool finish = 1, improve = 0;

    //check need to be decompose
    for(int i = 0; i < n; i++){
        if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
            finish = 0;
        }
    }

    while(!finish){
        //init
        improve = 0;
        vector<vector<complex<double>>> two_level_matrix(n, vector<complex<double>>(n, 0.0));
        for(int i = 0; i < n; i++){
            two_level_matrix[i][i] = 1;
        }

        //find Mii^2 + Mij^2 = 1 first
        for(int i = 0; i < n; i++){
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
                for(int j = 0; j < n; j++){
                    if(i != j){
                        if(fabs(sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i])) - 1.0) < 1e-6){//Mii^2 + Mij^2 = 1
                            improve = 1;
                            //create two level matrix
                            two_level_matrix[i][i] = conj(input_matrix[i][i]);
                            two_level_matrix[i][j] = conj(input_matrix[j][i]);
                            two_level_matrix[j][i] = -input_matrix[j][i];
                            two_level_matrix[j][j] = input_matrix[i][i];

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);
                            
                            /*for debug
                            cout<<"2-level\n";
                            for(int n = 0; n < two_level_matrix.size(); n++){
                                for(int k = 0; k < two_level_matrix[0].size(); k++){
                                    cout<<two_level_matrix[n][k]<<" ";
                                }
                                cout<<endl;
                            }
                            cout<<endl;*/
                            

                            //dag and push back
                            conjugateMatrix(two_level_matrix);
                            two_level_matrices.push_back(transposeMatrix(two_level_matrix));
                            break;
                        }
                    } 
                }
            }
            if(improve){
                break;
            }
        }


        if(!improve){
            for(int i = 0; i < n-1; i++){
                if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
                    for(int j = 0; j < n; j++){
                        if((i != j) && fabs(abs(input_matrix[i][j]) - 1.0) > 1e-6){
                            improve = 1;
                            //create two level matrix
                            two_level_matrix[i][i] = conj(input_matrix[i][i])/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[i][j] = conj(input_matrix[j][i])/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[j][i] = -input_matrix[j][i]/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[j][j] = input_matrix[i][i]/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);

                            //dag and push back
                            conjugateMatrix(two_level_matrix);
                            two_level_matrices.push_back(transposeMatrix(two_level_matrix));
                            break;
                        }
                    }
                }
                if(improve){
                    break;
                }
            }
        }

        finish = 1;
        for(int i = 0; i < n; i++){
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){
                finish = 0;
            }
        }
        
        /*for debug
        cout<<"U : "<<endl;
        for(int j = 0; j < input_matrix.size(); j++){
            for(int k = 0; k < input_matrix[j].size(); k++){
                cout<<input_matrix[j][k]<<" ";
            }
            cout<<endl;
        }
        cout<<endl;*/
    }
    
    two_level_matrices[two_level_matrices.size()-1] = matrixMultiply(two_level_matrices[two_level_matrices.size()-1], input_matrix);

    /*//for debug
    for(int i = 0; i < two_level_matrices.size(); i++){
        cout<<"matrix "<<(i+1)<<":"<<endl;
        for(int j = 0; j < two_level_matrices[i].matrix.size(); j++){
            for(int k = 0; k < two_level_matrices[i].matrix[j].size(); k++){
                cout<<two_level_matrices[i].matrix[j][k]<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
    }*/

    cout<<"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q["<<int(log2(n))<<"];\n\n";

    
    //gray-code
    for (int t = 0; t < two_level_matrices.size(); t++) {
        vector<vector<complex<double>>> U2;
        int i, j;
        U2 = to_2level(two_level_matrices[t], i, j);
        vector<string> str_U2 = gray_code(i,j,(int(log2(n))),"U"+to_string(t),U2);
        for (int s = 0; s < str_U2.size(); s++) {
            cout << str_U2[s];
        }
    }
    
    //cnu testcase
    vector<vector<complex<double>>> U(2, vector<complex<double>>(2, 0.0));
    U[0][1] = 1;
    U[1][0] = 1;

    int qubit = int(log2(n));
    //cnu decompose
    cout<<"\n\n";
    vector<string> cnu_gateset = cnu_decompose(U, 1, qubit);
    for(int i = 0; i < cnu_gateset.size(); i++){
        cout<<cnu_gateset[i];
    }
}