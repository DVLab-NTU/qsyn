#include "bits/stdc++.h"
using namespace std;

void print_matrix(const vector<vector<complex<double>>>& matrix) {
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < matrix[i].size(); ++j) {
            cout << matrix[i][j] << " ";
        }
        cout << endl;
    }
}

bool isUnitaryMatrix(const vector<vector<complex<double>>>& matrix) {
    // 檢查矩陣乘以其共軛轉置是否為單位矩陣
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < matrix.size(); ++j) {
            complex<double> sum = 0.0;
            for (size_t k = 0; k < matrix.size(); ++k) {
                    
                sum += matrix[i][k] * conj(matrix[j][k]);
            }

            //for debug
            //cout<<i<<" "<<j<<" : "<<sum<<endl;
            //cout.flush();

            if (i == j && abs(sum - 1.0) > 1e-3) {
                return false;
            } else if (i != j && abs(sum) > 1e-3) {
                return false;
            }
        }
    }
    return true;
}

void conjugateMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    for (size_t i = 0; i < matrix.size(); i++) {
        for (size_t j = 0; j < matrix[0].size(); j++) {
            matrix[i][j] = conj(matrix[i][j]); // 對每个元素共軛
        }
    }
}

vector<vector<complex<double>>> transposeMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    vector<vector<complex<double>>> result(matrix[0].size(), vector<complex<double>>(matrix.size(), 0.0));

    for (size_t i = 0; i < matrix[0].size(); i++) {
        for (size_t j = 0; j < matrix.size(); j++) {
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

double get_angle(complex<double> s) {
    return (s.real() != 0) ? atan(s.imag()/s.real()) : M_PI/2;
}

complex<double> get_det(vector<vector<complex<double>>>& U) {
    return U[0][0]*U[1][1] - U[0][1]*U[1][0];
}

vector<double> to_bloch(vector<vector<complex<double>>>& U) {
    assert(U.size() == 2);
    assert(U[0].size() == 2);
    assert(U[1].size() == 2);
    
    double theta, lambda, mu, global_phase;
    // print_matrix(U);

    theta = acos(abs(U[0][0]));
    global_phase = get_angle(get_det(U)) / 2;
    lambda = get_angle(U[0][0]) - global_phase;
    mu = get_angle(U[0][1]) - global_phase;
    // cout << theta << " " << lambda << " " << mu << " " << global_phase << endl;
    // if (global_phase > 1e-6) cerr << "not su" << endl;

    vector<double> bloch{theta, lambda, mu};
    if (abs(pow(abs(U[0][0]),2) + pow(abs(U[0][1]),2) - 1) > 1e-3) {
        cerr << "||U|| != 1" << endl;
        bloch.clear(); // ||U|| != 1
    }
    //double check_angle = get_angle(U[1][0]) + mu - global_phase - M_PI;
    // cout << check_angle << endl;
    // if (abs(sin(check_angle)) > 1e-6 || cos(check_angle) < 1 - 1e-6) {
    //     cerr << "|U| is not e^it" << endl;
    //     bloch.clear();   // |U| is not e^it
    // }

    return bloch;
}

vector<string> cu_decompose(vector<vector<complex<double>>>& U, int targit_b, int ctrl_b) {
    vector<string> ckt(7);
    vector<double> U_bloch = to_bloch(U);
    if (U_bloch.empty()) {
        // cerr << "not SU" << endl;
        ckt.clear();
        ckt.push_back("cu q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n");
        return ckt;
    }
    double theta = U_bloch[0];
    double lambda = U_bloch[1];
    double mu = U_bloch[2];

    ckt[0] = "rz(" + to_string(-mu) +") q[" + to_string(targit_b) + "];"+ "\n";
    ckt[1] = "cx q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n";
    ckt[2] = "rz(" + to_string(-lambda) +") q[" + to_string(targit_b) + "];\n";
    ckt[3] = "ry(" + to_string(-theta) +") q[" + to_string(targit_b) + "];\n";
    ckt[4] = "cx q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n";
    ckt[5] = "ry(" + to_string(theta) +") q[" + to_string(targit_b) + "];\n";
    ckt[6] = "rz(" + to_string(lambda + mu) +") q[" + to_string(targit_b) + "];" + "\n";

    return ckt;
}

vector<string> cnu_decompose(vector<vector<complex<double>>> U, int target_bits, int qubit) {
    vector<vector<complex<double>>> U_dag;
    vector<string> result;
    vector<string> cu_buff;
    string temp, temp2;
    int n = qubit - 1;
    for(int i = 0; i < qubit; i++){
        assert(n > 0);
        if(i != target_bits){
            // print_matrix(U);
            //if n = 1;
            if(n == 1){
                cu_buff = cu_decompose(U, target_bits, i);
                result.insert(result.end(), cu_buff.begin(), cu_buff.end());
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
            // print_matrix(U);
            cu_buff = cu_decompose(U, target_bits, i);
            result.insert(result.end(), cu_buff.begin(), cu_buff.end());

            //second Cn-1X
            // temp = "c" + to_string(n-1) + "x";
            temp = "mcx ";
            for(int j = i+1; j < qubit; j++){
                if(j != target_bits){
                    temp = temp + "q[" +to_string(j) + "], ";
                }
            }
            temp = temp + "q[" +to_string(target_bits) + "];\n";
            result.push_back(temp);

            //third CV_dag
            U_dag = transposeMatrix(U);
            conjugateMatrix(U_dag);
            cu_buff = cu_decompose(U, target_bits, i);  // U_dag perhaps?
            result.insert(result.end(), cu_buff.begin(), cu_buff.end()); 


            // fourth Cn-1X
            result.push_back(temp);
            n--;
        }
    }
    //result.push_back("--end cnu--\n");
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
        if (abs(abs(U[i][i]) - one) > 1e-3) break;
        --i;
    }
    if (i == -1) {
        cerr << "incorrect matrix" << endl;
        return U2;
    }
    //complex<double> uii = U[i][i];
    j = 0;
    while (j < i){
        if (abs(U[j][i]) > 1e-3) break;
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

vector<string> vecstr_Ctrl(int b, int n, vector<vector<complex<double>>>& U2, vector<bool>& i_state) {
    vector<string> half_ckt;
    vector<string> cnU;
    for (int ctrl_b = 0; ctrl_b < n; ++ctrl_b) {
        if (ctrl_b == b) continue;
        if (ctrl_b >= int(i_state.size()) || i_state[ctrl_b] == 0) {
            half_ckt.push_back("rx(pi) " + str_q(ctrl_b) + ";\n");
        }
    }

    if (U2.empty()) {   //cnx -- turn x into stringU
        // string cnx = "c" + to_string(n-1) + "x ";
        string cnx = "mcx ";
        if((n-1) == 1) {
            cnx = "cx ";
        }
        for (int ctrl_b = 0; ctrl_b < n; ++ctrl_b) {
            if (ctrl_b == b) continue;
            cnx += str_q(ctrl_b) + ", ";
        }
        cnx += str_q(b) + ";\n";
        cnU.push_back(cnx);
    } else {
        cnU = cnu_decompose(U2, b, n);
    }

    vector<string> full_ckt = half_ckt;
    full_ckt.insert(full_ckt.end(), cnU.begin(), cnU.end());
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

vector<string> gray_code(int i, int j, int n, vector<vector<complex<double>>>& U2){
    //cout << i << " " << j << endl;
    assert(i != j);
    vector<string> half_ckt;
    vector<bool> i_state, j_state;
    vector<vector<complex<double>>> dummy(0); //dummy indicate x
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
            vector<string> cnx = vecstr_Ctrl(b, n, dummy, i_state);
            half_ckt.insert(half_ckt.end(), cnx.begin(), cnx.end());
            i_state[b] = !i_state[b];
        }
    }
    assert(U_b != -1);
    vector<string> cnU = vecstr_Ctrl(U_b, n, U2, i_state);
    vector<string> full_ckt = half_ckt;
    full_ckt.insert(full_ckt.end(),cnU.begin(), cnU.end());
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

void decompose(string input, string output){
    //輸入matrix大小
    ifstream fin(input);
    ofstream fout(output);
    if(!fin.good()){
        cerr<<input<<" not exist\n";
        return 1;
    }
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
        if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-3){//|Mii| != 1
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
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-3){//|Mii| != 1
                for(int j = 0; j < n; j++){
                    if(i != j){
                        if(fabs(sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i])) - 1.0) < 1e-3){//Mii^2 + Mij^2 = 1
                            improve = 1;
                            //create two level matrix
                            two_level_matrix[i][i] = conj(input_matrix[i][i]);
                            two_level_matrix[i][j] = conj(input_matrix[j][i]);
                            two_level_matrix[j][i] = -input_matrix[j][i];
                            two_level_matrix[j][j] = input_matrix[i][i];

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);
                            
                            //for debug
                            // cout<<"2-level\n";
                            // for(int n = 0; n < two_level_matrix.size(); n++){
                            //     for(int k = 0; k < two_level_matrix[0].size(); k++){
                            //         cout<<two_level_matrix[n][k]<<" ";
                            //     }
                            //     cout<<endl;
                            // }
                            // cout<<endl;
                            

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
                if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-3){//|Mii| != 1
                    for(int j = 0; j < n; j++){
                        if((i != j) && fabs(abs(input_matrix[i][j]) - 1.0) > 1e-3){
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
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-3){
                finish = 0;
            }
        }
        
        // for debug
        // cout<<"U : "<<endl;
        // for(int j = 0; j < input_matrix.size(); j++){
        //     for(int k = 0; k < input_matrix[j].size(); k++){
        //         cout<<input_matrix[j][k]<<" ";
        //     }
        //     cout<<endl;
        // }
        // cout<<endl;
    }
    
    two_level_matrices[two_level_matrices.size()-1] = matrixMultiply(two_level_matrices[two_level_matrices.size()-1], input_matrix);

    /*//for debug
    for(int i = 0; i < two_level_matrices.size(); i++){
        cout<<"matrix "<<(i+1)<<":"<<endl;
        for(int j = 0; j < two_level_matrices[i].size(); j++){
            for(int k = 0; k < two_level_matrices[i][j].size(); k++){
                cout<<two_level_matrices[i][j][k]<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
    }*/

    fout<<"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q["<<int(log2(n))<<"];\n\n";

    
    //gray-code
    for(size_t t = 0; t < two_level_matrices.size(); t++) {
        vector<vector<complex<double>>> U2;
        int i, j;
        U2 = to_2level(two_level_matrices[t], i, j);
        // vector<string> str_U2 = cu_decompose(U2, i, j);
        // print_matrix(U2);
        
        vector<string> str_U2 = gray_code(i,j,(int(log2(n))),U2);
        for (size_t s = 0; s < str_U2.size(); s++) {
            fout << str_U2[s];
        }
    }
    
    /*//cnu testcase
    vector<vector<complex<double>>> U(2, vector<complex<double>>(2, 0.0));
    U[0][1] = 1;
    U[1][0] = 1;

    int qubit = int(log2(n));
    //cnu decompose
    cout<<"\n\n";
    vector<string> cnu_gateset = cnu_decompose(U, 1, qubit);
    for(int i = 0; i < cnu_gateset.size(); i++){
        cout<<cnu_gateset[i];
    }*/
}

int main(int argc, char* argv[]){
    decompose(argv[1], argv[2]);
}