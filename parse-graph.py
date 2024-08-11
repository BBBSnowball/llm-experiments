import re

def match(r, line):
    global m
    m = re.match(r, line)
    return m

nodes = []
edges = []
with open("graph2.dot", "r") as f:
    for line in f:
        if match(r"^digraph |^\s*\w+ =|^}$", line):
            pass
        elif match(r"^\s*(\S+) \[.*; label=\"([^\"]+)\"; \]$", line):
            nodes.append((m[1], m[2]))
        elif match(r"^\s*(\S+):x -> (\S+):x \[.*; label = \"(src [01])\"; \]", line):
            edges.append((m[1], m[2], m[3]))
        else:
            print("unknown line: " + repr(line))

class Datatype(object):
    def __init__(self, name, bitsize, is_float):
        self.name = name
        self.bitsize = bitsize
        self.is_float = is_float
datatypes = {}
datatypes["bf16"] = Datatype("bf16", 16, True)
datatypes["f16"] = Datatype("f16", 16, True)
datatypes["f32"] = Datatype("f32", 32, True)
datatypes["i32"] = Datatype("i32", 32, False)
datatypes["q4_0"] = Datatype("q4_0", 4.5, False)  # 16 + 32*4 bits for 32 numbers -> 4.5; see llama.cpp/ggml/src/ggml-common.h
datatypes["q6_K"] = Datatype("q6_K", 6.5625, False)  # 256*4 + 256*2 + 16*8 + 16 for 256 numbers = 6.5625

class Node(object):
    pass
class ConstantTensor(Node):
    pass
class Operation(Node):
    pass

nodes_by_addr = {}
nodes_by_op_index = []
nodes_by_const_index = []
for addr,label in nodes:
    if match(r"^(\S+)((?: \(reshaped\)| \(transposed\)| \(copy of .*\))*) \((\S+)\)\|(\d+) (\[[0-9, ]+\]) \| (?:<x>)?(.*)$", label):
        node = Operation()
        node.name = m[1]
        node.datatype = datatypes[m[3]]
        node.additional = m[2]
        assert len(nodes_by_op_index) == int(m[4]), "%d != %d" % (len(nodes_by_op_index), int(m[4]))
        node.dimensions = eval(m[5])
        node.operation = m[6]
        node.index = len(nodes_by_op_index)
        nodes_by_op_index.append(node)
    elif match(r"^<x>(\S+) \((\S+)\)\|CONST (\d+) (\[[0-9, ]+\])$", label):
        node = ConstantTensor()
        node.name = m[1]
        node.datatype = datatypes[m[2]]
        node.additional = ""
        assert len(nodes_by_const_index) == int(m[3]), "%d != %d" % (len(nodes_by_const_index), int(m[3]))
        node.dimensions = eval(m[4])
        node.index = len(nodes_by_const_index)
        nodes_by_const_index.append(node)
    else:
        assert False, "TODO: " + repr(label)
    node.addr = addr
    nodes_by_addr[addr] = node
    node.inputs = [None, None]

for a,b,label in edges:
    a = nodes_by_addr[a]
    b = nodes_by_addr[b]
    idx = int(label[4])
    b.inputs[idx] = a

for node in nodes_by_addr.values():
    x = node.datatype.bitsize
    for d in node.dimensions:
        x *= d
    node.bitsize = x

traintrack_cols = []
max_col = -1
if False:
    for node in nodes_by_op_index:
        while len(traintrack_cols) <= node.index:
            traintrack_cols.append({})
        node.input_traintracks = [-1, -1]
        for k in range(2):
            if isinstance(node.inputs[k], Operation):
                for i in range(100):
                    if all(map(lambda j: i not in traintrack_cols[j], range(node.inputs[k].index, node.index+1))):
                        node.input_traintracks[k] = i
                        for j in range(node.inputs[k].index, node.index+1):
                            traintrack_cols[j][i] = node.inputs[k]
                        max_col = max(max_col, i)
                        break
    traintrack_cols.append({})
