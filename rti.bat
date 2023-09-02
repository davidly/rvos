echo running t... > test_rvos.txt
rvos /h:40 rvos.elf tests\t >>test_rvos.txt
echo runing ts... >> test_rvos.txt
rvos /h:40 rvos.elf tests\ts >>test_rvos.txt
echo running sieve... >> test_rvos.txt
rvos /h:40 rvos.elf tests\sieve >>test_rvos.txt
rvos /h:40 rvos.elf tests\e >>test_rvos.txt
rvos /h:40 rvos.elf tests\tap >>test_rvos.txt
rvos /h:40 rvos.elf tests\tpi >>test_rvos.txt
rvos /h:40 rvos.elf tests\tins >>test_rvos.txt
rvos /h:40 rvos.elf tests\tp >>test_rvos.txt
rvos /h:40 rvos.elf tests\tphi >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash ml >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash mh >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash pcl >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash pch >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash spl >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash sph >>test_rvos.txt
rvos /h:40 rvos.elf tests\tcrash spm >>test_rvos.txt
rvos /h:40 rvos.elf tests\ttt >>test_rvos.txt
rvos /h:40 rvos.elf tests\tf >>test_rvos.txt
rvos /h:40 rvos.elf tests\tm >>test_rvos.txt
rvos /h:40 rvos.elf tests\ba tests\tp.bas >>test_rvos.txt
rvos /h:40 rvos.elf tests\ttime >>test_rvos.txt
rvos /h:40 rvos.elf tests\td >>test_rvos.txt
rvos /h:40 rvos.elf /h:2 tests\mysort /q /u tests\words.txt tests\sorted.txt >>test_rvos.txt
rvos /h:60 rvos.elf /h:40 tests\an david lee >>test_rvos.txt

diff baseline_test_rvos.txt test_rvos.txt
