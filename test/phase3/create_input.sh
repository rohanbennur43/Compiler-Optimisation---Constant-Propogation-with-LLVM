filepath=${1%.*}		# Remove extension if present
/usr/bin/clang -Xclang -disable-O0-optnone -fno-discard-value-names -O0 -S -emit-llvm $filepath.c -o $filepath.ll
/usr/bin/opt -mem2reg -S $filepath.ll -o $filepath.ll