else:
    while len(traintrack_cols) <= len(nodes_by_op_index):
        traintrack_cols.append({})
    for node in nodes_by_op_index:
        node.input_traintracks = [-1, -1]
        node.outputs_to = []
    for node in nodes_by_op_index:
        for node2 in node.inputs:
            if node2 is not None and isinstance(node2, Operation):
                node2.outputs_to.append(node)
    for node in nodes_by_op_index:
        if len(node.outputs_to) == 0:
            node.traintrack = -1
            continue
        last = max(map(lambda n: n.index, node.outputs_to))
        for i in range(100):
            if all(map(lambda j: i not in traintrack_cols[j], range(node.index, last+1))):
                node.traintrack = i
                for node2 in node.outputs_to:
                    for k in range(2):
                        if node2.inputs[k] == node:
                            node2.input_traintracks[k] = i
                for j in range(node.index, last+1):
                    traintrack_cols[j][i] = node
                max_col = max(max_col, i)
                break

prev_layer = -1
for node in nodes_by_op_index:
    # prefix1: outgoing arrow in first line
    prefix1 = ""
    x = " "
    for i in range(max_col+1):
        if i not in traintrack_cols[node.index]:
            prefix1 += x
        elif traintrack_cols[node.index][i] == node:
            prefix1 += '<'
            x = '-'
        else:
            prefix1 += '|'
        prefix1 += x
    #for i in range(max_col+1):
    #    if node.index > 0 and i not in traintrack_cols[node.index-1]:
    #        prefix1 += x
    #    elif traintrack_cols[node.index] in node.inputs:
    #        prefix1 += '>'
    #        x = '-'
    #    else:
    #        prefix1 += '|'
    #    prefix1 += x

    # prefix2: lines before inputs, i.e. draw lines for all active signals
    prefix2 = ""
    x = ' '
    for i in range(max_col+1):
        if i not in traintrack_cols[node.index]:
            prefix2 += x
        else:
            prefix2 += '|'
        prefix2 += x

    # prefix3: first input
    prefix3 = ""
    x = ' '
    for i in range(max_col+1):
        if i not in traintrack_cols[node.index]:
            prefix3 += x
        #elif traintrack_cols[node.index][i] == node.inputs[0] and x == " ":
        elif i == node.input_traintracks[0]:
            prefix3 += '>'
            x = '-'
        elif x != ' ':
            prefix3 += x
        else:
            prefix3 += '|'
        prefix3 += x

    # prefix4: second input
    prefix4 = ""
    x = ' '
    for i in range(max_col+1):
        if i not in traintrack_cols[node.index] or (traintrack_cols[node.index][i] == node.inputs[0] and traintrack_cols[node.index+1].get(i) != node.inputs[0]):
            prefix4 += x
        #elif traintrack_cols[node.index][i] == node.inputs[1] and x == " ":
        elif i == node.input_traintracks[1]:
            prefix4 += '>'
            x = '-'
        elif x != ' ':
            prefix4 += x
        else:
            prefix4 += '|'
        prefix4 += x

    # prefix0: before this node
    prefix0 = ""
    for i in range(max_col+1):
        if node.index == 0:
            prefix0 += "  "
            continue
        a = traintrack_cols[node.index-1].get(i)
        b = traintrack_cols[node.index].get(i)
        if a and a == b:
            prefix0 += "| "
        else:
            prefix0 += "  "

    # prefix5: after this node
    prefix5 = ""
    for i in range(max_col+1):
        a = traintrack_cols[node.index].get(i)
        b = traintrack_cols[node.index+1].get(i)
        if a and a == b:
            prefix5 += "| "
        else:
            prefix5 += "  "

    if match(".*-(\d+)$", node.name):
        layer = int(m[1])
    else:
        layer = -1
    if layer >= 0 and prev_layer == layer-1:
        print(prefix0 + "")
        print(prefix0 + "== layer %d ==" % layer)
    print(prefix0 + "")
    prev_layer = layer

    print(prefix1 + "%s: %s" % (node.name, node.operation))
    print(prefix2 + "  %.1f, %r" % (node.bitsize/8, node.dimensions))
    if len(node.outputs_to) == 0:
        print(prefix2 + "  not used by other computations")
    if node.inputs[0]:
        x = node.inputs[0]
        print(prefix3 + "  x: %s, %.1f, %r" % (x.name, x.bitsize/8, x.dimensions))
    if node.inputs[1]:
        x = node.inputs[1]
        print(prefix4 + "  y: %s, %.1f, %r" % (x.name, x.bitsize/8, x.dimensions))

