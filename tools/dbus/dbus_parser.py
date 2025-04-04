import dbus_config
import sys
from collections import Counter
from copy import deepcopy

USE_WARNINGS_INSTEAD_OF_ERRORS = True

def Critical(value):
    if USE_WARNINGS_INSTEAD_OF_ERRORS:
        print(f"Critical: {value}")
    else:
        Critical(value)

def print_daisy(daisy):
    daisy = daisy.copy()
    daisy.pop("inputs", None)
    return daisy

def print_tree(entries, offset = 0):
    for entry in entries:
        print("    " * offset + str(print_daisy(entry)))
        if "inputs" in entry:
            print_tree(entry["inputs"], offset + 1)

def match_daisy(a, b, id_filter=-256):
    name_match = a["name"] == b["name"]
    location_match = a["location"] == b["location"]
    if id_filter < 0:
        id_match = a["id"] != abs(int(id_filter))
    else:
        id_match = a["id"] == int(id_filter)
    return name_match and location_match and id_match

def daisy_list(daisy_list, multiinstance_list):
    # Verify all multi_daisies in multiinstance_list exist in daisy_list and have an id that is 0
    for multi_daisy in multiinstance_list:
        if not any(match_daisy(daisy, multi_daisy, 0) for daisy in daisy_list):
            Critical(f"Daisy {multi_daisy} in multiinstance_list does not exist in daisy_list")
    
    # Verify all daisies with id 0 in daisy_list exist in multiinstance_list
    for daisy in daisy_list:
        if daisy["id"] == 0 and not any(match_daisy(daisy, multi_daisy) for multi_daisy in multiinstance_list):
            Critical(f"Daisy {print_daisy(daisy)} in daisy_list does not exist in multiinstance_list")

    # Verify all multi_daisy in multiinstance_list have an id that is not 0
    for multi_daisy in multiinstance_list:
        if multi_daisy["id"] == 0:
            Critical(f"Multi_daisy {multi_daisy} in multiinstance_list has an id of 0")
        
    # Verify that num_inputs * width == i_debug_check and daisy_width == i_debug_daisy_check for each daisy in daisy_list
    for daisy in daisy_list:
        if daisy["num_inputs"] * daisy["width"] != daisy["i_debug_check"]:
            Critical(f"Daisy {print_daisy(daisy)} failed the i_debug_check validation")
        if daisy["daisy_width"] != daisy["i_debug_daisy_check"]:
            Critical(f"Daisy {print_daisy(daisy)} failed the i_debug_daisy_check validation")
        
    # Verify that all daisies have the same i_debug_daisy_check value
    i_debug_daisy_check_values = {daisy["i_debug_daisy_check"] for daisy in daisy_list}
    if len(i_debug_daisy_check_values) > 1:
        Critical(f"Not all daisies have the same i_debug_daisy_check value: {i_debug_daisy_check_values}")
    
    # Verify that all location, name, and note combinations in multiinstance_list are unique
    location_name_note_combinations = [(multi_daisy["location"], multi_daisy["name"], multi_daisy["note"]) for multi_daisy in multiinstance_list]
    if len(location_name_note_combinations) != len(set(location_name_note_combinations)):
        Critical("Not all location, name, and note combinations in multiinstance_list are unique")

    # Verify that all location and name combinations in daisy_list are unique
    location_name_combinations = [(daisy["location"], daisy["name"]) for daisy in daisy_list]
    if len(location_name_combinations) != len(set(location_name_combinations)):
        Critical("Not all location and name combinations in daisy_list are unique")

    # Warn if multiple daisies have the same id value in daisy_list or multiinstance_list (excluding id 0)
    combined_id_counts = Counter(daisy["id"] for daisy in daisy_list if daisy["id"] != 0) 
    + Counter(multi_daisy["id"] for multi_daisy in multiinstance_list if multi_daisy["id"] != 0)
    for id_value, count in combined_id_counts.items():
        if count > 1:
            print(f"Warning: {count} daisies in daisy_list have the same id value: {id_value}")

    multi_daisy_list = []
    for multi_daisy in multiinstance_list:
        for daisy in daisy_list:
            daisy = daisy.copy()
            if match_daisy(daisy, multi_daisy, 0):
                daisy["id"] = multi_daisy["id"]
                if multi_daisy["note"]:
                    daisy["name"] = multi_daisy["note"] + "_" + daisy["name"]
                multi_daisy_list.append(daisy)
                break
    
    daisy_list = [daisy for daisy in daisy_list if daisy["id"] != 0]
    daisy_list += multi_daisy_list

    daisy_list.sort(key=lambda daisy: daisy["id"])

    return daisy_list

