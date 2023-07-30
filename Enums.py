import sys

class Line:
	def __init__(self):
		self.offset = 0
		self.identifier = ""
		self.value = ""
		self.comment = ""

class Enum:
	def __init__(self, decl):
		self.decl = decl
		self.entries = []
		self.max_id_len = 0
		self.max_val_len = 0
		self.max_com_len = 0

class TrieNode:
	def __init__(self):
		self.entry = None
		self.char = '\0'
		self.children = []
	
def parse(line_str):
	line = Line()
	r_str = line_str.lstrip()
		
	id_pos = 0
	val_pos = r_str.find('=')
	com_pos = r_str.find("//")
	id_end = 	val_pos if val_pos >= 0 else com_pos if com_pos >= 0 else len(r_str)
	val_end = 	com_pos if com_pos >= 0 else len(r_str)
	com_end = 	len(r_str)
	    
	if id_pos >= 0: 	line.identifier = 	r_str[id_pos:		id_end].strip()
	if val_pos >= 0:	line.value = 		r_str[val_pos + 1:	val_end].strip()
	if com_pos >= 0:	line.comment = 		r_str[com_pos + 2:	com_end].strip()
	line.offset = len(line_str) - len(r_str)

	return line
	
assert(len(sys.argv) == 3)

txt_path = sys.argv[1]
h_path = sys.argv[2]

namespace = ""
enums = []
raw_code = None

with open(txt_path, 'r') as txt:
	for line_str in txt:
		if len(line_str.strip()) > 0:
			if raw_code is not None:
				raw_code += line_str
			elif line_str.find("raw_code:") >= 0:
				raw_code = ""
			elif line_str.find("namespace") >= 0:
				namespace = line_str.split("=",1)[1].strip()
			else:
				line = parse(line_str)
				if line.offset > 0:
					enums[-1].entries.append(line)
				else:
					enums.append(Enum(line))

code = "#pragma once"	
code += "\n" + "//auto-generated"		
if len(namespace) > 0:
	code += "\n" + "namespace " + namespace
	code += "\n" + "{"

code += "\n"

for enum in enums:
	for entry in enum.entries:
		entry.value = " = " + entry.value + ", " if len(entry.value) > 0 else ""
		entry.comment = " // " + entry.comment if len(entry.comment) > 0 else ""

		enum.max_id_len = max(enum.max_id_len, len(entry.identifier))
		enum.max_val_len = max(enum.max_val_len, len(entry.value))
		enum.max_com_len = max(enum.max_com_len, len(entry.comment))

for enum in enums:
	code += "\n" + enum.decl.identifier + " : unsigned __int32" + (" //" + enum.decl.comment if(len(enum.decl.comment)) else "")
	code += "\n" + "{"
	for entry in enum.entries:		
		code += "\n" + "\t" + f'{entry.identifier + (", " if len(entry.value) == 0 else ""):<{enum.max_id_len}}{entry.value:<{enum.max_val_len}}{entry.comment:<{enum.max_com_len}}'
	code += "\n" + "\t" + 'INVALID'
	code += "\n" + "};\n"

if raw_code:
	code += "\n"
	code += raw_code
	code += "\n"

code += "\n" + "//EnumToString functions"

for enum in enums:
	enum_name = enum.decl.identifier.split()[-1]
	code += "\n\n" + "inline constexpr const char * EnumToString(" + enum_name + " value) noexcept"
	code += "\n" + "{"
	code += "\n" + "\t" + "switch (value)"
	code += "\n" + "\t" + "{"

	dupl_entry_lists = []
	for entry in enum.entries:
		matched = False
		if len(entry.value) > 0:
			for dupl_list in dupl_entry_lists:
				if dupl_list[0].value == entry.value:
					matched = True
					dupl_list.append(entry)
					break
		if matched == False:
			dupl_entry_lists.append([entry])

	for dupl_list in dupl_entry_lists:
		code += "\n" + 2 * "\t" + f'case {enum_name}::{dupl_list[0].identifier:<{enum.max_id_len}}: return "{" or ".join([e.identifier for e in dupl_list])}";'
	code += "\n" + "\t" + "}"
	code += "\n" + "\t" +'return nullptr;'
	code += "\n" + "}"

