echo running t... > test_rvos.txt
rvos tests\t >>test_rvos.txt
echo runing ts... >> test_rvos.txt
rvos tests\ts >>test_rvos.txt
echo running sieve... >> test_rvos.txt
rvos tests\sieve >>test_rvos.txt
rvos tests\e >>test_rvos.txt
rvos tests\tap >>test_rvos.txt
rvos tests\tpi >>test_rvos.txt
rvos tests\tphi >>test_rvos.txt
rvos tests\tins >>test_rvos.txt
rvos tests\tcrash ml >>test_rvos.txt
rvos tests\tcrash mh >>test_rvos.txt
rvos tests\tcrash pcl >>test_rvos.txt
rvos tests\tcrash pch >>test_rvos.txt
rvos tests\tcrash spl >>test_rvos.txt
rvos tests\tcrash sph >>test_rvos.txt
rvos tests\tcrash spm >>test_rvos.txt
rvos tests\ttt >>test_rvos.txt
rvos tests\tf >>test_rvos.txt
rvos tests\ba tests\tp.bas >>test_rvos.txt
rvos tests\ttime >>test_rvos.txt
rvos tests\td >>test_rvos.txt
rvos /h:2 tests\mysort /q /u tests\words.txt tests\sorted.txt >>test_rvos.txt
rvos /h:40 tests\an david lee >>test_rvos.txt

diff baseline_test_rvos.txt test_rvos.txt
