echo "Testing I/O redirection"
echo "First line" > test1.txt
echo "Second line" >> test1.txt
cat test1.txt
cat < test1.txt > test2.txt
cat test2.txt
ls -l > filelist.txt
cat filelist.txt
exit 0