def signal_list(signals, reqired_signals):
    # Verify that all signals have a unique name
    signal_names = [signal["name"] for signal in signals]
    if len(signal_names) != len(set(signal_names)):
        Critical("Not all signals have a unique name")

    # Warn if any signal has a width of 0
    for signal in signals:
        if signal["width"] == 0:
            print(f"Warning: Signal {signal['name']} has a width of 0")

    # Verify that leaf signals do not have inputs
    for signal in signals:
        if signal["leaf_signal"] == 1 and len(signal["inputs"]) > 0:
            Critical(f"Leaf signal {signal['name']} should not have inputs")
        if signal["leaf_signal"] == 0 and len(signal["inputs"]) == 0:
            if signal["required"] == 1:
                print(f"Warning: Required Non-leaf signal {signal['name']} should have inputs")
            else:
                print(f"Info: Non-required Non-leaf signal {signal['name']} should have inputs")
    # Warn if any signal of width 1 is not marked as leaf_signa
    for signal in signals:
        if signal["width"] == 1 and signal["leaf_signal"] != 1:
            print(f"Warning: Signal {signal['name']} has a width of 1 but is not marked as leaf_signal")

    # Verify that all required signals exist in signals and have required set to 1
    missing_required_signals = [req_signal for req_signal in reqired_signals if req_signal not in [signal["name"] for signal in signals]]
    required_signals_with_wrong_flag = [signal["name"] for signal in signals if signal["name"] in reqired_signals and signal["required"] != 1]

    if len(missing_required_signals) > 0:
        print("Missing required signals:")
        print(*missing_required_signals, sep="\n")
        Critical("There are missing required signals.")

    if len(required_signals_with_wrong_flag) > 0:
        print("Required signals with 'required' flag set to 0:")
        print(*required_signals_with_wrong_flag, sep="\n")
        Critical("There are required signals with 'required' flag set to 0.")

    return signals

def embed_signal(input, signal):
    if signal is None:
        return
    input["width"] = signal["width"]
    input["leaf_signal"] = signal["leaf_signal"]
    input["required"] = signal["required"]
    input["resolved"] = signal["resolved"]
    input["soft_resolved"] = signal["soft_resolved"]
    input["inputs"] = deepcopy(signal["inputs"])
    input["sliced_width"] = signal["width"]
    input["computed_width"] = signal["computed_width"]
    if "width_spec" in input and ":" in input["width_spec"]:
        input["sliced_width"] = int(input["width_spec"][1:-1].split(":")[0]) - int(input["width_spec"][1:-1].split(":")[1]) + 1

def resolve_signal(signal):
    if "leaf_signal" in signal and signal["leaf_signal"] == 1:
        signal["resolved"] = 1
        signal["soft_resolved"] = 1
        signal["computed_width"] = signal["width"]
    else:
        resolved = 1 if len(signal["inputs"]) > 0 else 0
        soft_resolved = 1
        computed_width = 0
        for input in signal["inputs"]:
            if "resolved" not in input or input["resolved"] == 0:
                resolved = 0
            if "sliced_width" in input:
                computed_width += input["sliced_width"]
            if "width" not in input or input["width"] == 0:
                soft_resolved = 0
        signal["resolved"] = resolved
        signal["soft_resolved"] = soft_resolved
        signal["computed_width"] = computed_width
        compare_width = signal["width"] if "num_inputs" not in signal else signal["num_inputs"] * signal["width"]
        if compare_width != signal["computed_width"]:
            if all("width" in input and input["width"] != 0 for input in signal["inputs"]) and len(signal["inputs"]) > 0:
                print(f"Warning: Signal {signal['name']} has a width of {compare_width} but the computed width is {signal['computed_width']}")
            else:
                print(f"Info: Signal {signal['name']} has a width of {compare_width} but the computed width is {signal['computed_width']} but that can be due to missing inputs")

