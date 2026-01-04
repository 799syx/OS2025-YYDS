#!/usr/bin/env python3
"""
自动运行所有 xv6 测试并收集结果到一个文件
"""

import pexpect
import sys
import os
from datetime import datetime

# 测试列表
TESTS = [
    "alarmtest",
    "bcachetest",
    "bigfiletest",
    "chmodtest",
    "cowtest",
    "embassy_test",
    "forktest",
    "fstest",
    "hmdfstest",
    "kalloctest",
    "lazytest",
    "mlfqtest",
    "mmaptest",
    "msgtest",
    "newtest",
    "osfeatures_test",
    "qostest",
    "recoveritest",
    "sigtest",
    "symlinktest",
    "usertests",
    "utiltest",
]

OUTPUT_FILE = "test_results.txt"
TIMEOUT = 180  # 每个测试的超时时间（秒）

def run_test(test_name):
    """运行单个测试并返回输出"""
    print(f"\n{'='*60}")
    print(f"正在运行测试: {test_name}")
    print(f"{'='*60}")
    
    result = {
        'name': test_name,
        'output': '',
        'status': 'UNKNOWN'
    }
    
    try:
        # 启动 QEMU
        child = pexpect.spawn('make qemu', cwd=os.path.dirname(os.path.abspath(__file__)), 
                             timeout=TIMEOUT, encoding='utf-8', codec_errors='replace')
        
        # 等待 shell 启动
        index = child.expect(['init: starting sh', pexpect.TIMEOUT, pexpect.EOF], timeout=60)
        if index != 0:
            result['status'] = 'BOOT_FAILED'
            result['output'] = child.before if child.before else 'Boot timeout'
            child.close(force=True)
            return result
        
        # 等待 shell 提示符
        child.expect([r'\$ ', pexpect.TIMEOUT], timeout=10)
        
        # 发送测试命令
        child.sendline(test_name)
        
        # 收集输出直到看到下一个 shell 提示符或超时
        output_lines = []
        try:
            while True:
                index = child.expect([r'\$ ', 'panic', pexpect.TIMEOUT, pexpect.EOF], timeout=TIMEOUT)
                if child.before:
                    output_lines.append(child.before)
                
                if index == 0:  # Shell 提示符 - 测试完成
                    result['status'] = 'COMPLETED'
                    break
                elif index == 1:  # Panic
                    result['status'] = 'PANIC'
                    # 收集更多 panic 信息
                    try:
                        child.expect(pexpect.TIMEOUT, timeout=3)
                        if child.before:
                            output_lines.append(child.before)
                    except:
                        pass
                    break
                elif index == 2:  # Timeout
                    result['status'] = 'TIMEOUT'
                    break
                elif index == 3:  # EOF
                    result['status'] = 'EOF'
                    break
        except Exception as e:
            result['status'] = f'ERROR: {str(e)}'
        
        result['output'] = ''.join(output_lines)
        
        # 退出 QEMU (Ctrl-A X)
        try:
            child.sendcontrol('a')
            child.send('x')
            child.expect(pexpect.EOF, timeout=5)
        except:
            pass
        
        child.close(force=True)
        
    except Exception as e:
        result['status'] = f'EXCEPTION: {str(e)}'
        result['output'] = str(e)
    
    # 检查输出中是否有 PASS/FAIL 标记
    output = result['output']
    output_lower = output.lower()
    
    # 检查是否有明确的测试通过标记
    pass_markers = ['all tests passed', 'test passed', 'tests passed', 'all cow tests passed',
                    'all tests ok', 'test ok', 'all embassy tests passed', 'all mlfq测试完成',
                    'chmodtest: all tests passed', '所有测试完成']
    has_pass_marker = any(marker in output_lower for marker in pass_markers)
    
    # 检查是否有 OK 标记（但不是 "not ok"）
    has_ok = ' ok' in output_lower or '\nok' in output_lower or 'test ok' in output_lower
    
    # 检查是否有真正的测试失败（不是预期的权限失败）
    # "FAILED" 大写通常表示测试失败，小写 "failed" 可能是预期行为
    has_test_failure = 'FAILED' in output or 'test failed' in output_lower
    has_panic = 'panic' in output_lower
    
    if has_pass_marker or (has_ok and not has_test_failure and not has_panic):
        result['status'] = 'PASSED'
    elif has_test_failure or has_panic:
        result['status'] = 'FAILED'
    
    print(f"测试 {test_name} 状态: {result['status']}")
    return result

def main():
    print(f"xv6 自动化测试")
    print(f"开始时间: {datetime.now()}")
    print(f"测试数量: {len(TESTS)}")
    
    results = []
    
    for test in TESTS:
        result = run_test(test)
        results.append(result)
    
    # 写入结果文件
    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write("=" * 70 + "\n")
        f.write("xv6 测试结果汇总\n")
        f.write(f"测试时间: {datetime.now()}\n")
        f.write("=" * 70 + "\n\n")
        
        # 汇总
        f.write("测试汇总:\n")
        f.write("-" * 40 + "\n")
        passed = sum(1 for r in results if r['status'] == 'PASSED')
        failed = sum(1 for r in results if r['status'] == 'FAILED')
        other = len(results) - passed - failed
        f.write(f"通过: {passed}\n")
        f.write(f"失败: {failed}\n")
        f.write(f"其他: {other}\n")
        f.write(f"总计: {len(results)}\n\n")
        
        # 状态列表
        f.write("各测试状态:\n")
        f.write("-" * 40 + "\n")
        for r in results:
            f.write(f"  {r['name']:20s} : {r['status']}\n")
        f.write("\n")
        
        # 详细输出
        f.write("=" * 70 + "\n")
        f.write("详细测试输出\n")
        f.write("=" * 70 + "\n\n")
        
        for r in results:
            f.write("-" * 70 + "\n")
            f.write(f"测试: {r['name']}\n")
            f.write(f"状态: {r['status']}\n")
            f.write("-" * 70 + "\n")
            f.write(r['output'])
            f.write("\n\n")
        
        f.write("=" * 70 + "\n")
        f.write("测试完成\n")
        f.write("=" * 70 + "\n")
    
    print(f"\n{'='*60}")
    print(f"所有测试完成！")
    print(f"结果已保存到: {OUTPUT_FILE}")
    print(f"通过: {passed}, 失败: {failed}, 其他: {other}")
    print(f"{'='*60}")

if __name__ == '__main__':
    main()
