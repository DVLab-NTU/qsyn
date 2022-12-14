import numpy as np
import sys
import glob
import os

out_path = "./test_case/out"
qsyn_out_path = "./test_case/qsyn_out"
record_file_path = "./test_case/record.txt"

out_path_list = glob.glob(os.path.join(out_path, "*.out"))
qsyn_out_path_list = glob.glob(os.path.join(qsyn_out_path, "*.out"))

# print(out_path_list)
# print(qsyn_out_path_list)

def get_first_and_last_idx(arr):
	first_idx = arr.index("Listed by gate ID\n")
	idxs = [idx for idx, val in enumerate(arr) if "Gate" in val]
	last_idx = 0
	if(len(idxs) == 0):
		last_idx = first_idx
	else:
		last_idx = idxs[-1]
	return first_idx, last_idx

def clean_arr(arr):
	
	arr = [e for e in arr if (e != "\n" and e != "\t")]
	arr = [e[0:len(e)-1]  if ("\n" in e or "\t" in e) else e for e in arr]

	return arr

# def compare(path1, path2):
 
	

for path1, path2 in zip(qsyn_out_path_list, out_path_list):
	# print(path1)
	# print(path2)
	with open(path1, "r") as f1, open(path2, "r") as f2:
		arr1 = f1.readlines()
		first_idx, last_idx = get_first_and_last_idx(arr1)
		arr1 = arr1[first_idx:last_idx+1]
		# print(arr)
		arr2 = f2.readlines()
		# print(np.array(arr1))
		# print(np.array(arr2))
		have_bug = False
		for e1, e2 in zip(arr1, arr2):
			e1 = e1.split(" ")
			e2 = e2.split(" ")
			e1 = [e for e in e1 if e != ""]
			e2 = [e for e in e2 if e != ""]
			e1 = clean_arr(e1)
			e2 = clean_arr(e2)
			# print(e1)
			# print(e2)
			# break
			if(len(e1) != len(e2) or len(e1) != sum([1 for i, j in zip(e1, e2) if i == j])):
				# print("have bug")\
				have_bug = True
				with open(record_file_path, "a") as f:
					print(e1)
					f.write(f"{path1} has bug\n")
				break
		# print("correct")
		if(have_bug == False):
			with open(record_file_path, "a") as f:
				f.write(f"{path1} is correct\n")
		

	# with open(path1, "r") as f1, open(path2, "r") as f2:
	# 	arr1 = f1.readlines()
	# 	first_idx, last_idx = get_first_and_last_idx(arr1)
	# 	arr1 = arr1[first_idx:last_idx+1]
	# 	# print(arr)
	# 	arr2 = f2.readlines()
	# 	# print(np.array(arr1))
	# 	# print(np.array(arr2))
	# 	for e1, e2 in zip(arr1, arr2):
	# 		e1 = e1.split(" ")
	# 		e2 = e2.split(" ")
	# 		e1 = [e for e in e1 if e != ""]
	# 		e2 = [e for e in e2 if e != ""]
	# 		e1 = clean_arr(e1)
	# 		e2 = clean_arr(e2)
	# 		print(e1)
	# 		print(e2)
	# 		# break
	# 		if(len(e1) != len(e2) or len(e1) != sum([1 for i, j in zip(e1, e2) if i == j])):
	# 			print("have bug")
	# 			break
	# 	print("correct")