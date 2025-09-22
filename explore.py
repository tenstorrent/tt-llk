# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import os

import tree_sitter_cpp as tscpp
from tree_sitter import Language, Parser

TOP_LEVEL_FOLDERS = ["tt_llk_wormhole_b0", "tt_llk_blackhole", "tt_llk_quasar"]


def explore_directory_structure(top_level_folders):
    result = []
    for folder in top_level_folders:
        folder_data = []
        folder_path = os.path.join(os.getcwd(), folder)
        for item in sorted(os.listdir(folder_path)):
            item_path = os.path.join(folder_path, item)
            if os.path.isdir(item_path):
                subdir_structure = []
                for root, dirs, files in os.walk(item_path):
                    rel_path = os.path.relpath(root, item_path)
                    if rel_path == ".":
                        subdir_structure.append([item, sorted(files)])
                    else:
                        subdir_structure.append([f"{item}/{rel_path}", sorted(files)])
                folder_data.extend(subdir_structure)
        result.append([folder, folder_data])
    return result


def filter_cpp_files(directory_structure):
    cpp_extensions = {
        ".c",
        ".cpp",
        ".cc",
        ".cxx",
        ".c++",
        ".h",
        ".hpp",
        ".hh",
        ".hxx",
        ".h++",
    }
    filtered_result = []

    for folder_name, subdirs in directory_structure:
        filtered_subdirs = []
        for subdir_name, files in subdirs:
            cpp_files = sorted(
                [f for f in files if any(f.endswith(ext) for ext in cpp_extensions)]
            )
            if cpp_files:
                filtered_subdirs.append([subdir_name, cpp_files])
        filtered_subdirs.sort(key=lambda x: x[0])
        if filtered_subdirs:
            filtered_result.append([folder_name, filtered_subdirs])

    return filtered_result


def extract_parameter_info(param_node, source_code):
    """Extract parameter type, name, and default value from a parameter node"""
    param_text = source_code[param_node.start_byte : param_node.end_byte].decode(
        "utf-8"
    )

    # Initialize default values
    param_type_parts = []
    param_type = ""
    param_name = ""
    default_value = None

    # Handle different parameter node types
    if param_node.type in ["parameter_declaration", "optional_parameter_declaration"]:
        # First pass: collect all type-related tokens
        type_tokens = []
        name_candidates = []

        for child in param_node.children:
            child_text = source_code[child.start_byte : child.end_byte].decode("utf-8")

            # Type-related nodes and qualifiers
            if child.type in [
                "primitive_type",
                "type_identifier",
                "qualified_identifier",
                "template_type",
                "sized_type_specifier",
                "struct_specifier",
                "class_specifier",
                "enum_specifier",
                "union_specifier",
            ]:
                type_tokens.append(child_text)
            elif child_text in [
                "const",
                "constexpr",
                "volatile",
                "static",
                "extern",
                "inline",
            ]:
                type_tokens.append(child_text)

            # Reference and pointer indicators
            elif child.type == "&":
                type_tokens.append("&")
            elif child.type == "*":
                type_tokens.append("*")
            elif child.type == "reference_declarator":
                # Handle reference declarator (e.g., const Type& name)
                for ref_child in child.children:
                    ref_text = source_code[
                        ref_child.start_byte : ref_child.end_byte
                    ].decode("utf-8")
                    if ref_child.type == "&":
                        type_tokens.append("&")
                    elif ref_child.type == "identifier":
                        name_candidates.append(ref_text)
            elif child.type == "pointer_declarator":
                # Handle pointer declarator (e.g., Type* name)
                for ptr_child in child.children:
                    ptr_text = source_code[
                        ptr_child.start_byte : ptr_child.end_byte
                    ].decode("utf-8")
                    if ptr_child.type == "*":
                        type_tokens.append("*")
                    elif ptr_child.type == "identifier":
                        name_candidates.append(ptr_text)

            # Parameter name
            elif child.type == "identifier":
                name_candidates.append(child_text)

            # Default value
            elif child.type == "=" and child.next_sibling:
                default_value = source_code[
                    child.next_sibling.start_byte : child.next_sibling.end_byte
                ].decode("utf-8")

        # Reconstruct the type from tokens
        param_type = " ".join(type_tokens).strip()

        # Use the last identifier as the parameter name (most likely to be correct)
        if name_candidates:
            param_name = name_candidates[-1]

        # If we couldn't parse properly, fall back to extracting from full text
        if not param_type and not param_name:
            # Try to split the parameter text manually
            # Remove default value part first
            if "=" in param_text:
                param_text_no_default = param_text.split("=")[0].strip()
                default_value = param_text.split("=")[1].strip()
            else:
                param_text_no_default = param_text

            # Split into tokens and try to identify type vs name
            tokens = param_text_no_default.split()
            if tokens:
                # Last token is likely the name, everything else is type
                param_name = tokens[-1]
                param_type = " ".join(tokens[:-1])

    return {
        "type": param_type.strip(),
        "name": param_name.strip(),
        "default_value": default_value.strip() if default_value else None,
    }


