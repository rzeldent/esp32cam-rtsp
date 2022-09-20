. python3 -m pip install --upgrade pip setuptools wheel
. python3 -m pip install htmlmin

. python3 ./html_to_cpp.py ./html ./include/html_data.h
. python3 ./html_to_cpp_gzip.py ./html_gzip ./include/html_data_gzip.h
