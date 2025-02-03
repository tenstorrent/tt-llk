import sys

def check_strings_in_file(strings, file):
    try:
        with open(file, 'r') as f:
            content = f.read()

        for string in strings:
            if string in content:
                return string.lower()
        return "Unsupported architecture."
    
    except FileNotFoundError:
        print(f"Error: The file {file} does not exist.")
        return "not found"

if __name__ == "__main__":
    strings = sys.argv[1:-1]
    file = sys.argv[-1]

    result = check_strings_in_file(strings, file)
    print(result)
