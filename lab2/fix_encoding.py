#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""修复Markdown文件中的乱码"""

import re

# 读取文件
input_file = r'd:\gds\Documents\Operating_system\labcode\lab2\SLUB扩展练习总结.md'

with open(input_file, 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# 定义乱码到正确字符的映射
replacements = {
    '�': '',  # 删除所有乱码字符
}

# 特定的乱码修复规则
patterns = [
    (r'�\?', '：'),
    (r'�\*', '**'),
    (r'�\?', '的'),
    (r'�\?', '在'),
    (r'�\?', '）'),
    (r'�\?', '（'),
    (r'�\?', '层'),
    (r'�\?', '路径'),
    (r'�\?', '少'),
    (r'�\?', '用'),
    (r'�\?', '态'),
    (r'�\?', '费'),
    (r'�\?', '略'),
    (r'�\?', '入'),
    (r'�\?', '应'),
    (r'�\?', '单'),
    (r'�\?', '性'),
    (r'�\?', '试'),
    (r'�\?', '器'),
    (r'�\?', '义'),
    (r'�\?', '展'),
    (r'�\?', '点'),
    (r'�\?', '化'),
    (r'�\?', '证'),
    (r'�\?', '析'),
    (r'�\?', '↓'),
]

# 应用替换
for pattern, replacement in patterns:
    content = re.sub(pattern, replacement, content)

# 删除所有剩余的乱码字符
content = content.replace('�', '')

# 保存修复后的文件
with open(input_file, 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print("文件修复完成！")
