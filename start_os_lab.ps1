# 操作系统实验 - WSL 快速启动脚本
# 使用方法: 在 PowerShell 中运行 .\start_os_lab.ps1

Write-Host "==================================="
Write-Host "启动操作系统实验环境"
Write-Host "==================================="
Write-Host ""

# 获取当前脚本所在目录
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path

# 转换为 WSL 路径格式 (D:\path\to\dir -> /mnt/d/path/to/dir)
$wslPath = $scriptPath -replace '^([A-Z]):', {'/mnt/' + $_.Value[0].ToString().ToLower()} -replace '\\', '/'

Write-Host "实验目录: $scriptPath"
Write-Host "WSL 路径: $wslPath"
Write-Host ""

# 启动 WSL 并自动执行环境配置
Write-Host "正在启动 WSL 并配置环境..."
Write-Host ""

$commands = @(
    "cd `"$wslPath`"",
    "source setup_os_env.sh",
    "bash"
) -join "; "

# 使用 Ubuntu-22.04_New (根据你的 WSL 发行版调整)
wsl -d Ubuntu-22.04_New bash -c $commands
