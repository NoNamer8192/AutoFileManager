# 文件监控程序使用说明

## 功能简介
实时监控指定文件/文件夹的大小，在超过预设阈值时执行相应操作，系统资源占用极低。

## 使用方法
1. **修改监控列表**：编辑 `StatList.tsv` 文件，按格式添加监控条目
2. **启动监控程序**：运行主程序（如 `FileMonitor.exe`）

## TSV文件格式说明

### 文件结构
- **第一行**：必须为 `file	size	execute	type`（制表符分隔）
- **后续行**：每行一个监控任务，参数间用`Tab`分隔

### 参数说明
| 参数名 | 说明 |
|--------|------|
| file | 要监控的文件/文件夹完整路径 |
| size | 大小阈值（支持单位：B/KB/MB/GB/TB，不区分大小写，支持缩写） |
| execute | 操作类型：`warn`(警告) / `trash`(清空) |
| type | 监控类型：`file`(文件) / `path`(文件夹) |

## 配置示例

### 典型应用场景
路径：`C:\Users\<用户名>\AppData\Local\Temp	500MB	trash	path`
功能：当Temp文件夹达到500MB时自动清空内容（保留文件夹结构）

### 可直接使用的StatList.tsv内容
已经置于StatList.tsv，进行修改即可

# File Monitor Program README

## Overview
Monitors specified files/folders in real-time with minimal system resource usage, and performs predefined actions when size thresholds are exceeded.

## Usage
1. **Modify Monitoring List**: Edit the `StatList.tsv` file to add monitoring entries according to the format
2. **Start Monitoring Program**: Run the main program (e.g., `FileMonitor.exe`)

## TSV File Format

### File Structure
- **First line**: Must be `file	size	execute	type` (tab-separated)
- **Subsequent lines**: Each line represents one monitoring task, with parameters separated by **tabs**

### Parameter Description
| Parameter | Description |
|-----------|-------------|
| file | Full path to the file/folder to monitor |
| size | Size threshold (supports units: B/KB/MB/GB/TB, case-insensitive) |
| execute | Action type: `warn`(warning) / `trash`(clear contents) |
| type | Target type: `file`(file) / `path`(folder/drive) |

## Configuration Examples

### Typical Use Case
Path: `C:\Users\<Username>\AppData\Local\Temp`	500MB	trash	path
Function: Automatically clears contents when Temp folder reaches 500MB (preserves folder structure)

### Ready-to-use StatList.tsv Content:
