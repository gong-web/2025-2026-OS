# -*- coding: utf-8 -*-
import re

file_path = r'd:\gds\Documents\Operating_system\labcode\lab2\SLUB扩展练习总结.md'

with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 定义替换规则 (问号后面跟着的字符模式 -> 正确的字符)
replacements = {
    r'slub_caches\[8\] \?': 'slub_caches[8] →',
    r'uCore \?slab': 'uCore 的 slab',
    r'尺寸\?/32': '尺寸，8/32',
    r'稳定\?': '稳定。',
    r'函数\?\*\?': '函数族**：',
    r'复\?': '复用 ',
    r'统计\?': '统计。',
    r'快速路\?- \?': '快速路径 - 从 ',
    r'慢速路\?- ': '慢速路径 - ',
    r'流程\?\*\?': '流程图**：',
    r'非空\?─Yes\?': '非空？ ─Yes→ ',
    r'空闲\?─Yes\?': '空闲？ ─Yes→ ',
    r' \?返回对象': ' → 返回对象',
    r'─Yes\?初始\?': '─Yes→ 初始化 ',
    r'优化\?\*\?': '优化点**：',
    r'加\?\*\?': '加速**：9',
    r'\(O\(1\)\)\?': '（O(1)）',
    r'转\?\*：满\?': '转换**：满载 ',
    r'自动\?partial': '自动从 partial',
    r'遍\?': '遍历',
    r'处\?- CPU \?': '处理 - CPU 页 ',
    r'维\?=': '维护 =',
    r'空闲 \?释放\?': '空闲 → 释放回 ',
    r'使\?\?': '使用 → ',
    r'部分使\?': '部分使用',
    r'加\?partial': '加入 partial',
}

# 应用所有替换
for pattern, replacement in replacements.items():
    content = re.sub(pattern, replacement, content)

# 保存文件
with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print("修复完成！")