code += "\n"
code += "\n" + "//StringToEnum functions"
code += "\n"

code += "\n" + "template<typename EnumType>"
code += "\n" + "inline constexpr EnumType StringToEnum(const char* str) noexcept {}"

def make_trie(node, entries, i):
	entries = [e for e in entries if len(e.identifier) > i]

	while len(entries) > 0:
		c = entries[-1].identifier[i]
		branch = [e for e in entries if e.identifier[i] == c]
		entries = [e for e in entries if e.identifier[i] != c]

		child_node = TrieNode()
		child_node.entry = next(iter([e for e in branch if len(e.identifier) == i + 1]), None)
		child_node.char = c
		child_node.children = []

		make_trie(child_node, branch, i + 1)

		node.children.append(child_node)

	return node

def expand_string_enum(enum_name, node, i, offset):
	code = ""

	joint_list = []
	while node.entry is None and len(node.children) == 1:
		node = node.children[0]
		joint_list.append(node.char)

	if node.entry and len(joint_list) == 0:
		code += "\n" + (offset + 1) * "\t" + "if (str[" + str(i) + "] == '\\0') return " + enum_name + "::" + node.entry.identifier + ";"

	if len(joint_list) > 0:
		code += "\n" + (offset + 1) * "\t" + "if ("
		if len(joint_list) > 1: 
			code += "\n" + (offset + 1) * "\t" + "    "
		for j in range(0, len(joint_list)):
			code += (" &&" + "\n" + (offset + 1) * "\t" + "    " if j != 0 else "") + "str[" + str(i + j) + "] == '" + joint_list[j] + "'"
		if len(node.children) == 0:
			code += (" &&" + "\n" + (offset + 1) * "\t" + "    " if len(joint_list) != 0 else "") + "str[" + str(i + len(joint_list)) + "] == '\\0'"
		if len(joint_list) > 1: 
			code += "\n" + (offset + 1) * "\t" + "){"
		else:
			code += ")" + "\n" + (offset + 1) * "\t" + "{"
		if len(node.children) == 0:
			code += "\n" + (offset + 2) * "\t" + "return " + enum_name + "::" + node.entry.identifier + ";"
		else:
			code += expand_string_enum(enum_name, node, i + len(joint_list), offset + 1)
		code += "\n" + (offset + 1) * "\t" + "}"
	elif len(node.children) > 0:
		code += "\n" + (offset + 1) * "\t" + "switch (str[" + str(i) + "])"
		code += "\n" + (offset + 1) * "\t" + "{"

		for child in node.children:
			code += "\n" + (offset  + 1) * "\t" + "case '" + child.char + "':"
			code += expand_string_enum(enum_name, child, i + 1, offset + 1)
			code += "\n" + (offset  + 2) * "\t" + "break;"

		code += "\n" + (offset + 1) * "\t" + "}"
	return code

for enum in enums:
	enum_name = enum.decl.identifier.split()[-1]
	enum_trie = make_trie(TrieNode(), enum.entries, 0)
	code += "\n\n" + "template<> inline constexpr " + enum_name + " StringToEnum<" + enum_name + ">(const char* str) noexcept"
	code += "\n" + "{"
	code += "\n" + "\t" + "if (str == nullptr)"
	code += "\n" + "\t" + "{"
	code += "\n" + "\t\t" + "return " + enum_name + "::INVALID;"
	code += "\n" + "\t" + "}"
	code += expand_string_enum(enum_name, enum_trie, 0, 0)
	code += "\n" + "\t" + "return " + enum_name + "::INVALID;"
	code += "\n" + "}"

if len(namespace) > 0:
	code += "\n" + "}"

with open(h_path, 'w') as h_file:
    h_file.write(code)