def extract_template_parameters(template_node, source_code):
    """Extract template parameters from template_parameter_list"""
    template_params = []

    if template_node and template_node.type == "template_parameter_list":
        for child in template_node.children:
            if child.type == "type_parameter_declaration":
                param_name = ""
                default_value = None

                for subchild in child.children:
                    if subchild.type == "type_identifier":
                        param_name = source_code[
                            subchild.start_byte : subchild.end_byte
                        ].decode("utf-8")
                    elif subchild.type == "=" and subchild.next_sibling:
                        default_value = source_code[
                            subchild.next_sibling.start_byte : subchild.next_sibling.end_byte
                        ].decode("utf-8")

                template_params.append(
                    {
                        "type": "typename",
                        "name": param_name,
                        "default_value": default_value,
                    }
                )
            elif child.type == "optional_parameter_declaration":
                # Handle template parameters with default values like: bool untilize_en = false
                param_info = extract_parameter_info(child, source_code)
                template_params.append(param_info)
            elif child.type == "parameter_declaration":
                param_info = extract_parameter_info(child, source_code)
                template_params.append(param_info)

    return template_params


def parse_cpp_functions(file_structure):
    """Parse C/C++ files and extract function/method declarations and definitions"""
    CPP_LANGUAGE = Language(tscpp.language())
    parser = Parser(CPP_LANGUAGE)

    result = []

    for folder_name, subdirs in file_structure:
        folder_functions = []

        for subdir_name, files in subdirs:
            subdir_functions = []

            for file_name in files:
                file_path = os.path.join(
                    os.getcwd(), folder_name, subdir_name, file_name
                )

                try:
                    with open(file_path, "rb") as f:
                        source_code = f.read()

                    tree = parser.parse(source_code)

                    def extract_functions(node):
                        functions = []

                        if node.type == "template_declaration":
                            # Handle template function
                            func_info = {
                                "name": "",
                                "return_type": "",
                                "template_parameters": [],
                                "parameters": [],
                            }

                            # Extract template parameters
                            template_params_node = None
                            function_def_node = None
                            for child in node.children:
                                if child.type == "template_parameter_list":
                                    template_params_node = child
                                elif child.type == "function_definition":
                                    function_def_node = child

                            if template_params_node:
                                func_info["template_parameters"] = (
                                    extract_template_parameters(
                                        template_params_node, source_code
                                    )
                                )

                            # Extract function details from the function_definition child
                            if function_def_node:
                                for child in function_def_node.children:
                                    if child.type == "function_declarator":
                                        # Get function name and parameters
                                        for subchild in child.children:
                                            if subchild.type == "identifier":
                                                func_info["name"] = source_code[
                                                    subchild.start_byte : subchild.end_byte
                                                ].decode("utf-8")
                                            elif subchild.type == "parameter_list":
                                                # Extract parameters
                                                for param in subchild.children:
                                                    if (
                                                        param.type
                                                        == "parameter_declaration"
                                                    ):
                                                        param_info = (
                                                            extract_parameter_info(
                                                                param, source_code
                                                            )
                                                        )
                                                        func_info["parameters"].append(
                                                            param_info
                                                        )

                                    # Extract return type (look for primitive_type, type_identifier, etc.)
                                    elif child.type in [
                                        "primitive_type",
                                        "type_identifier",
                                        "qualified_identifier",
                                    ]:
                                        func_info["return_type"] = source_code[
                                            child.start_byte : child.end_byte
                                        ].decode("utf-8")

                            if func_info["name"]:
                                functions.append(func_info)

                            # Don't recurse into template_declaration children since we've already processed them

                        elif node.type in [
                            "function_definition",
                            "function_declarator",
                            "method_definition",
                        ]:
                            # Skip function_definition nodes that are children of template_declaration
                            if (
                                node.parent
                                and node.parent.type == "template_declaration"
                            ):
                                # This function is part of a template, skip it here as it's handled above
                                pass
                            else:
                                # Handle non-template function
                                func_info = {
                                    "name": "",
                                    "return_type": "",
                                    "template_parameters": [],
                                    "parameters": [],
                                }

                                # Extract function details
                                for child in node.children:
                                    if child.type == "function_declarator":
                                        # Get function name
                                        for subchild in child.children:
                                            if subchild.type == "identifier":
                                                func_info["name"] = source_code[
                                                    subchild.start_byte : subchild.end_byte
                                                ].decode("utf-8")
                                            elif subchild.type == "parameter_list":
                                                # Extract parameters
                                                for param in subchild.children:
                                                    if (
                                                        param.type
                                                        == "parameter_declaration"
                                                    ):
                                                        param_info = (
                                                            extract_parameter_info(
                                                                param, source_code
                                                            )
                                                        )
                                                        func_info["parameters"].append(
                                                            param_info
                                                        )

                                    # Extract return type (look for primitive_type, type_identifier, etc.)
                                    elif child.type in [
                                        "primitive_type",
                                        "type_identifier",
                                        "qualified_identifier",
                                    ]:
                                        func_info["return_type"] = source_code[
                                            child.start_byte : child.end_byte
                                        ].decode("utf-8")

                                if func_info["name"]:
                                    functions.append(func_info)

                        # Recursively check children, but skip children of template_declaration we've already processed
                        if node.type != "template_declaration":
                            for child in node.children:
                                functions.extend(extract_functions(child))

                        return functions

                    file_functions = extract_functions(tree.root_node)
                    if file_functions:
                        subdir_functions.append([file_name, file_functions])

                except Exception as e:
                    print(f"Error parsing {file_path}: {e}")

            if subdir_functions:
                folder_functions.append([subdir_name, subdir_functions])

        if folder_functions:
            result.append([folder_name, folder_functions])

    return result