def rec_resolve_signal(name, signals, signal_cache, missing_signals, alias_signals):
    if name in signal_cache:
        return signal_cache[name]
    for signal in signals:
        if signal["name"] == name:
            for input in signal["inputs"]:
                if "alias" in input and input["alias"] not in alias_signals:
                    alias_signals.append(input["alias"])
                resolved = rec_resolve_signal(input["alias"] if "alias" in input else input["name"], signals, signal_cache, missing_signals, alias_signals)
                embed_signal(input, resolved)
            resolve_signal(signal)
            signal_cache[name] = signal
            return signal
    if name not in missing_signals:
        missing_signals.append(name)
    return None

def build_signal_tree(daisies, signals, signal_cache, missing_signals, alias_signals):
    for daisy in daisies:
        for input in daisy["inputs"]:
            if "alias" in input and input["alias"] not in alias_signals:
                alias_signals.append(input["alias"])
            resolved = rec_resolve_signal(input["alias"] if "alias" in input else input["name"], signals, signal_cache, missing_signals, alias_signals)
            embed_signal(input, resolved)
        resolve_signal(daisy)
    return daisies

def signal_tree(daisies, signals, signal_cache, missing_signals, alias_signals):
    # Verify that all unresolved signals are not required
    unresolved_required_signals = [signal for signal in signal_cache.values() if signal["resolved"] == 0 and signal["required"] == 1]
    if len(unresolved_required_signals) > 0:
        for signal in unresolved_required_signals:
            print(f"Error: Signal name: {signal['name']}, required but unresolved")
        Critical("There are unresolved required signals.")
    
    # Verify that each signal and daisy input is only named once as an input
    input_names = []
    for signal in signals:
        for input_signal in signal["inputs"]:
            if input_signal["name"] in input_names:
                if input_signal.get("leaf_signal", 0) == 1:
                    print(f"Warning: Leaf input {input_signal['name']} is named more than once as an input")
                else:
                    print(f"Error: Non-leaf input {input_signal['name']} is named more than once as an input")
            elif input_signal["name"] not in missing_signals:
                input_names.append(input_signal["name"])
    for daisy in daisies:
        for input_signal in daisy["inputs"]:
            if input_signal["name"] in input_names:
                if input_signal.get("leaf_signal", 0) == 1:
                    print(f"Warning: Leaf input {input_signal['name']} is named more than once as an input")
                else:
                    print(f"Error: Non-leaf input {input_signal['name']} is named more than once as an input")
            elif input_signal["name"] not in missing_signals:
                input_names.append(input_signal["name"])

    # Verify that each signal is either used only as a name or as an alias
    name_alias_conflicts = set(input_names).intersection(alias_signals)
    if name_alias_conflicts:
        for conflict in name_alias_conflicts:
            if any(signal["name"] == conflict and signal.get("leaf_signal", 0) == 1 for signal in signals):
                print(f"Warning: Leaf signal {conflict} is used both as a name and as an alias")
            else:
                print(f"Error: Non-leaf signal {conflict} is used both as a name and as an alias")

    return daisies

def rec_prop_bits(daisy, offset):
    daisy["start_bit"] = offset
    if "width_spec" in daisy and ":" in daisy["width_spec"]:
        offset -= int(daisy["width_spec"][1:-1].split(":")[1])
    if "inputs" not in daisy:
        return 0
    for input in reversed(daisy["inputs"]):
        inc = rec_prop_bits(input, offset)
        offset += inc
    return daisy["sliced_width"] if "sliced_width" in daisy else daisy["width"]
        

def prop_bits(daisies):
    for daisy in daisies:
        rec_prop_bits(daisy, 0)
    return daisies

def traverse(node, path, result):
    path.append(node)
    if "inputs" not in node or not node["inputs"]:
        result.append(path[:])
    else:
        for child in node["inputs"]:
            traverse(child, path, result)
    path.pop()

def flatten(daisies):
    result = []
    for daisy in daisies:
        traverse(daisy, [], result)
    return result

