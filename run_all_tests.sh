#!/bin/bash

# 自动运行所有 xv6 测试并收集结果
# 输出文件
OUTPUT_FILE="test_results.txt"

# 测试列表（从 Makefile 中提取的测试程序）
TESTS=(
    "alarmtest"
    "bcachetest"
    "bigfiletest"
    "chmodtest"
    "cowtest"
    "embassy_test"
    "forktest"
    "fstest"
    "hmdfstest"
    "kalloctest"
    "lazytest"
    "mlfqtest"
    "mmaptest"
    "msgtest"
    "newtest"
    "osfeatures_test"
    "qostest"
    "recoveritest"
    "sigtest"
    "symlinktest"
    "usertests"
    "utiltest"
)

# 清空输出文件
echo "========================================" > "$OUTPUT_FILE"
echo "xv6 测试结果汇总" >> "$OUTPUT_FILE"
echo "测试时间: $(date)" >> "$OUTPUT_FILE"
echo "========================================" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 超时时间（秒）
TIMEOUT=120

run_single_test() {
    local test_name=$1
    echo "正在运行测试: $test_name"
    echo "" >> "$OUTPUT_FILE"
    echo "----------------------------------------" >> "$OUTPUT_FILE"
    echo "测试: $test_name" >> "$OUTPUT_FILE"
    echo "----------------------------------------" >> "$OUTPUT_FILE"
    
    # 使用 expect 运行 QEMU 并执行测试
    timeout $TIMEOUT expect -c "
        set timeout $TIMEOUT
        spawn make qemu
        expect {
            \"init: starting sh\" {
                send \"$test_name\r\"
            }
            timeout {
                send_user \"启动超时\n\"
                exit 1
            }
        }
        # 等待测试完成
        expect {
            -re \"\\$\" {
                # 测试完成，返回 shell 提示符
            }
            \"panic\" {
                send_user \"内核 panic\n\"
            }
            timeout {
                send_user \"测试超时\n\"
            }
        }
        # 等待一小段时间收集输出
        sleep 2
        send \"\x01x\"
        expect eof
    " 2>&1 | tee -a "$OUTPUT_FILE"
    
    echo "" >> "$OUTPUT_FILE"
}

# 运行所有测试
for test in "${TESTS[@]}"; do
    run_single_test "$test"
done

echo "" >> "$OUTPUT_FILE"
echo "========================================" >> "$OUTPUT_FILE"
echo "所有测试完成" >> "$OUTPUT_FILE"
echo "========================================" >> "$OUTPUT_FILE"

echo ""
echo "测试完成！结果已保存到 $OUTPUT_FILE"
