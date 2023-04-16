. python3 -m pip install --upgrade pip setuptools wheel
. python3 -m pip install minify-html

. python3 ./minify.py ./html/index.html ./html/index.min.html
. python3 ./minify.py ./html/restart.html ./html/restart.min.html