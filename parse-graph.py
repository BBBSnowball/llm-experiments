import re
from functools import reduce
import operator

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
    label = label.replace("\\>", ">")
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
    m = reduce(operator.mul, node.dimensions, 1)
    node.bitsize = x*m

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
    if match(".*-(\d+)$", node.name):
        layer = int(m[1])
    else:
        layer = -1
    node.layer = layer
    if layer >= 0 and prev_layer == layer-1:
        node.is_new_layer = True
    else:
        node.is_new_layer = False
    prev_layer = layer

for node in nodes_by_const_index:
    node.costs = {"read": node.bitsize/8}
    node.read_cost = node
    node.cost_is_paid = False
    # If we encounter this, we will read it from memory, so don't assume
    # that we will need some temporary storage for it like we will need
    # for intermediate results. This is sort-of true but maybe not if the
    # same input is used for more than one calculation.
    node.in_temp_storage = False

for node in nodes_by_op_index:
    node.read_cost = None
    node.in_temp_storage = True
    if node.operation == "rms_norm(x)":
        # norm is over first dimension
        n = node.dimensions[0]
        # It will be repeated if the other dimensions are >1.
        m = reduce(operator.mul, node.dimensions[1:], 1)

        # cost: sum over x*x, divide by n, sqrt, 1/x, scale all x by that
        #NOTE Each fmul may include an add.
        node.costs = { "fmul": n*2*m, "fdiv": m, "sqrt": m }
    elif node.operation == "x*y":
        # non-matrix mul
        m = reduce(operator.mul, node.dimensions, 1)
        node.costs = { "fmul": m }
    elif node.operation == "X*Y":
        if len(node.dimensions) == 2:
            assert len(node.dimensions) == 2
            assert len(node.inputs[0].dimensions) == 2
            assert len(node.inputs[1].dimensions) == 2
            assert node.inputs[0].dimensions[0] == node.inputs[1].dimensions[0]
            assert node.inputs[0].dimensions[1] == node.dimensions[0]
            assert node.inputs[1].dimensions[1] == node.dimensions[1]
            #
            #          BBBB
            #  AAA  x  BBBB  =  CCCC  (with 3 mul+add for each C)
            #  AAA     BBBB     CCCC
            c = node.dimensions[0] * node.dimensions[1] * node.inputs[0].dimensions[0]
            node.costs = { "fmul": c, "fadd": c }
        else:
            # Oh, well... We even seem to have indices that appear only once and vanish in the output.
            assert len(node.inputs[0].dimensions) == len(node.dimensions)
            assert len(node.inputs[1].dimensions) == len(node.dimensions)
            assert node.inputs[0].dimensions[0] == node.inputs[1].dimensions[0]
            m = reduce(operator.mul, node.dimensions, 1)
            node.costs = { "fmul": m * node.inputs[0].dimensions[0] }
    elif node.operation == "reshape(x)" or node.operation == "transpose(x)" or node.operation == "permute(x)":
        #TODO Does permute actually move data around?
        node.costs = {}
        node.read_cost = node.inputs[0]
        node.in_temp_storage = node.inputs[0].in_temp_storage
    elif node.operation == "view(x)":
        node.costs = {}
        # This is too pessimistic if the view is only reading part of it. We will fix this when we look at read_cost.
        node.read_cost = node.inputs[0]
        node.in_temp_storage = node.inputs[0].in_temp_storage
    elif node.operation == "x->y":
        node.costs = {"write": node.bitsize}
    elif node.operation == "soft_max(x)":
        ncols = node.inputs[0].dimensions[0]
        nrows = reduce(operator.mul, node.dimensions[1:], 1)
        # let's count compare as add
        node.costs = { "fpow": nrows, "fmul": ncols*nrows*3, "fadd": ncols*nrows*3, "fexp": ncols*nrows }
    elif node.operation == "cont(x)":
        # This seems to forward to DUP, i.e. make a copy.
        # -> Let's assume no cost, for now.
        node.costs = {}
    elif node.operation == "x+y":
        m = reduce(operator.mul, node.dimensions, 1)
        node.costs = { "fadd": m }
    elif node.operation == "unary(x)" and "silu" in node.name:
        # There are several unary functions but it's probably SILU based on the name:
        # Sigmoid Linear Unit (SiLU) function
        #   x/(1.0f + expf(-x))
        m = reduce(operator.mul, node.dimensions, 1)
        node.costs = { "fadd": 2*m, "fdiv": m, "fexp": m }
    elif node.operation == "get_rows(x)":
        node.costs = {"read": (node.bitsize + node.inputs[1].bitsize) / 8}
    elif node.operation == "rope(x)":
        # That one is complicated. Let's guess. Sorry.
        assert node.dimensions == node.inputs[0].dimensions
        assert node.inputs[1].dimensions == [1, 1]
        m = reduce(operator.mul, node.dimensions, 1)
        node.costs = { "fmul": m*3, "fsin": m }
    else:
        assert False, "TODO: handle operation: %r" % node.operation

