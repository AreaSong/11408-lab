#!/bin/bash
# 新建一次尝试：在关卡的 mine/ 下创建「日期-说明」目录，并在 NOTES.md 追加对应小节。
#
# 用法：
#   ./new.sh                           交互模式：依次选择科目、关卡、填写说明
#   ./new.sh <科目> <关卡号> [说明]
#   ./new.sh lib 02                    关卡已存在：说明缺省为 first（第一次）或 review（之后）
#   ./new.sh lib 02 recursive          指定说明
#   ./new.sh lib 02-linked-list        关卡还不存在：给出完整目录名，先建关卡骨架
#
# 科目：lib / co / os / net；说明和目录名只能用小写字母、数字和连字符（会成为 CLion 里的程序名）。

set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
subjects=(lib co os net)
subject_names=(数据结构 计组 操作系统 计网)

die() { echo "错误：$*" >&2; exit 1; }

is_slug() { [[ "$1" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]]; }

check_subject() {
    case "$1" in
        lib|co|os|net) ;;
        *) die "科目只能是 lib、co、os、net，收到的是：$1" ;;
    esac
}

# 输出 subject 下以 num 开头的已有关卡目录（可能为空）
find_level_dir() {
    local d
    for d in "$root/$1/$2"-*; do
        [ -d "$d" ] && echo "$d"
    done
    return 0
}

count_attempts() {
    local mine="$1/mine"
    [ -d "$mine" ] || { echo 0; return; }
    find "$mine" -mindepth 1 -maxdepth 1 -type d | wc -l | tr -d ' '
}

default_desc() {
    if [ "$(count_attempts "$1")" -eq 0 ]; then echo first; else echo review; fi
}

notes_header() {
    printf '# %s/%s 复盘\n\n## 概念卡片\n\n' "$1" "$2"
}

# 根据科目和关卡参数（NN 或 NN-name）确定 level_dir，不存在时建骨架
resolve_level() {
    local subject="$1" level_arg="$2" num existing
    case "$level_arg" in
        [0-9][0-9]) ;;
        [0-9][0-9]-*) is_slug "${level_arg:3}" || die "关卡目录名只能用小写字母、数字和连字符：$level_arg" ;;
        *) die "关卡号要写成两位数字（如 02），或完整目录名（如 02-linked-list）" ;;
    esac
    num="${level_arg:0:2}"
    existing="$(find_level_dir "$subject" "$num")"
    if [ -n "$existing" ]; then
        [ "$(echo "$existing" | wc -l)" -eq 1 ] || die "$subject 下有多个 $num 开头的关卡目录，请先清理"
        if [ "$level_arg" != "$num" ] && [ "$(basename "$existing")" != "$level_arg" ]; then
            die "$subject 下已有关卡 $(basename "$existing")，和你给的 $level_arg 不一致"
        fi
        level_dir="$existing"
        return
    fi
    [ "$level_arg" != "$num" ] || die "$subject/$num 还不存在，第一次请给完整目录名，例如 ./new.sh $subject $num-xxx"
    level_dir="$root/$subject/$level_arg"
    mkdir -p "$level_dir/mine" "$level_dir/tests"
    notes_header "$subject" "$level_arg" > "$level_dir/NOTES.md"
    echo "已创建关卡骨架：$subject/$level_arg/"
}

# ---------- 交互模式 ----------

ask() {
    local prompt="$1" default="${2:-}" reply
    if [ -n "$default" ]; then
        read -r -p "$prompt [$default]：" reply || die "输入已结束，已退出"
        echo "${reply:-$default}"
    else
        read -r -p "${prompt}：" reply || die "输入已结束，已退出"
        echo "$reply"
    fi
}

pick_subject() {
    local i choice
    echo "选择科目："
    for i in 0 1 2 3; do
        echo "  $((i + 1))) ${subjects[$i]}  ${subject_names[$i]}"
    done
    while true; do
        choice="$(ask "输入编号 1-4")"
        case "$choice" in
            [1-4]) subject="${subjects[$((choice - 1))]}"; return ;;
            *) echo "请输入 1 到 4" ;;
        esac
    done
}