def prune_1(flat_list):
    result = []
    for path in flat_list:
        if "width" in path[-1]:
            if "leaf_signal" not in path[-1] or path[-1]["leaf_signal"] == 0:
                print(f"Warning: Non-leaf signal {path[-1]['name']} at the end of the path")
            else:
                result.append(path)
    return result

def prune_1_5(flat_list):
    result = []
    for path in flat_list:
        if path[-1]["name"] == "0" or path[-1]["name"] == "1":
            continue
        if "alias" in path[-1] and (path[-1]["alias"] == "0" or path[-1]["alias"] == "1"):
            continue
        result.append(path)
    return result

def prune_1_6(flat_list):
    result = []
    for path in flat_list:
        if any([step["soft_resolved"] == 0 for step in path]):
            continue
        result.append(path)
    return result

def update_path(path):
    path = deepcopy(path)
    low = 0
    high = path[0]["width"] * path[0]["num_inputs"] - 1
    path[0]["low"] = low
    path[0]["high"] = high
    for i in range(1, len(path)):
        low = max(low, path[i]["start_bit"])
        if low != path[i]["start_bit"]:
            #print(f"Warning: {path[i]['name']} start_bit {path[i]['start_bit']} is not equal to the previous low {low}")
            pass
        high = min(high, path[i]["start_bit"] + path[i]["sliced_width"] - 1)
        if high != path[i]["start_bit"] + path[i]["sliced_width"] - 1:
            #print(f"Warning: {path[i]['name']} start_bit + sliced_width {path[i]['start_bit'] + path[i]['sliced_width'] - 1} is not equal to the previous high {high}")
            pass
        path[i]["low"] = low
        path[i]["high"] = high
    return path

def prune_2(flat_list):
    result = []
    for path in flat_list:
        path = update_path(path)
        if path[-1]["low"] <= path[-1]["high"]:
            result.append(path)
            if path[-1]["low"] != path[-1]["start_bit"]:
                #print(f"Warning: {path[-1]['name']} low {path[-1]['low']} is not equal to start_bit {path[-1]['start_bit']}")
                pass
            if path[-1]["high"] != path[-1]["start_bit"] + path[-1]["sliced_width"] - 1:
                #print(f"Warning: {path[-1]['name']} high {path[-1]['high']} is not equal to start_bit + sliced_width - 1 {path[-1]['start_bit'] + path[-1]['sliced_width'] - 1}")
                pass
    return result

def calc_fbr(flat_list):
    for path in flat_list:
        lstop  = path[-1]
        ws_fbr_low = int(lstop["width_spec"][1:-1].split(":")[1]) if ":" in lstop["width_spec"] else 0
        ws_fbr_high = int(lstop["width_spec"][1:-1].split(":")[0]) if ":" in lstop["width_spec"] else lstop["sliced_width"] - 1
        if ws_fbr_low != 0:
            #print(f"Warning: {lstop['name']} width_spec low {ws_fbr_low} is not equal to 0")
            pass
        if ws_fbr_high != lstop["sliced_width"] - 1:
            #print(f"Warning: {lstop['name']} width_spec high {ws_fbr_high} is not equal to sliced_width - 1 {lstop['sliced_width'] - 1}")
            pass
        if lstop["low"] < lstop["start_bit"]:
            Critical(f"Warning: {lstop['name']} low {lstop['low']} is less than start_bit {lstop['start_bit']}")
        if lstop["high"] > lstop["start_bit"] + lstop["sliced_width"] - 1:
            Critical(f"Warning: {lstop['name']} high {lstop['high']} is greater than start_bit + sliced_width - 1 {lstop['start_bit'] + lstop['sliced_width'] - 1}")
        if lstop["low"] != lstop["start_bit"]:
            ws_fbr_low += lstop["low"] - lstop["start_bit"]
            print(f"Warning: {lstop['name']} low {lstop['low']} is not equal to start_bit {lstop['start_bit']}")
        if lstop["high"] != lstop["start_bit"] + lstop["sliced_width"] - 1:
            ws_fbr_high -= lstop["start_bit"] + lstop["sliced_width"] - 1 - lstop["high"]
            print(f"Warning: {lstop['name']} high {lstop['high']} is not equal to start_bit + sliced_width - 1 {lstop['start_bit'] + lstop['sliced_width'] - 1}")
        lstop["fbr_low"] = ws_fbr_low
        lstop["fbr_high"] = ws_fbr_high
    return flat_list

