import re

def check_content(input):
    open_braces = 0
    for char in input:
        if char == "{":
            open_braces += 1
        elif char == "}":
            open_braces -= 1
        if open_braces < 0 or open_braces > 1:
            print(f"Warning: {open_braces} unmatched opening braces")
    

def format(input):
    input = re.sub(r"//.*", "", input) # remove comments
    input = input.replace("{", "").replace("}", "") # remove { and }
    input = input.replace("\n", "") # remove newlines
    input = input.replace(" ", "") # remove whitespace
    input = input.replace("\t", "") # remove whitespace
    input = input.split(",") # split on ,
    return input

def to_entries(input):
    entries = []
    for line in input:
        if "[" in line:
            before, after = line.split("[", 1)
            after = after[:-1]
            if ":" not in after:
                after = f"{after}:{after}"
            if "+:" in after:
                i1, i2 = after.split("+:", 1)
                after = f"{int(i1)+int(i2)-1}:{i1}"
            if "-:" in after:
                i1, i2 = after.split("-:", 1)
                after = f"{i1}:{int(i1)-int(i2)+1}"
            after = f"[{after}]"
            entries.append((before, after))
        elif "'" in line:
            before, after = line.split("'", 1)
            after = after[1:]
            entries.append((after, f"[{int(before)-1}:0]"))
        else:
            entries.append((line, None))
    return entries

def format_entries(entries):
    formatted_entries = []
    ml = 0
    for entry in entries:
        ml = max(ml, len(entry[0]))
    for entry in entries:
        if entry[1]:
            formatted_entries.append(f'{{"name":"{entry[0]}",{" " * (ml - len(entry[0]))} "width_spec":"{entry[1]}"}},')
        else:
            formatted_entries.append(f'{{"name":"{entry[0]}",{" " * (ml - len(entry[0]))} "width_spec":""}},')
    return formatted_entries

FNAME = "signals.scratch"
    
file = open(FNAME, "r")
content = file.read()
file.close()
check_content(content)
formatted_content = format(content)
entries = to_entries(formatted_content)
entries = format_entries(entries)
print(*entries, sep="\n")

