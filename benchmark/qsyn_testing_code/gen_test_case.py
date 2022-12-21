import numpy as np
import random
import os

random.seed(2022)
in_path = "./test_case/in"
out_path = "./test_case/out"
num = 1000

def get_not_empty_id_list(output):
	ids = []
	for e in output:
		g = e[1]
		if(g == "h" or g == "x" or g == "z" or g == "t" or g == "tdg" or g == "s" or g == "rz"):
			ids.append(e[2])
		elif(g == "cx"):
			ids.append(e[2])
			ids.append(e[3])
	return [*set(ids)]



def generate_test_case(num, case_idx, in_path, out_path):
	cmd = []
	output = []
	cmd_num = num
	max_qubit_num = -1
	qubit_id_list = []
	exec_time_for_each_qubit = np.zeros(0)
	gate_num = 0
	# output form [[gate num, which gate, qubit id], .....]

	gates = ["h", "x", "z", "t", "tdg", "s", "cx", "rz"]
	operation = ["add_gate", "del_gate", "del_qubit", "add_qubit"]

	for i in range(cmd_num):
		op = random.choice(operation)

		if(op == "add_gate"):
			if(len(qubit_id_list) == 0):
				continue

			g = random.choice(gates)

			if(g == "h" or g == "x" or g == "z" or g == "t" or g == "tdg" or g == "s"):
				qubit_id = random.choice(qubit_id_list)
				cmd.append(f"qcga -{g} {qubit_id}")
				output.append([gate_num, g, qubit_id])

			elif(g == "cx"):
				if(len(qubit_id_list) < 2):
					continue
				idx = random.sample(qubit_id_list, 2)
				cmd.append(f"qcga -{g} {idx[0]} {idx[1]}")
				output.append([gate_num, g, idx[0], idx[1]])

			elif(g == "rz"):
				qubit_id = random.choice(qubit_id_list)
				phase = random.uniform(0, np.pi)
				cmd.append(f"qcga -{g} -ph {phase} {qubit_id}")
				output.append([gate_num, g, qubit_id, phase])

			gate_num += 1

		elif(op == "del_gate"):

			if(len(output) == 0):
				continue

			del_element = random.choice(output)
			cmd.append(f"qcgd {del_element[0]}")
			output.remove(del_element)

		elif(op == "add_qubit"):
			exec_time_for_each_qubit = np.zeros(len(exec_time_for_each_qubit)+1)
			cmd.append("qcba")
			max_qubit_num += 1
			qubit_id_list.append(max_qubit_num)

		elif(op == "del_qubit"):
			ids = get_not_empty_id_list(output)
			empty_ids = [qubit_id for qubit_id in qubit_id_list if qubit_id not in ids]

			if(len(empty_ids) == 0):
				continue

			del_id = random.choice(empty_ids)
			cmd.append(f"qcbd {del_id}")
			qubit_id_list.remove(del_id)


	with open(os.path.join(in_path, f"{case_idx}.in"), "w") as f:
		for e in cmd:
			f.write(e)
			f.write("\n")
		f.write("qccp\n")
		f.write("qq -f\n")

	with open(os.path.join(out_path, f"{case_idx}.out"), "w") as f:
		f.write("Listed by gate ID\n")
		for e in output:
			g = e[1]
			if(g == "h" or g == "x" or g == "z" or g == "t" or g == "s" or g == "rz"):
				f.write(f"Gate {e[0]}: {e[1]} Exec Time: {int(exec_time_for_each_qubit[e[2]])} Qubit: {e[2]}")
				exec_time_for_each_qubit[e[2]] += 1

			elif(g == "tdg"):
				f.write(f"Gate {e[0]}: {e[1][0:2]} Exec Time: {int(exec_time_for_each_qubit[e[2]])} Qubit: {e[2]}")
				exec_time_for_each_qubit[e[2]] += 1

			elif(g == "cx"):
				exec_time = exec_time_for_each_qubit[e[2]] if exec_time_for_each_qubit[e[2]] > exec_time_for_each_qubit[e[3]] else exec_time_for_each_qubit[e[3]]
				f.write(f"Gate {e[0]}: {e[1]} Exec Time: {int(exec_time)} Qubit: {e[2]} {e[3]}")
				exec_time += 1
				exec_time_for_each_qubit[e[2]] = exec_time_for_each_qubit[e[3]] = exec_time
			f.write("\n")


for i in range(5):
	generate_test_case(num, i, in_path, out_path)