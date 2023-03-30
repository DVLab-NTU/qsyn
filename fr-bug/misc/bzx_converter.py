myfile = open("output.zx", "r")
myout = open("new.zx", 'w')
while myfile:
    line  = myfile.readline().rstrip('\n')
    if line == "":
        break
    if line.startswith('//'):
        continue
    lst = line.split(' ')

    outstr = f"{lst[0]} ({lst[1]},{lst[2]})"
    for word in lst[3:]:
        outstr += f" {word}"

    myout.write(outstr + '\n')

    
myfile.close() 
myout.close()