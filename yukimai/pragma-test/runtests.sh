 #!/bin/bash
 
 RVCC=~/d/rvi/bin/riscv64-unknown-elf-gcc
 
 for fsrc in exp/*.c
 do
    testcase=$(basename -- "$fsrc" .c)
    echo "Running $testcase"
    $RVCC -S -o "$testcase.exp.S" "exp/$testcase.c"
    if [ -f "val/$testcase.c" ]; then
        $RVCC -S -o "$testcase.val.S" "val/$testcase.c"
        diff "$testcase.exp.S" "$testcase.val.S"
    fi
done