for node in nodes_by_op_index:
    if "read" in node.costs:
        continue
    if node.read_cost is not None:
        continue
    if "read" in node.costs:
        # We have assigned a better value in the previous loop. Let's keep that.
        continue
    read_cost = 0
    for input in node.inputs:
        if node.operation == "x->y" and input is node.inputs[1]:
            continue
        if input is not None and input.read_cost is not None:
            rc = input.read_cost
            while rc is not None and rc.read_cost is not rc:
                rc = rc.read_cost
            if rc is None or rc.cost_is_paid:
                pass
            elif rc.bitsize == input.bitsize:
                read_cost += rc.bitsize / 8
                rc.cost_is_paid = True
            else:
                # We are reading only part of the data, so don't mark it paid.
                read_cost += input.bitsize / 8
    if read_cost > 0:
        node.costs["read"] = read_cost

max_temp_size = 0
total_costs = {}
for node in nodes_by_op_index:
    size = 0
    for node2 in traintrack_cols[node.index].values():
        if node2.in_temp_storage:
            size += node2.bitsize
    if size > 0:
        node.costs["tempsize"] = size/8
        max_temp_size = max(max_temp_size, size/8)

    for k,v in node.costs.items():
        if k == "tempsize":
            continue
        if k not in total_costs:
            total_costs[k] = 0
        total_costs[k] += v

print("Size of temporary storage is up to %d bytes. This is without KV cache, which is usually much larger!" % max_temp_size)
#print("Total cost: %r" % total_costs)
print("Total cost:")
for k,v in total_costs.items():
    if k == "read" or k == "write":
        v = "%.3f GiB" % (v/1024/1024/1024)
    else:
        v = "%g" % v
    print("  %-5s %s" % (k+":", v))

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

    if node.is_new_layer:
        print(prefix0 + "")
        print(prefix0 + "== layer %d ==" % node.layer)
    print(prefix0 + "")

    if node.operation == "x->y" and len(node.outputs_to) == 0:
        x = " (STORE)"
    elif node.operation == "x->y":
        x = " (COPY)"
    elif node.operation == "X*Y":
        x = " (MAT_MUL)"
    elif node.operation == "x*y":
        x = " (MUL)"
    elif node.operation == "unary(x)" and "silu" in node.name:
        # There are several unary functions but it's probably SILU based on the name:
        # Sigmoid Linear Unit (SiLU) function
        #   x/(1.0f + expf(-x))
        x = " (SILU?)"
    else:
        x = ""

    print(prefix1 + "%s: %s%s" % (node.name, node.operation, x))
    print(prefix2 + "  %.1f, %r" % (node.bitsize/8, node.dimensions))
    print(prefix2 + "  costs: %r" % (node.costs))
    if len(node.outputs_to) == 0:
        print(prefix2 + "  not used by other computations")
    if node.inputs[0]:
        x = node.inputs[0]
        print(prefix3 + "  x: %s, %.1f, %r" % (x.name, x.bitsize/8, x.dimensions))
    if node.inputs[1]:
        x = node.inputs[1]
        print(prefix4 + "  y: %s, %.1f, %r" % (x.name, x.bitsize/8, x.dimensions))

