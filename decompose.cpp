#include "bits/stdc++.h"
using namespace std;

struct two_level{
    vector<vector<complex<double>>> matrix;
    int i,j;
};

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

vector<string> cnu_decompose(vector<vector<complex<double>>>& U, int target_bits, int qubit) {
    vector<string> result;
    string temp, temp2;
    int n = qubit - 1;
    for(int i = 1; i <= qubit; i++){
        if(i != target_bits){
            //first
            temp = "cu^(1/2) q["+to_string(i)+"], q["+to_string(target_bits)+"];";
            result.push_back(temp);

            //second
            temp = "";
            for(int j = 0; j < n-1; j++){
                temp = temp + "c";
            }
            temp = temp + "x ";
            for(int j = i+1; j <= qubit; j++){
                if(j != target_bits){
                    temp = temp + "q[" +to_string(j) + "], ";
                }
            }
            temp2 = temp;
            result.push_back(temp);

            //third
            temp = "cu^(1/2)_dag q["+to_string(i)+"], q["+to_string(target_bits)+"];";
            result.push_back(temp);

            for(int j = 1; j <= qubit; j++){
                if(j != qubit && j!= target_bits){
                    temp = "cx q["+to_string(i)+"], q["+to_string(target_bits)+"];";
                    result.push_back("ccx ");
                    
                }
            }

        }
    }
}

vector<string> cnu_decompose_recursive(vector<vector<complex<double>>>& U, int target_bits, int qubit) {
    
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
    vector<two_level> two_level_matrices;
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
        two_level temp;
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

                            temp.matrix = transposeMatrix(two_level_matrix);
                            temp.i = i>j? j:i;
                            temp.j = i>j? i:j;
                            two_level_matrices.push_back(temp);
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
                            two_level_matrix[i][i] = conj(input_matrix[i][i]);
                            two_level_matrix[i][j] = conj(input_matrix[j][i]);
                            two_level_matrix[j][i] = -input_matrix[j][i];
                            two_level_matrix[j][j] = input_matrix[i][i];

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);

                            //dag and push back
                            conjugateMatrix(two_level_matrix);

                            temp.matrix = transposeMatrix(two_level_matrix);
                            temp.i = i>j? j:i;
                            temp.j = i>j? i:j;
                            two_level_matrices.push_back(temp);
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
    
    two_level_matrices[two_level_matrices.size()-1].matrix = matrixMultiply(two_level_matrices[two_level_matrices.size()-1].matrix, input_matrix);

    //for debug
    for(int i = 0; i < two_level_matrices.size(); i++){
        cout<<"matrix "<<(i+1)<<":"<<endl;
        for(int j = 0; j < two_level_matrices[i].matrix.size(); j++){
            for(int k = 0; k < two_level_matrices[i].matrix[j].size(); k++){
                cout<<two_level_matrices[i].matrix[j][k]<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
    }

    
    //gray-code
    int qubit = int(log2(n));
    for(int i = 0; i < two_level_matrices.size(); i++){
        string binary_i, binary_j;
        binary_i = intToBinary(two_level_matrices[i].i, qubit);
        binary_j = intToBinary(two_level_matrices[i].j, qubit);
        cout<<"i "<<binary_i<<" j "<<binary_j<<" ";
    }
    
    //cnu testcase
    vector<vector<complex<double>>> U(2, vector<complex<double>>(2, 0.0));
    U[0][0] = 1;
    U[1][1] = 1;

    //cnu decompose
    vector<string> cnu_gateset = cnu_decompose(U, n, qubit);
}