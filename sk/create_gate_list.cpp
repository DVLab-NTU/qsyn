#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include "qpp.h"


using namespace std;
using namespace qpp;


// Example identity matrix (replace with your actual identity matrix)
cmat Id2 = cmat::Identity(2, 2);

cmat to_su2(cmat u)
{

  cplx det = u.determinant();
  cplx one(1,0);
  return (sqrt(one / det) * u);
}

vector<string> binary_prod(int n)
{
  
  vector<string> bin_list;
  for (int i = 1; i <= n; i++)
  {
    
    for (int j = 0;j < pow(2,i); j++)
    {
      boost::dynamic_bitset<> B(i, j);
      string B_string;
      to_string(B,B_string);
      bin_list.push_back(B_string);
    }
    
  }
  
  return bin_list;

}

// vector<cmat> create_unitaries(const vector<cmat>  base, int limit)
// {
//     vector<cmat> gate_list;
//     vector<string> bin_list = binary_prod(limit);
//     for (int i = 0; i < bin_list.size(); i++)
//     {
//         cmat u = Id2; // Initialize with the identity matrix
//         string bits = bin_list[i];

//         for (int j = 0; j < bits.length(); j++)
//         {
//             char bit = bits[j];
//             int index = int(bit) - 48; // Convert char to int (assuming ASCII)
//             u *= base[index];
//         }

//         gate_list.push_back(u);
//     }

//     return gate_list;
// }

vector<cmat> create_unitaries(const vector<cmat>  base, int limit)
{
    vector<cmat> gate_list;
    vector<int> cur_string;
    gate_list.push_back(Id2);
    for (int i = 0; i <= limit; i++){
      cur_string.push_back(0);
    }
    for( int j = 0; j < limit; j++){
        while(cur_string[j] == 0){
            cmat u = Id2;
            for (int i = 0; i < j; i++){
              cout << cur_string[i];
              u *= base[cur_string[i]];
            }
            cout << endl;
            gate_list.push_back(u);
            cur_string[0] += 1;
            int temp = 0;
            while(cur_string[temp] == base.size()){
              cur_string[temp] = 0;
              temp += 1;
              cur_string[temp] += 1;
            }
        }
        for (int i = 0; i <= limit; i++){
          cur_string[i] = 0;
        }
    }
    return gate_list;
}

void save_unitaries(const vector<cmat>& unitaries, const string& filename)
{
    ofstream file(filename, ios::binary);

    if (!file.is_open())
    {
        cerr << "Error opening file: " << filename << endl;
        return;
    }

    int num_matrices = unitaries.size();
    file.write(reinterpret_cast<const char*>(&num_matrices), sizeof(int));

    for (const auto& u : unitaries)
    {
        file.write(reinterpret_cast<const char*>(u.data()), sizeof(complex<double>) * u.size());
    }

    file.close();
}

void saveRandUnitaries(const string& filename, int numMatrices) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return;
    }

    // Write the number of matrices to the file
    file.write(reinterpret_cast<const char*>(&numMatrices), sizeof(int));

    // Generate and save random unitary matrices
    for (int i = 0; i < numMatrices; ++i) {
        cmat u(2,2);
        u = randU();
        u = to_su2(u); 
        file.write(reinterpret_cast<const char*>(u.data()), sizeof(complex<double>) * u.size());
    }

    file.close();
}
int main()
{
    // Example usage
    // Define your base matrices
    vector<cmat> base = { to_su2(gt.H),to_su2(gt.S), to_su2(gt.T),to_su2((gt.T.conjugate()).transpose()) };

    int limit;
    cout << "type in number of iterations: " << '\n' ;
    cin >> limit;    
    // for(int i = 1; i < 16; i++){
    //     vector<cmat> unitaries = create_unitaries(base, i);
    //     save_unitaries(unitaries, "gate_list_" + std::to_string(i) + ".dat");
    // }
    vector<cmat> unitaries = create_unitaries(base, limit);
    save_unitaries(unitaries, "gate_list_" + std::to_string(limit) + ".dat");
    saveRandUnitaries("RandomUnitary.dat", 1000);
  
    // Save the resulting unitary matrices to a file

    return 0;
}