# 从 LEVELS.md 的关卡总览表读出「编号 主题」，并标注已建情况
list_levels() {
    local levels_md="$root/$subject/LEVELS.md" num topic dir status
    [ -f "$levels_md" ] || return 0
    echo
    echo "$subject 的关卡（来自 LEVELS.md）："
    grep -E '^\| [0-9]{2} \|' "$levels_md" | awk -F'|' '!seen[$2]++ {gsub(/^ +| +$/, "", $2); gsub(/^ +| +$/, "", $3); print $2 "\t" $3}' |
    while IFS=$'\t' read -r num topic; do
        dir="$(find_level_dir "$subject" "$num" | head -1)"
        if [ -n "$dir" ]; then
            status="已建 $(basename "$dir")，尝试 $(count_attempts "$dir") 次"
        else
            status="未建"
        fi
        printf '  %s  %s  [%s]\n' "$num" "$topic" "$status"
    done
}

pick_level() {
    local num name existing
    list_levels
    while true; do
        num="$(ask "输入两位关卡号，例如 02")"
        [[ "$num" =~ ^[0-9]{2}$ ]] && break
        echo "关卡号要写成两位数字"
    done
    existing="$(find_level_dir "$subject" "$num" | head -1)"
    if [ -n "$existing" ]; then
        level_arg="$num"
        return
    fi
    echo "这一关还没建，需要起一个英文目录名（小写字母、数字、连字符）。"
    while true; do
        name="$(ask "英文目录名，例如 linked-list")"
        is_slug "$name" && break
        echo "「${name}」不行：只能用小写字母、数字和连字符，不能为空、不能有空格或中文"
    done
    level_arg="$num-$name"
}

pick_desc() {
    local dir default
    dir="$(find_level_dir "$subject" "${level_arg:0:2}" | head -1)"
    default=first
    if [ -n "$dir" ]; then
        default="$(default_desc "$dir")"
        if [ "$(count_attempts "$dir")" -gt 0 ]; then
            echo "已有的尝试："
            find "$dir/mine" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort | sed 's/^/  - /'
        fi
    fi
    while true; do
        desc="$(ask "这次尝试的说明（如 review、recursive）" "$default")"
        is_slug "$desc" && break
        echo "「${desc}」不行：只能用小写字母、数字和连字符，不能有空格或中文"
    done
}

interactive() {
    local confirm
    pick_subject
    pick_level
    pick_desc
    echo
    confirm="$(ask "将创建 $subject/$level_arg 下的 mine/$(date +%F)-${desc}，确认？(y/n)" "y")"
    case "$confirm" in
        y|Y|yes) ;;
        *) echo "已取消"; exit 0 ;;
    esac
}

# ---------- 创建尝试 ----------

create_attempt() {
    local attempt attempt_dir rel f
    mkdir -p "$level_dir/mine"
    [ -n "$desc" ] || desc="$(default_desc "$level_dir")"
    is_slug "$desc" || die "说明只能用小写字母、数字和连字符，例如 recursive、two-pointer"

    attempt="$(date +%F)-$desc"
    attempt_dir="$level_dir/mine/$attempt"
    [ ! -e "$attempt_dir" ] || die "今天已经有一次同名尝试：mine/${attempt}，换个说明再试"
    mkdir -p "$attempt_dir"

    # 有标准答案时，按答案的文件名建同名空文件，方便对照着写
    if [ -d "$level_dir/answer" ]; then
        (cd "$level_dir/answer" && find . -type f \( -name '*.c' -o -name '*.h' \)) | while read -r f; do
            mkdir -p "$attempt_dir/$(dirname "$f")"
            : > "$attempt_dir/$f"
        done
    fi

    [ -f "$level_dir/NOTES.md" ] || notes_header "$subject" "$(basename "$level_dir")" > "$level_dir/NOTES.md"
    printf '\n## %s\n- \n' "$attempt" >> "$level_dir/NOTES.md"

    rel="${attempt_dir#"$root"/}"
    echo "已创建：$rel/"
    if [ -n "$(ls -A "$attempt_dir")" ]; then
        (cd "$attempt_dir" && find . -type f | sed 's|^\./|  - |')
    fi
    echo "已在 NOTES.md 末尾添加小节：## $attempt"
    echo "在 CLion 里重新加载 CMake 项目后，就能在运行目标里看到这次尝试。"
}

main() {
    subject="" level_arg="" desc="" level_dir=""
    if [ $# -eq 0 ]; then
        interactive
    elif [ $# -ge 2 ] && [ $# -le 3 ]; then
        subject="$1" level_arg="$2" desc="${3:-}"
    else
        die "用法：./new.sh（交互）或 ./new.sh <科目> <关卡号> [说明]，例如 ./new.sh lib 02 review"
    fi
    check_subject "$subject"
    resolve_level "$subject" "$level_arg"
    create_attempt
}

main "$@"