def split_data(data, path, res):
    name = data["name"]
    fbr_low = data["fbr_low"]
    fbr_high = data["fbr_high"]
    low = data["low"]
    high = data["high"]
    daisy_id = data["daisy_id"]
    
    
    start_bit = data["start_bit"]
    end_bit = data["end_bit"]
    ts = start_bit
    te = 31

    while True:
        print(f"Splitting {name} from {ts} to {te}")

        offset = ts - start_bit

        print_data = {}
        print_data["name"] = name
        print_data["fbr_low"] = fbr_low + offset
        print_data["fbr_high"] = print_data["fbr_low"] + (te - ts)
        print_data["low"] = low + offset
        print_data["high"] = print_data["low"] + (te - ts)

        print_data["daisy_id"] = daisy_id

        print_data["sig_sel"] = print_data["low"] // 128
        print_data["rd_sel"] = ((print_data["low"] % 128) // 32) * 2
        print_data["start_bit"] = print_data["low"] % 32
        print_data["num_bits"] = print_data["high"] - print_data["low"] + 1
        print_data["end_bit"] = print_data["start_bit"] + print_data["num_bits"] - 1
        print_data["mask"] = (1 << print_data["num_bits"]) - 1

        assert print_data["end_bit"] <= 31

        print_data["name_cnt"] = len(path) - 2

        if print_data["fbr_low"] != 0 or print_data["fbr_high"] != path[-1]["width"] - 1:
            print_data["name"] = f"{print_data['name']}__{print_data['fbr_high']}_{print_data['fbr_low']}"

        res.append((print_data.copy(), path))

        if te == end_bit:
            break
        ts = te + 1
        te = ts + 31 if ts + 31 < end_bit else end_bit



def prep_for_print(flat_list):
    res = []
    for path in flat_list:
        print_data = {}
        print_data["name"] = path[-1]["name"].upper().replace(" ", "_")
        print_data["fbr_low"] = path[-1]["fbr_low"]
        print_data["fbr_high"] = path[-1]["fbr_high"]
        print_data["low"] = path[-1]["low"]
        print_data["high"] = path[-1]["high"]
        
        print_data["daisy_id"] = path[0]["id"]
        
        print_data["sig_sel"] = print_data["low"] // 128
        print_data["rd_sel"] = ((print_data["low"] % 128) // 32) * 2
        print_data["start_bit"] = print_data["low"] % 32
        print_data["num_bits"] = print_data["high"] - print_data["low"] + 1
        print_data["end_bit"] = print_data["start_bit"] + print_data["num_bits"] - 1
        print_data["mask"] = (1 << print_data["num_bits"]) - 1

        print_data["name_cnt"] = len(path) - 2

        if print_data["end_bit"] > 31:
            print(print_data)
            split_data(print_data, path, res)
        else:
            if print_data["fbr_low"] != 0 or print_data["fbr_high"] != path[-1]["width"] - 1:
                print_data["name"] = f"{print_data['name']}__{print_data['fbr_high']}_{print_data['fbr_low']}"
            res.append((print_data.copy(), path))

    while True:
        name_counts = Counter(print_data["name"] for print_data, _ in res)
        non_unique_names = {name for name, count in name_counts.items() if count > 1}
        if len(non_unique_names) == 0:
            break
        for print_data, path in res:
            if print_data["name"] in non_unique_names:
                if print_data["name_cnt"] < 0:
                    add_name = print_data["low"]
                    print_data["name"] = f"{print_data['name']}__{add_name}"
                else:
                    add_name = path[print_data["name_cnt"]]["name"].upper().replace(" ", "_")
                    print_data["name"] = f"{add_name}_{print_data['name']}"
                    print_data["name_cnt"] -= 1

    return res

def print_desc(path):
    for step in path:
        location = (" location: " + step["location"]) if "location" in step else ""
        name = (" name: " + step["name"])
        alias = (" alias: " + step["alias"]) if "alias" in step else ""
        note = (" note: " + step["note"]) if "note" in step else ""
        print(f"// {location}{name}{alias}{note}")

def print_cpp(ppl):
    for print_data, path in ppl:
        print_desc(path)
        print_data["name"] = ''.join(c if c.isalnum() else '_' for c in print_data["name"])
        print(f'#define {print_data["name"]}_WORD {{{print_data["sig_sel"]}, {print_data["daisy_id"]}, {print_data["rd_sel"]}, 0, 1, 0}}')
        print(f'#define {print_data["name"]} {{{print_data["name"]}_WORD, {print_data["start_bit"]}, 0x{print_data["mask"]:X}}}')
        print()

signal_cache = {}
missing_signals = []
alias_signals = []

daisies = daisy_list(dbus_config.DBUS_DAISIES, dbus_config.DBUS_MULTIINSTANCES)
signals = signal_list(dbus_config.DBUS_SIGNALS, dbus_config.DBUS_REQUIRED_SIGNALS)
daisies = build_signal_tree(daisies, signals, signal_cache, missing_signals, alias_signals)
daisies = signal_tree(daisies, signals, signal_cache, missing_signals, alias_signals)
daisies = prop_bits(daisies)
flat_list = flatten(daisies)
flat_list_p1 = prune_1(flat_list)
flat_list_p1_5 = prune_1_5(flat_list_p1)
flat_list_p1_6 = prune_1_6(flat_list_p1_5)
flat_list_p2 = prune_2(flat_list_p1_6)
flat_list_fbr = calc_fbr(flat_list_p2)
ppl = prep_for_print(flat_list_fbr)

print()
print("Daisies:")
print(*[print_daisy(daisy) for daisy in daisies], sep="\n")
print()

print()
print("Resolved daisies:")
print_tree(daisies)
print()

print()
print("Signals:")
print(f"Resolved: {len([signal for signal in signal_cache.keys() if signal_cache[signal]['resolved'] == 1])}, unresolved: {len([signal for signal in signal_cache.keys() if signal_cache[signal]['resolved'] == 0])}, total: {len(signal_cache)}, missing: {len(missing_signals)}")
print(f"Leaf: {len([signal for signal in signal_cache.keys() if signal_cache[signal]['leaf_signal'] == 1])}, non Leaf: {len([signal for signal in signal_cache.keys() if signal_cache[signal]['leaf_signal'] == 0])}, total: {len(signals)}")
print()

print()
print("Resolved signals:")
print(*[signal for signal in signal_cache.keys() if signal_cache[signal]['resolved'] == 1], sep="\n")
print()

print()
print("Unresolved signals:")
print(*[signal for signal in signal_cache.keys() if signal_cache[signal]['resolved'] == 0], sep="\n")
print()

print()
print("Alias signals:")
print(*alias_signals, sep="\n")
print()

print()
print("Missing signals:")
print(*[f'{{"name":"{signal}", "width":0, "leaf_signal":0, "required":1, "inputs":[]}},' for signal in missing_signals], sep="\n")
print()

print()
print("Flat list:", len(flat_list))
#print(*[[print_daisy(step) for step in path] for path in flat_list], sep="\n")
print()

print()
print("Prune 1 list:", len(flat_list_p1))
#print(*[[print_daisy(step) for step in path] for path in flat_list_p1], sep="\n")
print()

print()
print("Prune 1.5 list:", len(flat_list_p1_5))
#print(*[[print_daisy(step) for step in path] for path in flat_list_p1_5], sep="\n")
print()

print()
print("Prune 1.6 list:", len(flat_list_p1_6))
#print(*[[print_daisy(step) for step in path] for path in flat_list_p1_6], sep="\n")
print()

print()
print("Prune 2 list:", len(flat_list_p2))
#print(*[[print_daisy(step) for step in path] for path in flat_list_p2], sep="\n")
print()

print()
print("Final list:", len(flat_list_fbr))
#print(*[[print_daisy(step) for step in path] for path in flat_list_fbr], sep="\n")
print()

print()
print("Final list for print:", len(ppl))
print(*[print_data for print_data, step in ppl], sep="\n")
print()

print()
print("C++ print:")
print()
print_cpp(ppl)