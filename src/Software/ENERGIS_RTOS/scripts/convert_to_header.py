import sys
import re
import os

def minify_html(html):
    """Minifies HTML by removing extra whitespace, comments, and line breaks."""
    html = re.sub(r"\s+", " ", html)  # Remove extra spaces
    html = re.sub(r"<!--.*?-->", "", html, flags=re.DOTALL)  # Remove comments
    html = re.sub(r"\s*>\s*", ">", html)  # Remove spaces before/after tags
    html = re.sub(r"\s*<\s*", "<", html)  # Remove spaces before/after tags
    return html.strip()  # Remove leading/trailing spaces

def convert_to_header(input_file):
    output_file = input_file.replace(".html", ".h")
    variable_name = os.path.splitext(os.path.basename(input_file))[0] + "_html"
    header_guard = os.path.splitext(os.path.basename(input_file))[0].upper() + "_HTML_H"

    try:
        with open(input_file, "r", encoding="utf-8") as f:
            content = f.read()

        # Minify the HTML content
        content = minify_html(content)

        # Escape double quotes and split into C string lines
        content = content.replace('"', '\\"')
        lines = content.splitlines()
        formatted_content = '\n'.join([f'    "{line}\\n"' for line in lines])

        # Create C header file format
        header = f"""// Auto-generated C header file from HTML
#ifndef {header_guard}
#define {header_guard}

const char {variable_name}[] = 
{formatted_content}
;

#endif // {header_guard}
"""

        with open(output_file, "w", encoding="utf-8") as f:
            f.write(header)

        print(f"✅ Conversion complete: {output_file}")

    except Exception as e:
        print(f"❌ Error: {e}")

# Run script with command-line argument
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python convert_to_header.py <input_file.html>")
    else:
        convert_to_header(sys.argv[1])