def pivot_on_arguments(flattened_by_folder):
    """Pivot the function data to create lists of arguments tagged with their source functions"""
    arguments_by_folder = []

    for folder_name, functions in flattened_by_folder:
        all_arguments = []

        for func in functions:
            # Process template parameters
            for template_param in func["template_parameters"]:
                arg_info = {
                    "name": template_param["name"],
                    "type": template_param["type"],
                    "default_value": template_param["default_value"],
                    "parameter_category": "template",
                    "source_function": func["name"],
                    "source_file": func["source_file"],
                    "function_return_type": func["return_type"],
                }
                all_arguments.append(arg_info)

            # Process runtime parameters
            for runtime_param in func["parameters"]:
                arg_info = {
                    "name": runtime_param["name"],
                    "type": runtime_param["type"],
                    "default_value": runtime_param["default_value"],
                    "parameter_category": "runtime",
                    "source_function": func["name"],
                    "source_file": func["source_file"],
                    "function_return_type": func["return_type"],
                }
                all_arguments.append(arg_info)

        # Sort arguments by name
        all_arguments.sort(key=lambda x: x["name"])

        if all_arguments:
            arguments_by_folder.append([folder_name, all_arguments])

    return arguments_by_folder


if __name__ == "__main__":
    result = explore_directory_structure(TOP_LEVEL_FOLDERS)
    cpp_result = filter_cpp_files(result)
    functions_result = parse_cpp_functions(cpp_result)

    # Filter to only keep llk_lib subdirectories
    filtered_functions_result = []
    for folder_name, subdirs in functions_result:
        filtered_subdirs = []
        for subdir_name, files in subdirs:
            if "llk_lib" in subdir_name:
                filtered_subdirs.append([subdir_name, files])
        if filtered_subdirs:
            filtered_functions_result.append([folder_name, filtered_subdirs])

    # Flatten functions by top-level folder with file tagging
    flattened_by_folder = []
    for folder_name, subdirs in filtered_functions_result:
        all_functions = []
        for subdir_name, files in subdirs:
            for file_name, functions in files:
                for func in functions:
                    # Add source file information to each function
                    tagged_func = func.copy()
                    tagged_func["source_file"] = f"{subdir_name}/{file_name}"
                    all_functions.append(tagged_func)
        if all_functions:
            flattened_by_folder.append([folder_name, all_functions])

    # Print detailed hierarchical structure first
    print("=== DETAILED FUNCTIONS BY FOLDER/FILE ===")
    for folder_name, subdirs in filtered_functions_result:
        print(f"\n{folder_name}:")
        for subdir_name, files in subdirs:
            print(f"  {subdir_name}:")
            for file_name, functions in files:
                print(f"    {file_name}: ({len(functions)} functions)")
                for func in functions:
                    # Print function signature
                    template_str = ""
                    if func.get("template_parameters"):
                        template_params = ", ".join(
                            [f"{tp['name']}" for tp in func["template_parameters"]]
                        )
                        template_str = f"template<{template_params}> "

                    param_strs = []
                    for param in func.get("parameters", []):
                        param_str = f"{param['type']} {param['name']}"
                        if param.get("default_value"):
                            param_str += f" = {param['default_value']}"
                        param_strs.append(param_str)
                    params_str = ", ".join(param_strs)

                    print(
                        f"      {template_str}{func['return_type']} {func['name']}({params_str})"
                    )

                    # Print detailed parameter info
                    if func.get("template_parameters"):
                        print(f"        Template Parameters:")
                        for tp in func["template_parameters"]:
                            default_str = (
                                f" = {tp['default_value']}"
                                if tp.get("default_value")
                                else ""
                            )
                            print(f"          {tp['type']} {tp['name']}{default_str}")

                    if func.get("parameters"):
                        print(f"        Runtime Parameters:")
                        for param in func["parameters"]:
                            default_str = (
                                f" = {param['default_value']}"
                                if param.get("default_value")
                                else ""
                            )
                            print(
                                f"          {param['type']} {param['name']}{default_str}"
                            )

    # Print merged function list in Google Sheets CSV format
    print("\n\n=== MERGED FUNCTIONS BY FOLDER (Google Sheets Format) ===")
    print(
        "tt_llk_wormhole_b0,tt_llk_blackhole,tt_llk_quasar,Function Name,Source File,Return Type,Template Parameters,Runtime Parameters"
    )

    # Create a merged view by function name
    function_map = {}

    for folder_name, functions in flattened_by_folder:
        for func in functions:
            func_name = func["name"]
            if func_name not in function_map:
                function_map[func_name] = {
                    "folders": set(),
                    "files": set(),
                    "return_types": set(),
                    "template_params": set(),
                    "runtime_params": set(),
                    "signatures": {},  # Store full signature per folder
                    "files_by_folder": {},  # Store files per folder for conflict detection
                    "signatures_by_file": {},  # Store signatures per file for within-folder conflicts
                }

            # Track which folder this function appears in
            function_map[func_name]["folders"].add(folder_name)
            function_map[func_name]["files"].add(func["source_file"])
            function_map[func_name]["return_types"].add(func["return_type"])

            # Track files by folder
            if folder_name not in function_map[func_name]["files_by_folder"]:
                function_map[func_name]["files_by_folder"][folder_name] = set()
            function_map[func_name]["files_by_folder"][folder_name].add(
                func["source_file"]
            )

            # Format parameters for this function
            template_params_str = "; ".join(
                [f"{tp['name']}" for tp in func.get("template_parameters", [])]
            )
            runtime_params_str = "; ".join(
                [f"{rp['type']} {rp['name']}" for rp in func.get("parameters", [])]
            )

            if template_params_str:
                function_map[func_name]["template_params"].add(template_params_str)
            if runtime_params_str:
                function_map[func_name]["runtime_params"].add(runtime_params_str)

            # Store full signature for conflict detection
            signature = f"{func['return_type']} {func_name}({runtime_params_str})"
            if template_params_str:
                signature = f"template<{template_params_str}> {signature}"
            function_map[func_name]["signatures"][folder_name] = signature

            # Store signature per file for within-folder conflict detection
            file_key = f"{folder_name}::{func['source_file']}"
            function_map[func_name]["signatures_by_file"][file_key] = signature

    # Print merged CSV
    conflicts_files = []
    conflicts_signatures = []

    for func_name in sorted(function_map.keys()):
        func_data = function_map[func_name]

        # Check presence in each folder
        wormhole_present = "✓" if "tt_llk_wormhole_b0" in func_data["folders"] else ""
        blackhole_present = "✓" if "tt_llk_blackhole" in func_data["folders"] else ""
        quasar_present = "✓" if "tt_llk_quasar" in func_data["folders"] else ""

        # Handle file conflicts
        files_str = "; ".join(sorted(func_data["files"]))
        if len(func_data["files"]) > 1:
            conflicts_files.append((func_name, func_data["files_by_folder"]))

        # Handle return type union
        return_types_str = "; ".join(sorted(func_data["return_types"]))

        # Handle parameter unions
        template_params_str = "; ".join(sorted(func_data["template_params"]))
        runtime_params_str = "; ".join(sorted(func_data["runtime_params"]))

        # Check for signature conflicts (across folders and within folders)
        has_conflicts = False

        # Check for conflicts across folders
        if len(func_data["signatures"]) > 1:
            signatures_list = list(func_data["signatures"].values())
            if (
                len(set(signatures_list)) > 1
            ):  # Different signatures exist across folders
                has_conflicts = True

        # Check for conflicts within folders (same function in multiple files with different signatures)
        if len(func_data["signatures_by_file"]) > len(func_data["signatures"]):
            # More file signatures than folder signatures means multiple files per folder
            signatures_by_file_list = list(func_data["signatures_by_file"].values())
            if len(set(signatures_by_file_list)) > len(
                set(func_data["signatures"].values())
            ):
                has_conflicts = True

        if has_conflicts:
            # Combine both folder-level and file-level signatures for comprehensive conflict reporting
            all_signatures = {}
            all_signatures.update(func_data["signatures"])  # Folder-level signatures

            # Add file-level signatures for functions appearing in multiple files within same folder
            for file_key, signature in func_data["signatures_by_file"].items():
                folder_name = file_key.split("::")[0]
                file_path = file_key.split("::")[1]
                # Only add file-level detail if there are multiple files per folder
                folder_file_count = sum(
                    1
                    for fk in func_data["signatures_by_file"].keys()
                    if fk.startswith(f"{folder_name}::")
                )
                if folder_file_count > 1:
                    all_signatures[f"{folder_name} ({file_path})"] = signature

            conflicts_signatures.append((func_name, all_signatures))

        # Escape commas for CSV
        func_name_clean = f'"{func_name}"' if "," in func_name else func_name
        files_clean = f'"{files_str}"' if "," in files_str else files_str
        return_types_clean = (
            f'"{return_types_str}"' if "," in return_types_str else return_types_str
        )
        template_clean = (
            f'"{template_params_str}"'
            if "," in template_params_str
            else template_params_str
        )
        runtime_clean = (
            f'"{runtime_params_str}"'
            if "," in runtime_params_str
            else runtime_params_str
        )

        print(
            f"{wormhole_present},{blackhole_present},{quasar_present},{func_name_clean},{files_clean},{return_types_clean},{template_clean},{runtime_clean}"
        )

    # Print conflict information
    if conflicts_files:
        print("\n\n=== FUNCTIONS IN DIFFERENT FILES ===")
        for func_name, files_by_folder in conflicts_files:
            print(f"{func_name}:")
            for folder, files in files_by_folder.items():
                for file in sorted(files):
                    print(f"  {folder}: {file}")

    if conflicts_signatures:
        print("\n\n=== FUNCTIONS WITH DIFFERENT SIGNATURES ACROSS FOLDERS ===")
        for func_name, signatures in conflicts_signatures:
            print(f"{func_name}:")
            for folder, signature in signatures.items():
                print(f"  {folder}: {signature}")

    # Pivot on arguments
    arguments_by_folder = pivot_on_arguments(flattened_by_folder)

    # Print argument-pivoted results
    print("\n\n=== ARGUMENTS BY FOLDER ===")
    for folder_name, arguments in arguments_by_folder:
        print(f"\n{folder_name}: ({len(arguments)} arguments)")
        for arg in arguments:
            default_str = f" = {arg['default_value']}" if arg["default_value"] else ""
            print(
                f"  {arg['type']} {arg['name']}{default_str} ({arg['parameter_category']})"
            )
            print(
                f"    -> {arg['function_return_type']} {arg['source_function']}() in {arg['source_file']}"
            )
