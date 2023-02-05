#!/usr/bin/python3

import os
import sys
import htmlmin
import gzip

if (len(sys.argv) <= 2):
    print('Usage: bin_to_cpp_gzip.py <input_dir> <file.h>')
    sys.exit(1)

input_dir = sys.argv[1]
file_h = sys.argv[2]

file_names = os.listdir(input_dir)
file_names = filter(lambda x: x[0] != '.' and 
os.path.isfile(os.path.join(input_dir, x)), file_names)
file_names = sorted(file_names)

output_file = open(file_h, 'w')
output_file.write(
    '//*******************************************************************************\n'
    '// HTML import gzipped\n'
    '// Machine generated file\n'
    '// ******************************************************************************\n'
    '\n\n')

for file_name in file_names:
    print(f'Processing: {file_name}... ')

    file_path = os.path.join(input_dir, file_name)
    file_data_name = f'file_data_{file_name}'.replace('.', '_')

    file = open(file_path, 'r')
    html = file.read()
    file.close()

    html_mimified = htmlmin.minify(html, remove_empty_space=True)
    html_mimified_gzip = gzip.compress(bytes(html_mimified, 'utf-8'))
    html_mimified_gzip_values = ','.join(f'0x{i:02x}' for i in html_mimified_gzip)

    output_file.write(f'constexpr unsigned char {file_data_name}[] = {{\n{html_mimified_gzip_values}\n}};\n\n')

output_file.close()

print('Done.')
