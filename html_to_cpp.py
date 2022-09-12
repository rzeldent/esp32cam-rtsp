# pip install Pillow

import os
import sys
import htmlmin

if (len(sys.argv) <= 2):
    print('Usage: html_to_cpp.py <input_dir> <file.h>')
    sys.exit(1)

input_dir = sys.argv[1]
file_h = sys.argv[2]

file_names = os.listdir(input_dir)
file_names = filter(lambda x: x[0] != '.' and os.path.isfile(os.path.join(input_dir, x)), file_names)
file_names = sorted(file_names)

output_file = open(file_h, 'w')
output_file.write('//*******************************************************************************\n')
output_file.write('// HTML import\n')
output_file.write('// Machine generated file\n')
output_file.write('// ******************************************************************************\n')
output_file.write('\n')

for file_name in file_names:
    print(f'Processing: {file_name}... ')

    file_path = os.path.join(input_dir, file_name)
    file_data_name = f'file_data_{file_name}'.replace('.', '_')

    file = open(file_path, 'r')
    html = file.read()
    file.close()

    html_mimified = htmlmin.minify(html, remove_empty_space=True)

    output_file.write('\n')
    output_file.write('constexpr char ' + file_data_name + '[] = "')
    # escape "
    html_mimified_escaped = html_mimified.replace('"', '\\"')
    output_file.write(html_mimified_escaped)
    output_file.write('";\n')

output_file.close()

print('Done.')