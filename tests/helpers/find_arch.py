import sys

def check_strings_in_file(strings, file_path):
    try:
        with open(file, 'r') as f:
            content = f.read()

        for string in strings:
            if string in content:
                return string.lower()
        return "Unsupported architecture."
    
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' does not exist.", file=sys.stderr)
        return None


if __name__ == "__main__":
    strings = sys.argv[1:-1]
    file_path = sys.argv[-1]

    result = check_strings_in_file(strings,file_path)

    if result is not None:
        print(result)
