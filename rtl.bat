echo running t... > test_rvos.txt
rvos linux\t >>test_rvos.txt
echo runing glob... >> test_rvos.txt
rvos linux\glob >>test_rvos.txt
echo runing ts... >> test_rvos.txt
rvos linux\ts >>test_rvos.txt
echo running sieve... >> test_rvos.txt
rvos linux\sieve >>test_rvos.txt
rvos linux\e >>test_rvos.txt
rvos linux\tap >>test_rvos.txt
rvos linux\tpi >>test_rvos.txt
rvos linux\tphi >>test_rvos.txt
rvos linux\tins >>test_rvos.txt
rvos linux\terrno >>test_rvos.txt
rvos linux\tp1k >>test_rvos.txt
rvos linux\tdir >>test_rvos.txt
rvos linux\tcrash ml >>test_rvos.txt
rvos linux\tcrash mh >>test_rvos.txt
rvos linux\tcrash pcl >>test_rvos.txt
rvos linux\tcrash pch >>test_rvos.txt
rvos linux\tcrash spl >>test_rvos.txt
rvos linux\tcrash sph >>test_rvos.txt
rvos linux\tcrash spm >>test_rvos.txt
rvos linux\ttt >>test_rvos.txt
rvos linux\tf >>test_rvos.txt
rvos linux\tm >>test_rvos.txt
rvos linux\ba linux\tp.bas >>test_rvos.txt
rvos linux\ttime >>test_rvos.txt
rvos linux\td >>test_rvos.txt
rvos /h:2 linux\mysort /q /u linux\words.txt linux\sorted.txt >>test_rvos.txt
rvos /h:40 linux\an david lee >>test_rvos.txt

diff baseline_test_rvos.txt test_rvos.txt
